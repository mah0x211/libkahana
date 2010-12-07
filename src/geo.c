/* 
**
**  Modify : 4/16/2010
**  (C) Masatoshi Teruya
**
*/
#include "kahana_geo.h"
#include "kahana_private.h"

static const char BASE32[] = "0123456789bcdefghjkmnpqrstuvwxyz";
// 軌道長半径
static const double WGS84_major = 6378137.0;
// 軌道短半径
static const double WGS84_minor = 6356752.314245;
// 離心率
static const double WGS84_eccentricity = ( ( ( 6378137.0 * 6378137.0 ) - ( 6356752.314245 * 6356752.314245 ) ) / ( 6378137.0 * 6378137.0 ) );
// 角度
static const double	degree = 180/M_PI;
// 弧度
static const double rad = M_PI/180;

#pragma mark INIT AND FREE
apr_status_t kahanaGeoCreate( kGeo_t **newgeo, apr_pool_t *p, double lat, double lon, size_t precision, bool isWGS84 )
{
	kGeo_t *geo = NULL;
	apr_pool_t *sp = NULL;
	apr_status_t rc;
	
	if( ( rc = kahanaMalloc( p, sizeof( kGeo_t ), (void**)&geo, &sp ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", kahanaLogErr2Str( ETYPE_APR, rc ) );
	}
	else
	{
		geo->p = sp;
		geo->lat = lat;
		geo->lon = lon;
		geo->precision = precision;
		*newgeo = geo;
		
		if( isWGS84 ){
			kahanaGeoSetLatLon( geo, lat, lon, precision, isWGS84 );
		}
		else {
			kahanaGeoTokyo2WGS84( geo );
		}
		//apr_pool_cleanup_register( sp, (void*)geo, GeoCleanup, GeoCleanup );
	}
	
	return rc;
}

apr_status_t kahanaGeoCreateWithAngleAndDistanceFromOrigin( kGeo_t **newgeo, apr_pool_t *p, double angle, double dist, kGeo_t *origin )
{
	kGeo_t *geo = NULL;
	apr_status_t rc = kahanaGeoCreate( &geo, p, origin->lat, origin->lon, origin->precision, TRUE );
	
	if( rc == APR_SUCCESS )
	{
		double dist_sin;
		double dist_cos;
		
		*newgeo = geo;
		dist = dist / WGS84_major;
		dist_sin = sin( dist );
		dist_cos = cos( dist );
		angle = angle * rad;
		
		geo->lat = asin( origin->lat_sin * dist_cos + origin->lat_cos * dist_sin * cos( angle ) );
		geo->lon = origin->lon_rad + atan2( sin( angle ) * dist_sin * origin->lat_cos, ( dist_cos - origin->lat_sin * origin->lat_sin ) );
		geo->lon = fmod( geo->lon + M_PI, 2 * M_PI ) - M_PI;
		
		kahanaGeoSetLatLon( geo, geo->lat * degree, geo->lon * degree, geo->precision, TRUE );
	}
	
	return rc;
}

void kahanaGeoDestroy( kGeo_t *geo )
{
	if( geo ){
		apr_pool_destroy( geo->p );
	}
}

void kahanaGeoSetLatLon( kGeo_t *geo, double lat, double lon, size_t precision, bool isWGS84 )
{
	geo->lat = lat;
	geo->lon = lon;

	if( isWGS84 ){
		geo->lat_rad = degree * lat;
		geo->lon_rad = degree * lon;
		geo->lat_sin = sin( geo->lat );
		geo->lat_cos = cos( geo->lat );
		geo->lon_sin = sin( geo->lon );
		geo->lon_cos = cos( geo->lon );
		kahanaGeoHashEncode( geo, precision );
	}
	else {
		kahanaGeoTokyo2WGS84( geo );
	}
}

#pragma mark CONVERT
void kahanaGeoTokyo2WGS84( kGeo_t *geo )
{
	double olat = geo->lat;
	double olon = geo->lon;
	
	geo->lon = olon - olat * 0.000046038 - olon * 0.000083043 + 0.010040;
	geo->lat = olat - olat * 0.00010695 + olon * 0.000017464 + 0.0046017;
	kahanaGeoSetLatLon( geo, geo->lat, geo->lon, geo->precision, TRUE );
}

apr_status_t kahanaGeoDMS2Deg( const char *dms, double *rv )
{
	apr_status_t rc = APR_SUCCESS;

	if( !dms ){
		rc = APR_EINVAL;
	}
	else
	{
		double deg = 0;
		size_t len = strlen( dms );
		char buf[len];
		char *ptr = buf;
		const char *token;
		double val;
		int i = 0;
		
		memcpy( ptr, dms, len );
		ptr[len] = '\0';
		while( ( token = (const char*)apr_strtok( NULL, ".", (char**)&ptr ) ) )
		{
			val = kahanaStrtod( token );
			if( errno ){
				rc = errno;
				kahanaLogPut( NULL, NULL, "failed to kahanaStrtod(): %s", kahanaLogErr2Str( ETYPE_SYS, rc ) );
				break;
			}
			
			if( i == 0 ){
				deg = val;
			}
			else if( i == 1 )
			{
				deg += val/60;
				val = kahanaStrtod( ptr );
				if( errno ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to kahanaStrtod(): %s", kahanaLogErr2Str( ETYPE_SYS, rc ) );
					break;
				}
				deg += val/3600;
				break;
			}
			i++;
		}
		if( rc == APR_SUCCESS ){
			*rv = deg;
		}
	}
	
	return rc;
}

const char *kahanaGeoDeg2DMS( apr_pool_t *p, double deg )
{
	long ldeg = deg * 360000;
	
	return (const char*)apr_psprintf( p,"%2d.%02d.%02d.%02ld", 
						(int)floor( deg ),
						(int)(floor( ldeg / 6000 )) % 60,
						(int)(floor( ldeg / 100 )) % 60,
						ldeg % 100 );
}


#pragma mark GeoHash
void kahanaGeoHashEncode( kGeo_t *geo, size_t precision )
{
	static char bits[] = { 16, 8, 4, 2, 1 };
	double rangeLat[] = { -90.0, 90.0 };
	double rangeLon[] = { -180.0, 180.0 };
	bool even_bit = TRUE;
	double mid;
	int bit = 0, ch = 0;
	int i = 0;
	
	geo->precision = ( precision > 16 ) ? 16 : precision;
	while( i < geo->precision )
	{
		if( even_bit )
		{
			mid = ( rangeLon[0] + rangeLon[1] ) / 2;
			if( geo->lon >= mid ){
				ch |= bits[bit];
				rangeLon[0] = mid;
			}
			else{
				rangeLon[1] = mid;
			}
		}
		else
		{
			mid = ( rangeLat[0] + rangeLat[1] ) / 2;
			if( geo->lat >= mid ){
				ch |= bits[bit];
				rangeLat[0] = mid;
			}
			else{
				rangeLat[1] = mid;
			}
		}
		even_bit = !even_bit;
		if( bit < 4 ){
			bit++;
		}
		else {
			geo->geohash[i++] = BASE32[ch];
			bit = 0;
			ch = 0;
		}
	}
	geo->geohash[i] = '\0';
}

void kahanaGeoHashDecode( kGeo_t *geo )
{
	static char bits[] = { 16, 8, 4, 2, 1 };
	double rangeLat[] = { -90.0, 90.0 };
	double rangeLon[] = { -180.0, 180.0 };
	int i, j, hashlen;
	double lat_err, lon_err;
	char c, cd, mask, is_even=1;
	
	lat_err = 90.0;  lon_err = 180.0;
	hashlen = strlen( geo->geohash );
	for( i = 0; i < hashlen; i++ )
	{
		c = tolower( geo->geohash[i] );
		cd = strchr( BASE32, c ) - BASE32;
		for( j = 0; j < 5; j++ )
		{
			mask = bits[j];
			if( is_even ){
				lon_err /= 2;
				rangeLon[!(cd&mask)] = ( rangeLon[0] + rangeLon[1] ) / 2;
			} else {
				lat_err /= 2;
				rangeLat[!(cd&mask)] = ( rangeLat[0] + rangeLat[1] ) / 2;
			}
			is_even = !is_even;
		}
	}
	geo->lat = ( rangeLat[0] + rangeLat[1] ) / 2;
	geo->lon = ( rangeLon[0] + rangeLon[1] ) / 2;
}

