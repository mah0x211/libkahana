#include "kahana.h"
#include "kahana_private.h"

// global object struct definition in kahana_private.h
static Kahana_t *kObj = NULL;
// get global object
Kahana_t *_kahanaGlobal( void ){
	return kObj;
}

// initialize
apr_status_t kahanaInitilize( void )
{
	apr_status_t rc;
	
	// init apr and
	if( ( rc = apr_initialize() ) == APR_SUCCESS )
	{
		apr_pool_t *p = NULL;
		
		// create pool and kahana object
		if( ( rc = apr_pool_create( &p, NULL ) ) ||
			!( kObj = (Kahana_t*)apr_pcalloc( p, sizeof( Kahana_t ) ) ) ){
			rc = ( rc == APR_SUCCESS ) ? APR_ENOMEM : rc;
			apr_pool_destroy( p );
		}
		else
		{
			kObj->p = p;
			kObj->debug = TRUE;
			kObj->logs = apr_hash_make( p );
			kObj->jaMap = NULL;
			// initilize net, str, io, module
			if( ( rc = _kahanaCurlWrapInit() ) ||
				( rc = _kahanaStrInit() ) ||
				( rc = _kahanaIOInit( kObj ) ) ||
				//!( rc = apr_dbd_init( p ) ) &&
				//( rc = _kahanaDbInit( kObj ) ) ||
				( rc = _kahanaModuleInit( kObj ) ) ){
				apr_pool_destroy( p );
			}
		}
	}
	
	if( rc != APR_SUCCESS ){
		fprintf( stderr, "failed to kahanaInitilize(): %s", kahanaLogErr2Str( ETYPE_APR, rc ) );
	}
	
	return rc;
}

// terminate
void kahanaTerminate( void )
{
	if( kObj ){
		_kahanaCurlWrapCleanup();
		_kahanaModuleCleanup();
		_kahanaStrCleanup();
		apr_pool_destroy( kObj->p );
	}
}

#pragma mark UTIL
apr_status_t kahanaMalloc( apr_pool_t *p, size_t size, void **newobj, apr_pool_t **newp )
{
	apr_status_t rc = APR_SUCCESS;
	apr_pool_t *sp = NULL;
	
	if( ( rc = apr_pool_create( &sp, ( p ) ? p : kObj->p ) ) == APR_SUCCESS )
	{
		void *obj = apr_pcalloc( sp, size );
		
		if( !obj ){
			rc = APR_ENOMEM;
			apr_pool_destroy( sp );
		}
		else {
			*newobj = obj;
			*newp = sp;
		}
	}
	
	return rc;
}

const char **kahanaStackTrace( unsigned int level )
{
	void *buf[level];
	int naddr = backtrace( buf, level );
	char **syms = (char**)backtrace_symbols( buf, naddr );
	
	if( !syms ){
		kahanaLogPut( NULL, NULL, "failed to backtrace_symbols() reason %s", strerror( errno ) );
	}
	else{
		syms = realloc( syms, sizeof( char* ) * ( naddr + 1 ) );
		syms[naddr] = NULL;
	}
	
	return (const char**)syms;
}


apr_status_t kahanaRandom( void *rnd, size_t size )
{
	apr_pool_t *tp = NULL;
	apr_status_t rc;
	
	if( !( rc = apr_pool_create( &tp, NULL ) ) )
	{
		apr_file_t *fdes = NULL;
		
		if( !( rc = apr_file_open( &fdes, "/dev/urandom", APR_READ, APR_UREAD|APR_OS_DEFAULT, tp ) ) ){
			rc = apr_file_read( fdes, rnd, &size );
			apr_file_close( fdes );
		}
		else{
			*((int*)rnd) = rand();
		}
		apr_pool_destroy( tp );
	}
	
	return rc;
}

apr_status_t kahanaUniqueId( const char **unique, apr_pool_t *p )
{
	apr_status_t rc;
	unsigned long rnd = 0;
	
	if( !( rc = kahanaRandom( (void*)&rnd, sizeof( rnd ) ) ) )
	{
		char *msg = NULL;
		char *key = NULL;
		
		asprintf( &msg, "%10lu", rnd );
		asprintf( &key, "%llu", apr_time_now );
		
		*unique = kahanaHmacSHA1( p, msg, key );
		free( msg );
		free( key );
	}
	
	return rc;
}

const char *kahanaHmacSHA1( apr_pool_t *p, const char *msg, const char *key )
{
	apr_sha1_ctx_t ctx;
	unsigned char digest[APR_SHA1_DIGESTSIZE];
	unsigned char k_ipad[65], k_opad[65];
	unsigned char kt[APR_SHA1_DIGESTSIZE];
	unsigned char *k = (unsigned char *)key;
    const char hex[] = "0123456789abcdef";
	char hex_digest[APR_SHA1_DIGESTSIZE*2], *hd;
	int len, i;

	len = strlen( key );
	// if key is longer than 64 bytes reset it to key=SHA1(key)
	if( len > 64 ){
		k = kt;
		len = APR_SHA1_DIGESTSIZE; 
	}
	
	// HMAC_MD5 transform
	// start out by string key in pads
	memset((void *)k_ipad, 0, sizeof(k_ipad));
	memset((void *)k_opad, 0, sizeof(k_opad));
	memmove(k_ipad, k, len);
	memmove(k_opad, k, len);

	// XOR key with ipad and opad values
	for (i = 0; i < 64; i++){
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}
	// perform inner SHA-1
	apr_sha1_init( &ctx );
	apr_sha1_update_binary( &ctx, k_ipad, 64 );
	apr_sha1_update_binary( &ctx, (const unsigned char *)msg, strlen( msg ) );
	apr_sha1_final( digest, &ctx );
	// perform outer SHA-1
	apr_sha1_init( &ctx );
	apr_sha1_update_binary( &ctx, k_opad, 64 );
	apr_sha1_update_binary( &ctx, digest, sizeof( digest ) );
	apr_sha1_final( digest, &ctx );
	
	// create hexdigest
	hd = hex_digest;
	for( i = 0; i < sizeof( digest ); i++ ){
		*hd++ = hex[digest[i] >> 4];
		*hd++ = hex[digest[i] & 0xf];
	}
	*hd = '\0';
	/*
		len = apr_base64_encode_len( sizeof( digest ) );
		hex_digest = apr_pcalloc( p, len );
		len = apr_base64_encode_binary( hex_digest, digest, sizeof( digest ) );
		hex_digest[len - 2] = '\0';
	*/
	
	return (const char*)apr_pstrdup( p, hex_digest );
}
