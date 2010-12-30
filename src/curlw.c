/* 
**
**  Modify : 4/17/2010
**  (C) Masatoshi Teruya
**
*/

#include "kahana_curlw.h"
#include "kahana_private.h"

static apr_status_t CurlWrapDestroy( void *data )
{
	kCurlWrap_t *cw = (kCurlWrap_t*)data;
	
	if( cw )
	{
		if( cw->curl ){
			curl_easy_cleanup( cw->curl );
		}
		if( cw->query ){
			apr_hash_clear( cw->query );
		}
	}
	
	return APR_SUCCESS;
}


static size_t cbCurlWrapReceive( void *buf, size_t len, size_t nitems, kCurlWrap_t *cw )
{
	apr_size_t size = len * nitems;
	
	kahanaBufStrNCat( cw->rcvd, buf, size );
	
	return size;
}

apr_status_t kahanaCurlWrapCreate( kCurlWrap_t **newcw, apr_pool_t *p )
{
	apr_status_t rc;
	apr_pool_t *rp;
	kCurlWrap_t *cw = NULL;
	kBuffer_t *bo = NULL;
	CURL *curl = NULL;
	
	if( ( rc = kahanaMalloc( p, sizeof( kCurlWrap_t ), (void**)&cw, &rp ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
	}
	else if( ( rc = kahanaBufCreate( &bo, rp, sizeof( char ), 0 ) ) ){
		apr_pool_destroy( rp );
	}
	else if( !( curl = curl_easy_init() ) ){
		kahanaLogPut( NULL, NULL, "failed to curl_easy_init(): %s", strerror( errno ) );
		rc = APR_EGENERAL;
		apr_pool_destroy( rp );
	}
	else
	{
		cw->p = rp;
		cw->curl = curl;
		cw->res = 0;
		cw->url = NULL;
		cw->query = apr_hash_make( rp );
		cw->ec = APR_SUCCESS;
		cw->errstr = NULL;
		cw->rcvd = bo;
		*newcw = cw;
		apr_pool_cleanup_register( rp, (void*)cw, CurlWrapDestroy, apr_pool_cleanup_null );
	}
	
	return rc;
}

apr_status_t kahanaCurlWrapSetQuery( kCurlWrap_t *cw, const char *key, const char *val, bool escape )
{
	cw->ec = APR_SUCCESS;
	
	if( key )
	{
		if( val && escape )
		{
			char *escval = curl_easy_escape( cw->curl, val, strlen( val ) );
			
			if( !escval ){
				cw->errstr = apr_psprintf( cw->p, "failed to curl_easy_escape(): %s", strerror( errno ) );
				kahanaLogPut( NULL, NULL, cw->errstr );
				cw->ec = APR_EGENERAL;
			}
			else {
				val = apr_pstrdup( cw->p, escval );
				free( escval );
			}
		}
		
		if( cw->ec == APR_SUCCESS ){
			apr_hash_set( cw->query, key, APR_HASH_KEY_STRING, val );
		}
	}
	
	return cw->ec;
}

apr_status_t kahanakCurlWrapRequest( kCurlWrap_t *cw, bool post, const char *url )
{
	apr_pool_t *tp = NULL;
	
	if( ( cw->ec = apr_pool_create( &tp, cw->p ) ) ){
		cw->errstr = apr_psprintf( cw->p, "failed to apr_pool_create(): %s", STRERROR_APR( cw->ec ) );
		kahanaLogPut( NULL, NULL, cw->errstr );
		return cw->ec;
	}
	else
	{
		apr_hash_index_t *idx = apr_hash_first( tp, cw->query );
		char *key = NULL;
		char *val = NULL;
		const char *query = "";
		
		while( idx )
		{
			apr_hash_this( idx, (const void**)&key, NULL, (void*)&val );
			if( key ){
				query = (const char*)apr_pstrcat( tp, query, "&", key, "=", val, NULL );
			}
			idx = apr_hash_next( idx );
		}
		query++;
		// POST request
		if( post ){
			cw->url = (const char*)apr_pstrdup( cw->p, url );
			curl_easy_setopt( cw->curl, CURLOPT_POST, post );
			curl_easy_setopt( cw->curl, CURLOPT_POSTFIELDS, query );
		}
		// GET request
		else{
			cw->url = (const char*)( ( query ) ? apr_psprintf( cw->p, "%s?%s", url, query ) : apr_pstrdup( cw->p, url ) );
		}
		apr_pool_destroy( tp );
		
		curl_easy_setopt( cw->curl, CURLOPT_NOPROGRESS, 1 );
		curl_easy_setopt( cw->curl, CURLOPT_URL, cw->url );
		curl_easy_setopt( cw->curl, CURLOPT_WRITEFUNCTION, cbCurlWrapReceive );
		curl_easy_setopt( cw->curl, CURLOPT_FILE, cw );
		if( ( cw->res = curl_easy_perform( cw->curl ) ) ){
			cw->ec = cw->res;
			cw->errstr = apr_psprintf( cw->p, "failed to curl_easy_perform(): %s", curl_easy_strerror( cw->res ) );
			kahanaLogPut( NULL, NULL, cw->errstr );
		}
	}
	
	return cw->ec;
}


apr_status_t _kahanaCurlWrapInit( void )
{
	apr_status_t rc = curl_global_init( CURL_GLOBAL_ALL );
	
	if( rc != 0 ){
		kahanaLogPut( NULL, NULL, "failed to curl_global_init(): %s", curl_easy_strerror( rc ) );
	}
	return rc;
}

void _kahanaCurlWrapCleanup( void ){
	curl_global_cleanup();	
}
