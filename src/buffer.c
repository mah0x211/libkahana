/* 
**
**  Modify : 11/25/2009
**  (C) Masatoshi Teruya
**
*/
#include "kahana_buffer.h"

struct kBuffer_t{
	// cleaner
	apr_pool_t *p;
	void *mem;
	size_t bytes;
	size_t size;
	size_t nalloc;
	size_t nmem;
};

static apr_status_t BufferCleanup( void *ctx )
{
	kBuffer_t *bo = (kBuffer_t*)ctx;
	
	if( bo && bo->mem ){
		free( bo->mem );
	}
	return APR_SUCCESS;
}

static int BufRealloc( kBuffer_t *bo, size_t nalloc )
{
	// expand memory
	if( nalloc > bo->nmem )
	{
		nalloc = bo->bytes * ( nalloc + bo->nalloc );
		if( ( bo->mem = realloc( bo->mem, nalloc ) ) ){
			bo->nmem = nalloc;
		}
		else {
			return errno;
		}
	}
	
	return 0;
}

static int BufShift( kBuffer_t *bo, size_t cur, size_t n, size_t padding )
{
	int rc = 0;
	
	if( n != 0 )
	{
		// right shift
		if( n > 0 )
		{
			if( ( rc = BufRealloc( bo, bo->size + n + padding ) ) == 0 ){
				memmove( bo->mem + cur + n, bo->mem + cur, bo->size - cur );
			}
		}
		// left shift
		else{
			memcpy( bo->mem + cur + n, bo->mem + cur, bo->size - cur );
		}
		bo->size += n;
	}
	
	return rc;
}

apr_status_t kahanaBufCreate( kBuffer_t **newbo, apr_pool_t *p, size_t bytes, size_t nalloc )
{
	kBuffer_t *bo = NULL;
	apr_status_t rc;
	apr_pool_t *sp;
	
	if( ( rc = kahanaMalloc( p, sizeof( kBuffer_t ), (void**)&bo, &sp ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
	}
	else
	{
		bo->p = sp;
		bo->bytes = bytes;
		bo->size = 0;
		bo->nalloc = nalloc;
		bo->nmem = bytes * nalloc;
		
		if( !( bo->mem = calloc( bo->nalloc, bo->bytes ) ) ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to calloc(): %s", strerror( rc ) );
			apr_pool_destroy( sp );
			bo = NULL;
		}
		else{
			*newbo = bo;
			apr_pool_cleanup_register( sp, (void*)bo, BufferCleanup, BufferCleanup );
		}
	}
	
	return rc;
}
void kahanaBufDestroy( kBuffer_t *bo )
{
	if( bo ){
		apr_pool_destroy( bo->p );
	}
}

int kahanaBufCompaction( kBuffer_t *bo )
{
	int rc = 0;
	size_t nsize = bo->bytes * bo->size;
	
	// expand memory
	if( bo->nmem > nsize )
	{
		bo->nmem = nsize;
		if( ( bo->mem = realloc( bo->mem, nsize ) ) ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to realloc(): %s", strerror( rc ) );
		}
	}
	
	return rc;
}

const void *kahanaBufPtr( kBuffer_t *bo ){
	return bo->mem;
}
size_t kahanaBufBytes( kBuffer_t *bo ){
	return bo->bytes;
}
size_t kahanaBufSize( kBuffer_t *bo ){
	return bo->size;
}
size_t kahanaBufAllocSize( kBuffer_t *bo ){
	return bo->nmem;
}
size_t kahanaBufNAllocSize( kBuffer_t *bo ){
	return bo->nalloc;
}
void kahanaBufSetNAllocSize( kBuffer_t *bo, size_t nalloc ){
	bo->nalloc = nalloc;
}

// MARK:  String
int kahanaBufStrCompaction( kBuffer_t *bo )
{
	int rc = 0;
	size_t nsize = bo->bytes * bo->size;
	
	// expand memory
	if( bo->nmem > nsize )
	{
		bo->nmem = nsize + 1;
		if( ( bo->mem = realloc( bo->mem, nsize ) ) ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to realloc(): %s", strerror( rc ) );
		}
	}
	
	return rc;
}

int kahanaBufStrSet( kBuffer_t *bo, const char *str )
{
	int rc = 0;
	
	if( str )
	{
		size_t len = strlen( str );
		if( ( rc = BufRealloc( bo, len + 1 ) ) != 0 ){
			kahanaLogPut( NULL, NULL, "failed to kahanaBufStrSet(): %s", strerror( rc ) );
		}
		else{
			memset( bo->mem, 0, bo->nmem );
			memcpy( bo->mem, str, len );
			bo->size = len;
			((char*)bo->mem)[bo->size] = '\0';
		}
	}
	
	return rc;
}

int kahanaBufStrNSet( kBuffer_t *bo, const char *str, size_t len )
{
	int rc = 0;
	
	if( len > 0 )
	{
		if( ( rc = BufRealloc( bo, len + 1 ) ) != 0 ){
			kahanaLogPut( NULL, NULL, "failed to kahanaBufStrNSet(): %s", strerror( rc ) );
		}
		else{
			memset( bo->mem, 0, bo->nmem );
			memcpy( bo->mem, str, len );
			bo->size = len;
			((char*)bo->mem)[bo->size] = '\0';
		}
	}
	
	return rc;
}

int kahanaBufStrShift( kBuffer_t *bo, size_t cur, size_t n, size_t padding )
{
	int rc = 0;
	size_t len = bo->size;
	
	if( ( rc = BufShift( bo, cur, n, padding ) ) != 0 ){
		kahanaLogPut( NULL, NULL, "failed to kahanaBufStrShift(): %s", strerror( rc ) );
	}
	else if( len != bo->size ){
		((char*)bo->mem)[bo->size] = '\0';
	}
	
	return rc;
}

int kahanaBufStrCat( kBuffer_t *bo, const char *str )
{
	int rc = 0;
	
	if( str )
	{
		size_t len = strlen( str );
		
		if( ( rc = BufRealloc( bo, bo->size + len + 1 ) ) != 0 ){
			kahanaLogPut( NULL, NULL, "failed to kahanaBufStrCat(): %s", strerror( rc ) );
		}
		else{
			memcpy( bo->mem + bo->size, str, len );
			bo->size += len;
			((char*)bo->mem)[bo->size] = '\0';
		}
	}
	
	return rc;
}

int kahanaBufStrNCat( kBuffer_t *bo, const char *str, size_t n )
{
	int rc = 0;
	
	if( n > 0 )
	{
		if( ( rc = BufRealloc( bo, bo->size + n + 1 ) ) != 0 ){
			kahanaLogPut( NULL, NULL, "failed to kahanaBufStrCat(): %s", strerror( rc ) );
		}
		else{
			memcpy( bo->mem + bo->size, str, n );
			bo->size += n;
			((char*)bo->mem)[bo->size] = '\0';
		}
	}
	
	return rc;
}


int kahanaBufStrChrCat( kBuffer_t *bo, const char c )
{
	int rc = BufRealloc( bo, bo->size + 2 );
	
	if( rc != 0 ){
		kahanaLogPut( NULL, NULL, "failed to kahanaBufStrCat(): %s", strerror( rc ) );
	}
	else {
		((char*)bo->mem)[bo->size] = c;
		bo->size += 1;
		((char*)bo->mem)[bo->size] = '\0';
	}
	
	return rc;
}

int kahanaBufStrInsert( kBuffer_t *bo, const char *ins, size_t cur )
{
	int rc = 0;
	
	if( cur >= 0 )
	{
		size_t len = strlen( ins );
		
		if( ( rc = BufShift( bo, cur, len, 1 ) ) != 0 ){
			kahanaLogPut( NULL, NULL, "failed to kahanaBufStrInsert(): %s", strerror( rc ) );
		}
		// insert string
		else{
			memcpy( bo->mem + cur, ins, len );
			((char*)bo->mem)[bo->size] = '\0';
		}
	}
	
	return rc;
}


int kahanaBufStrReplace( kBuffer_t *bo, const char *rep, const char *str, size_t n )
{
	int rc = 0;
	
	if( rep && str )
	{
		char *match = bo->mem;
		size_t len = strlen( str );
		size_t rlen = strlen( rep );
		size_t shift = len - rlen;
		size_t cur;
		
		// number of replace
		if( n <= 0 )
		{
			while( ( match = strstr( match, rep ) ) )
			{
				cur = bo->size - strlen( match );
				if( shift != 0 )
				{
					if( ( rc = BufShift( bo, cur + rlen, shift, 1 ) ) != 0 ){
						break;
					}
					else {
						((char*)bo->mem)[bo->size] = '\0';
					}
				}
				if( len ){
					memcpy( match, str, len );
					match += shift + len;
				}
			}
		}
		else
		{
			size_t i = 0;
			for( i = 0; i < n && ( match = strstr( match, rep ) ); i++ )
			{
				cur = bo->size - strlen( match );
				if( shift != 0 )
				{
					if( ( rc = BufShift( bo, cur + rlen, shift, 1 ) ) != 0 ){
						break;
					}
					else {
						((char*)bo->mem)[bo->size] = '\0';
					}
				}
				if( len ){
					memcpy( match, str, len );
					match += len;
				}
			}
		}
	}
	
	return rc;
}

int kahanaBufStrReplaceFromTo( kBuffer_t *bo, size_t from, size_t to, const char *str )
{
	int rc = 0;
	
	if( from >= 0 && from < to )
	{
		size_t len = strlen( str );
		size_t rlen = to - from;
		size_t shift = len - rlen;
		
		if( shift != 0 ){
			rc = BufShift( bo, from + rlen, shift, 1 );
		}
		
		if( rc == 0 )
		{
			// replace string
			if( len ){
				memcpy( bo->mem + from, str, len );
			}
			((char*)bo->mem)[bo->size] = '\0';
		}
	}
	
	return rc;
}


