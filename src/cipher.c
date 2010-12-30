/* 
**
**  Modify : 04/20/2010
**  (C) Masatoshi Teruya
**
*/

#include "kahana_cipher.h"

struct kCipher_t{
	apr_pool_t *p;
	const char *name;
	const char *mode;
	const char *iv;
	const char *enc;
	const char *dec;
	int iv_size;
	int block_bytes;
	size_t enc_bytes;
	size_t dec_bytes;
};


static apr_status_t CipherDestroy( void *data )
{
	/*
	kCipher_t *co = (kCipher_t*)data;
	
	if( co )
	{
		if( req->curl ){
			curl_easy_cleanup( req->curl );
		}
		if( req->query ){
			apr_hash_clear( req->query );
		}
	}
	*/
	return APR_SUCCESS;
}

static const char *CreateIV( apr_pool_t *p, int iv_size )
{
	static const char iv_code[64] = 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789"
		"-_";
	char iv[iv_size];
	int i = 0;
	
	for( i = 0; i < iv_size; i++ ){
		iv[i] = iv_code[(int)( rand() * 63.0 / ( RAND_MAX + 1.0 ) )];
	}
	iv[iv_size] = '\0';
	
	return (const char*)apr_pstrdup( p, iv );
}

kCipher_t *kahanaCipherInit( apr_pool_t *p, const char *cipher, const char *mode, const char *iv, const char **errstr )
{
	apr_status_t rc;
	apr_pool_t *sp = NULL;
	kCipher_t *co = NULL;
	
	// check algorithm
	if( mcrypt_module_self_test( (char*)cipher, NULL ) ){
		*errstr = "selected cipher has not block mode.";
	}
	else
	{
		// check mode
		int block_mode = mcrypt_module_is_block_mode( (char*)mode, NULL );
		
		if( block_mode && !mcrypt_module_is_block_algorithm_mode( (char*)cipher, NULL ) ){
			*errstr = apr_psprintf( p, "selected cipher[%s:%s] cannot work with block mode.", cipher, mode );
		}
		else if( !block_mode && mcrypt_module_is_block_algorithm( (char*)cipher, NULL ) ){
			*errstr = apr_psprintf( p, "selected cipher[%s:%s] work with only block mode.", cipher, mode );
		}
		else if( ( rc = apr_pool_create( &sp, p ) ) ){
			*errstr = apr_psprintf( p, "%s",  STRERROR_APR( rc ) );
		}
		else
		{
			co = (kCipher_t*)apr_pcalloc( sp, sizeof( kCipher_t ) );
			co->p = sp;
			co->name = apr_pstrdup( sp, cipher );
			co->mode = apr_pstrdup( sp, mode );
			co->iv = NULL;
			co->enc = NULL;
			co->dec = NULL;
			co->iv_size = 0;
			co->block_bytes = 0;
			co->enc_bytes = 0;
			co->dec_bytes = 0;
			apr_pool_cleanup_register( sp, (void*)co, CipherDestroy, apr_pool_cleanup_null );

			// check Initialize Vector
			if( iv && ( co->iv_size = strlen( iv ) ) ){
				co->iv = (const char*)apr_pstrdup( sp, iv );
			}
		}
	}
	
	return co;
}

void kahanaCipherDestroy( kCipher_t *co )
{
	if( co ){
		apr_pool_destroy( co->p );
	}
}

const char *kahanaCipherEncData( kCipher_t *co )
{
	return (const char*)apr_pstrdup( co->p, co->enc );
}
// encode base64 binary
const char *kahanaCipherEncDataBase64( kCipher_t *co )
{
	char *buf = (char*)apr_pcalloc( co->p, (apr_size_t)apr_base64_encode_len( co->enc_bytes ) );
	
	apr_base64_encode_binary( buf, (const unsigned char*)co->enc, co->enc_bytes );
	
	return buf;
}

size_t kahanaCipherEncBytes( kCipher_t *co ){
	return co->enc_bytes;
}
const char *kahanaCipherDecData( kCipher_t *co ){
	return (const char*)apr_pstrdup( co->p, co->dec );
}
size_t kahanaCipherDecBytes( kCipher_t *co ){
	return co->dec_bytes;
}

const char *kahanaCipherIVFromEncData( const char *base64url )
{
	char *ptr = NULL;
	
	for( ptr = (char*)base64url; *ptr; ptr++ )
	{
		if( *ptr == '@' ){
			*ptr++ = '\0';
			break;
		}
	}
	
	return ptr;
}

void kahanaCipherSetIV( kCipher_t *co, const char *iv )
{
	co->iv = ( iv ) ? (const char*)apr_pstrdup( co->p, iv ) : NULL;
	co->iv_size = ( iv ) ? strlen( iv ) : 0;
}

const char *kahanaCipherEncrypt( kCipher_t *co, const char *str, const char *clue )
{
	int rc;
	const char *errstr = NULL;
	MCRYPT mc = mcrypt_module_open( (char*)co->name, NULL, (char*)co->mode, NULL );
	
	if( mc == MCRYPT_FAILED ){
		errstr = apr_psprintf( co->p, "Failed to initialize cipher - %s-%s", co->name, co->mode );
	}
	else
	{
		int iv_size = mcrypt_enc_get_iv_size( mc );
		
		if( !co->iv_size ){
			co->iv_size = iv_size;
			co->iv = CreateIV( co->p, co->iv_size );
		}
		else if( co->iv_size != iv_size ){
			errstr = apr_psprintf( co->p, "Invalid cipher[%s:%s] vector size[%d]: %d", co->name, co->mode, iv_size, co->iv_size );
		}
		
		if( !errstr )
		{
			if( ( rc = mcrypt_generic_init( mc, (void*)clue, strlen( clue ), (void*)co->iv ) ) < 0 ){
				errstr = apr_psprintf( co->p, "%s", mcrypt_strerror( rc ) );
			}
			else
			{
				unsigned char *tmp = NULL;
				size_t len = strlen( str );
				size_t ext_len = 0;
				
				if( !mcrypt_enc_is_block_mode( mc ) ){
					co->block_bytes = 0;
					tmp = calloc( len, sizeof( char ) );
					memcpy( (void*)tmp, str, len );
				}
				else
				{
					co->block_bytes = mcrypt_enc_get_block_size( mc );
					ext_len = len % co->block_bytes;
					
					if( ext_len ){
						ext_len = len + co->block_bytes - ext_len;
						tmp = calloc( ext_len, 1 );
					}
					memcpy( (void*)tmp, str, len );
				}
				
				// encrypt
				if( ( rc = mcrypt_generic( mc, (void*)tmp, ext_len ) ) != 0 ){
					errstr = apr_psprintf( co->p, "%s", mcrypt_strerror( rc ) );
				}
				
				co->enc = apr_pcalloc( co->p, ext_len );
				memmove( (void*)co->enc, tmp, ext_len );
				co->enc_bytes = ext_len;
				free( tmp );
				mcrypt_generic_deinit( mc );
			}
		}
		
		if( ( rc = mcrypt_module_close( mc ) ) ){
			errstr = apr_psprintf( co->p, "%s", mcrypt_strerror( rc ) );
		}
	}
	
	return errstr;
}

const char *kahanaCipherDecrypt( kCipher_t *co, const char *base64, const char *clue )
{
	int rc;
	const char *errstr = NULL;
	MCRYPT mc = mcrypt_module_open( (char*)co->name, NULL, (char*)co->mode, NULL );
	
	if( mc == MCRYPT_FAILED ){
		errstr = apr_psprintf( co->p, "Failed to initialize cipher - %s-%s", co->name, co->mode );
	}
	else
	{
		if( ( rc = mcrypt_generic_init( mc, (void*)clue, strlen( clue ), (void*)co->iv ) ) < 0 ){
			errstr = apr_psprintf( co->p, "%s", mcrypt_strerror( rc ) );
		}
		else
		{
			int len = apr_base64_decode_len( base64 );
			char buf[len];
			
			len = apr_base64_decode_binary( (unsigned char*)buf, base64 );
			mdecrypt_generic( mc, buf, len );
			co->dec = (const char*)apr_pstrdup( co->p, buf ); 
			co->dec_bytes = strlen( co->dec );
		}
		
		if( ( rc = mcrypt_module_close( mc ) ) ){
			errstr = apr_psprintf( co->p, "%s", mcrypt_strerror( rc ) );
		}
	}
	
	return errstr;
}

