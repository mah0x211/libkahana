/* 
**
**  Modify : 05/05/2010
**  (C) Masatoshi Teruya
**
*/

#include "kahana_io.h"
#include "kahana_private.h"

struct kIOCache_t{
	apr_pool_t *p;
	bool store;
	size_t size;
	apr_hash_t *hash;
};

typedef struct {
	apr_pool_t *p;
	void *data;
	const char *key;
	size_t size;
} kIOCacheFile_t;

static kIOCache_t *GetCache( void )
{
	Kahana_t *kahana = _kahanaGlobal();
	return ( kahana ) ? kahana->iocache : NULL;
}

// MARK:  CACHE
static void *CacheFile( apr_pool_t *p, const char *key, time_t mtime, size_t *size )
{
	kIOCache_t *c = GetCache();
	void *cache = NULL;
	
	if( c && c->store )
	{
		*size = 0;
		kIOCacheFile_t *cfile = (kIOCacheFile_t*)apr_hash_get( c->hash, key, APR_HASH_KEY_STRING );
		
		if( cfile )
		{
			char *epoch = (char*)( cfile->data + cfile->size );
			
			if( epoch[0] == '|' && kahanaStrtoll( epoch + 1, 10 ) == mtime )
			{
				cache = apr_pcalloc( p, cfile->size );
				memcpy( cache, cfile->data, cfile->size );
				*size = cfile->size;
			}
		}
	}
	
	return cache;
}

static apr_status_t AddCache( const char *key, void *data, size_t size, time_t mtime )
{
	apr_status_t rc = APR_SUCCESS;
	kIOCache_t *c = GetCache();
	
	if( c && c->store )
	{
		apr_pool_t *p = NULL;
		kIOCacheFile_t *cfile = (kIOCacheFile_t*)apr_hash_get( c->hash, key, APR_HASH_KEY_STRING );
		char *epoch = NULL;
		size_t elen = 0;
		
		// free if cache exist
		if( cfile ){
			c->size -= cfile->size;
			apr_hash_set( c->hash, cfile->key, APR_HASH_KEY_STRING, NULL );
			apr_pool_destroy( cfile->p );
			cfile = NULL;
		}
		
		// create cache packet
		if( ( rc = kahanaMalloc( c->p, sizeof( kIOCacheFile_t ), (void**)&cfile, &p ) ) ){
			kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
		}
		else if( ( elen = asprintf( &epoch, "|%lld", (long long)mtime ) ) == -1 ){
			rc = errno;
			apr_pool_destroy( p );
			kahanaLogPut( NULL, NULL, "failed to asprintf(): %s", strerror( rc ) );
		}
		else
		{
			cfile->p = p;
			cfile->key = (const char*)apr_pstrdup( p, key );
			cfile->size = size;
			cfile->data = (void*)apr_pcalloc( p, size + elen );
			memcpy( cfile->data, data, size );
			memcpy( cfile->data + size, epoch, elen );
			((unsigned char*)cfile->data)[size + elen] = '\0';
			c->size += size + elen;
			apr_hash_set( c->hash, cfile->key, APR_HASH_KEY_STRING, (void*)cfile );
			free( epoch );
		}
	}
	
	return rc;
}


// MARK:  CACHE STATUS
bool kahanaIOIsCached( const char *path )
{
	kIOCache_t *c = GetCache();
	bool cached = FALSE;
	
	if( c && c->store )
	{
		kIOCacheFile_t *cfile = (kIOCacheFile_t*)apr_hash_get( c->hash, path, APR_HASH_KEY_STRING );
		
		if( cfile )
		{
			struct stat finfo;
			
			if( stat( path, &finfo ) == -1 ){
				kahanaLogPut( NULL, NULL, "failed to stat(): %s", strerror( errno ) );
			}
			else
			{
				char *epoch = (char*)( cfile->data + cfile->size );
				if( epoch && epoch[0] == '|' && kahanaStrtoll( epoch + 1, 10 ) == finfo.st_mtime ){
					cached = TRUE;
				}
			}
		}
	}
	
	return cached;
}


// MARK:  FILE AND DIRECTORY
apr_status_t kahanaIOFStat( apr_pool_t *p, const char *path, apr_finfo_t *finfo, apr_int32_t wanted )
{
	return apr_stat( finfo, path, wanted, p );
}

// correct = APR_SUCCESS, incorrect = APR_NOTFOUND, nofile = APR_NOENT
apr_status_t kahanaIOPathTypeIs( apr_pool_t *p, const char *path, ... )
{
	apr_status_t rc;
	apr_finfo_t finfo = {0};
	
	if( !( rc = kahanaIOFStat( p, path, &finfo, APR_FINFO_TYPE ) ) )
	{
		apr_filetype_e type;
		va_list args;
		
		rc = APR_NOTFOUND;
		va_start( args, path );
		while( ( type = va_arg( args, apr_filetype_e ) ) )
		{
			if( finfo.filetype == type ){
				rc = APR_SUCCESS;
				break;
			}
		}
		va_end(args);
	}
	
	return rc;
}

apr_status_t kahanaIOReadDir( apr_pool_t *p, kDir_t **newDirObj, const char *path, apr_int32_t wanted, ... )
{
	apr_dir_t *dir = NULL;
	kDir_t *dirObj = NULL;
	apr_pool_t *sp = NULL;
	apr_status_t rc;
	
	if( ( rc = apr_dir_open( &dir, dirObj->path, p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to apr_dir_open(): %s", STRERROR_APR( rc ) );
	}
	else if( ( rc = kahanaMalloc( p, sizeof( kDir_t ), (void**)&dirObj, &sp ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
	}
	else
	{
		apr_filetype_e type;
		va_list types;
		
		dirObj->p = sp;
		dirObj->files = apr_array_make( sp, 0, sizeof( apr_finfo_t ) );
		dirObj->path = (const char*)apr_pstrdup( sp, path );
		*newDirObj = dirObj;
		
		do
		{
			apr_finfo_t finfo = { 0 };
			
			if( ( rc = apr_dir_read( &finfo, wanted, dir ) ) ){
				kahanaLogPut( NULL, NULL, "failed to apr_dir_read(): %s", STRERROR_APR( rc ) );
				break;
			}
			else
			{
				va_start( types, wanted );
				while( ( type = va_arg( types, apr_filetype_e ) ) )
				{
					if( finfo.filetype == type ){
						APR_ARRAY_PUSH( dirObj->files, apr_finfo_t ) = finfo;
						break;
					}
				}
				va_end( types );
			}
		
		} while( rc == APR_SUCCESS );
		
		rc = ( rc == APR_ENOENT ) ? APR_SUCCESS : rc;
		if( ( rc = apr_dir_close( dir ) ) ){
			kahanaLogPut( NULL, NULL, "failed to apr_dir_close(): %s", STRERROR_APR( rc ) );
		}
	}
	
	return rc;
}
int kahanaIODirIndex( kDir_t *dir )
{
	return dir->files->nelts;
}
apr_finfo_t kahanaIOFinfoAtDirIndex( kDir_t *dir, int idx )
{
	return APR_ARRAY_IDX( dir->files, idx, apr_finfo_t );
}

apr_status_t kahanaIOTempDir( apr_pool_t *p, const char **dir )
{
	apr_status_t rc;
	
	// create temp file
	if( ( rc = apr_temp_dir_get( dir, p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to apr_temp_dir_get(): %s", STRERROR_APR( rc ) );
	}
	
	return rc;
}

apr_status_t kahanaIOMakeDir( apr_pool_t *p, apr_fileperms_t perm, ... )
{
	apr_status_t rc = APR_SUCCESS;
	const char *path = NULL;
	va_list args;
	
	va_start( args, perm );
	while( ( path = va_arg( args, const char* ) ) )
	{
		// check dir
		if( ( rc = kahanaIOPathTypeIs( p, path, APR_DIR, 0 ) ) && 
			rc != APR_NOTFOUND && rc != APR_ENOENT ){
			break;
		}
		else if( rc == APR_ENOENT )
		{
			if( ( rc = apr_dir_make_recursive( path, perm, p ) ) ){
				kahanaLogPut( NULL, NULL, "failed to apr_dir_make_recursive(): %s", STRERROR_APR( rc ) );
				break;
			}
		}
	}
	va_end(args);
	
	return rc;
}


apr_status_t kahanaIOTempFile( apr_pool_t *p, apr_file_t **fp, apr_int32_t flags )
{
	int rc = APR_SUCCESS;
	const char *tmpdir = NULL;
	
	// create temp file
	if( ( rc = apr_temp_dir_get( &tmpdir, p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to apr_temp_dir_get(): %s", STRERROR_APR( rc ) );
	}
	else if( ( rc = apr_file_mktemp( fp, (char*)apr_pstrcat( p, tmpdir, "/XXXXXX", NULL ), flags, p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to apr_file_mktemp(): %s", STRERROR_APR( rc ) );
	}
	
	return rc;
}


apr_status_t kahanaIOMakeSymlink( apr_pool_t *p, ... )
{
	apr_status_t rc = APR_SUCCESS;
	const char *src = NULL;
	const char *sym = NULL;
	va_list args;
	
	va_start( args, p );
	while( ( src = va_arg( args, const char* ) ) &&
			( sym = va_arg( args, const char* ) ) )
	{
		// check src path
		if( ( rc = kahanaIOPathTypeIs( p, src, APR_REG, APR_DIR, APR_LNK, 0 ) ) && 
			rc != APR_NOTFOUND && rc != APR_ENOENT ){
			break;
		}
		else if( rc == APR_ENOENT ){
			continue;
		}
		
		// check sym path
		if( ( rc = kahanaIOPathTypeIs( p, sym, APR_LNK, 0 ) ) && 
			rc != APR_NOTFOUND && rc != APR_ENOENT ){
			break;
		}
		if( rc == APR_ENOENT && symlink( src, sym ) == -1 ){
			kahanaLogPut( NULL, NULL, "failed to symlink(): %s", strerror( rc ) );
			break;
		}
	}
	va_end(args);
	
	return rc;
}

apr_status_t kahanaIOReadFile( apr_pool_t *p, apr_file_t *file, char **out )
{
	apr_status_t rc;
	kBuffer_t *bo = NULL;
	
	if( ( rc = kahanaBufCreate( &bo, p, sizeof( char ), 0 ) ) == APR_SUCCESS )
	{
		char chunk[HUGE_STRING_LEN];
		apr_size_t size = HUGE_STRING_LEN;
		
		while( ( rc = apr_file_read( file, chunk, &size ) ) == APR_SUCCESS )
		{
			if( ( rc = kahanaBufStrNCat( bo, chunk, size ) ) ){
				break;
			}
		}
		
		if( rc && rc != APR_EOF ){
			kahanaLogPut( NULL, NULL, "failed to apr_file_read(): %s", STRERROR_APR( rc ) );
		}
		else if( ( rc = kahanaBufStrNCat( bo, chunk, size ) ) == APR_SUCCESS ){
			*out = apr_pstrdup( p, kahanaBufPtr( bo ) );
		}
		
		kahanaBufDestroy( bo );
	}
	
	return rc;
}

apr_status_t kahanaIOLoadFile( apr_pool_t *p, const char *path, size_t *size, bool *is_cache, void **data )
{
	apr_status_t rc = APR_SUCCESS;
	struct stat finfo;
	char *d = NULL;
	
	*size = 0;
	if( stat( path, &finfo ) == -1 ){
		rc = errno;
		kahanaLogPut( NULL, NULL, "failed to stat(): %s", strerror( rc ) );
	}
	else
	{
		apr_file_t *file = NULL;
		
		if( ( d = CacheFile( p, path, finfo.st_mtime, size ) ) ){
			*is_cache = TRUE;
			*size = finfo.st_size;
			*data = d;
		}
		else if( ( rc = apr_file_open( &file, path, APR_READ, APR_UREAD|APR_OS_DEFAULT, p ) ) ){
			kahanaLogPut( NULL, NULL, "failed to apr_file_open(): %s", STRERROR_APR( rc ) );
		}
		else
		{
			if( ( rc = kahanaIOReadFile( p, file, &d ) ) == APR_SUCCESS )
			{
				*is_cache = FALSE;
				*size = finfo.st_size;
				*data = d;
				rc = AddCache( path, data, finfo.st_size, finfo.st_mtime );
			}
			apr_file_close( file );
		}
	}
	
	return rc;
}

apr_status_t _kahanaIOInit( Kahana_t *kahana )
{
	apr_status_t rc;
	apr_pool_t *p;
	kIOCache_t *iocache;
	
	kahana->iocache = NULL;
	if( ( rc = kahanaMalloc( kahana->p, sizeof( kIOCache_t ), (void**)&iocache, &p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
	}
	else
	{
		iocache->p = p;
		iocache->store = FALSE;
		iocache->size = 0;
		iocache->hash = apr_hash_make( p );
		kahana->iocache = iocache;
	}
	
	return rc;
}

