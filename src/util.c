#include "kahana_util.h"
#include "kahana_private.h"

apr_status_t kahanaUtilRand( void *rnd, size_t size )
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


// MARK: HMAC-SHA1
/* MARK: Utilities */
const char *kahanaUtilHMACSHA1( apr_pool_t *p, unsigned char *key, size_t ksize, const unsigned char *msg, size_t msize )
{
	apr_sha1_ctx_t ctx;
	const char hex[] = "0123456789abcdef";
	unsigned char digest[APR_SHA1_DIGESTSIZE];
	unsigned char ipad[65], opad[65];
	unsigned char *k = key;
	char result[APR_SHA1_DIGESTSIZE*2];
	char *hd;
	int i;
	
	/* if key is longer than 64 bytes reset it to key=SHA1(key) */
	if( ksize > 64 ){
		apr_sha1_init( &ctx );
		apr_sha1_update_binary( &ctx, k, ksize );
		apr_sha1_final( digest, &ctx );
		k = digest;
		ksize = APR_SHA1_DIGESTSIZE;
	}
	
	/* HMAC_MD5 transform */
	/* start out by string key in pads */
	memset( (void *)ipad, 0, sizeof( ipad ) );
	memset( (void *)opad, 0, sizeof( opad ) );
	memmove( ipad, k, ksize );
	memmove( opad, k, ksize );
	/* perform XOR inner and outer SHA-1 values */
	for( i = 0; i < 64; i++ ){
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}
	apr_sha1_init( &ctx );
	apr_sha1_update_binary( &ctx, ipad, 64 );
	apr_sha1_update_binary( &ctx, msg, msize );
	apr_sha1_final( digest, &ctx );
	apr_sha1_init( &ctx );
	apr_sha1_update_binary( &ctx, opad, 64 );
	apr_sha1_update_binary( &ctx, digest, sizeof( digest ) );
	apr_sha1_final( digest, &ctx );
	
	// create hexdigest
	hd = result;
	for( i = 0; i < sizeof( digest ); i++ ){
		*hd++ = hex[digest[i] >> 4];
		*hd++ = hex[digest[i] & 0xf];
	}
	*hd = '\0';
	/*
		MARK: HMAC-SHA1 digest to BASE64 
		len = apr_base64_encode_len( sizeof( digest ) );
		hex_digest = apr_pcalloc( p, len );
		len = apr_base64_encode_binary( hex_digest, digest, sizeof( digest ) );
		hex_digest[len - 2] = '\0';
	*/
	return apr_pstrdup( p, result );
}


apr_status_t kahanaUtilChecksum( apr_pool_t *p, kCheckSumType_e type, const char **hex_digest, const char *msg, ... )
{
	apr_status_t rc = APR_SUCCESS;
	char *keys = NULL;
	
	if( !( keys = calloc( 0, sizeof( char ) ) ) ){
		rc = APR_ENOMEM;
	}
	else
	{
		va_list args;
		char *key;
		int i, len = 0;
		
		va_start( args, msg );
		while( ( key = va_arg( args, char* ) ) )
		{
			if( ( i = strlen( key ) ) )
			{
				if( !( keys = realloc( keys, len + i + 1 ) ) ){
					rc = APR_ENOMEM;
					len = 0;
					break;
				}
				memcpy( keys + len, key, i );
				len += i;
				keys[len] = '\0';
			}
		}
		va_end( args );
		
		if( len )
		{
			rc = APR_SUCCESS;
			if( type == CHKSUM_STR )
			{
				char *m = NULL;
				
				if( asprintf( &m, "%s", msg ) == -1 ){
					rc = APR_ENOMEM;
				}
				else {
					*hex_digest = kahanaUtilHMACSHA1( p, (unsigned char*)keys, len, (const unsigned char*)m, strlen( m ) );
					free( m );
				}
			}
			else if( type == CHKSUM_FILE )
			{
				apr_file_t *fp = NULL;
				
				if( ( rc = apr_file_open( &fp, msg, APR_READ|APR_BINARY, APR_OS_DEFAULT, p ) ) == APR_SUCCESS )
				{
					apr_finfo_t finfo;
					apr_mmap_t *mm = NULL;

					if( ( rc = apr_file_info_get( &finfo, APR_FINFO_SIZE, fp ) ) == APR_SUCCESS &&
						( rc = apr_mmap_create( &mm, fp, 0, finfo.size, APR_MMAP_READ, p ) ) != APR_SUCCESS ){
						mm = NULL;
					}
					apr_file_close( fp );
					
					if( mm ){
						*hex_digest = kahanaUtilHMACSHA1( p, (unsigned char*)keys, len, mm->mm, finfo.size );
						rc = apr_mmap_delete( mm );
					}
				}
			}
			else {
				rc = APR_BADARG;
			}
		}
		free( keys );
	}
	
	return rc;
}

