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
		fprintf( stderr, "failed to kahanaInitilize(): %s", STRERROR_APR( rc ) );
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

// MARK:  UTIL
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


