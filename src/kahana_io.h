/*
 *  dictionary.h
 *  mod_kahana
 *
 *  Created by Mah on 06/12/16.
 *  Copyright 2006 Masatoshi Teruya All rights reserved.
 *
 */

#ifndef ___KAHANA_IO___
#define ___KAHANA_IO___

#include "kahana.h"

typedef struct {
	apr_pool_t *p;
	const char *path;
	apr_array_header_t *files;
} kDir_t;

apr_status_t kahanaIOFStat( apr_pool_t *p, const char *path, apr_finfo_t *finfo, apr_int32_t wanted );
// correct = APR_SUCCESS, incorrect = APR_NOTFOUND, nofile = APR_NOENT
apr_status_t kahanaIOPathTypeIs( apr_pool_t *p, const char *path, ... );
bool kahanaIOIsCached( const char *path );

apr_status_t kahanaIOReadDir( apr_pool_t *p, kDir_t **dirObj, const char *path, apr_int32_t wanted, ... );
int kahanaIODirIndex( kDir_t *dir );
apr_finfo_t kahanaIOFinfoAtDirIndex( kDir_t *dir, int idx );
apr_status_t kahanaIOTempDir( apr_pool_t *p, const char **dir );
apr_status_t kahanaIOMakeDir( apr_pool_t *p, apr_fileperms_t perm, ... );

apr_status_t kahanaIOLoadFile( apr_pool_t *p, const char *path, size_t *size, bool *is_cache, void **data );
apr_status_t kahanaIOReadFile( apr_pool_t *p, apr_file_t *file, char **out );
apr_status_t kahanaIOTempFile( apr_pool_t *p, apr_file_t **fp, apr_int32_t flags );
apr_status_t kahanaIOMakeSymlink( apr_pool_t *p, ... );

#endif
