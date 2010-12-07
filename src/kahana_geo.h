/*
 *  kahana_geo.h
 *  Modify : 4/16/2010
 *  (C) Masatoshi Teruya All rights reserved.
 *
 */

#ifndef ___KAHANA_GEO___
#define ___KAHANA_GEO___

#include "kahana.h"
#include <math.h>

typedef struct
{
	apr_pool_t *p;
	double lat;
	double lon;
	double lat_rad;
	double lat_sin;
	double lat_cos;
	double lon_rad;
	double lon_sin;
	double lon_cos;
	
	char geohash[16];
	int precision;
} kGeo_t;

apr_status_t kahanaGeoCreate( kGeo_t **newgeo, apr_pool_t *p, double lat, double lon, size_t precision, bool isWGS84 );
apr_status_t kahanaGeoCreateWithAngleAndDistanceFromOrigin( kGeo_t **newgeo, apr_pool_t *p, double angle, double dist, kGeo_t *origin );
void kahanaGeoDestroy( kGeo_t *geo );
void kahanaGeoSetLatLon( kGeo_t *geo, double lat, double lon, size_t precision, bool isWGS84 );

void kahanaGeoTokyo2WGS84( kGeo_t *geo );
apr_status_t kahanaGeoDMS2Deg( const char *dms, double *rv );
const char *kahanaGeoDeg2DMS( apr_pool_t *p, double deg );

void kahanaGeoHashEncode( kGeo_t *geo, size_t precision );
void kahanaGeoHashDecode( kGeo_t *geo );

#endif
