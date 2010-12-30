/*
 *  kahana_buffer.h
 *
 *  Copyright 2006 Masatoshi Teruya All rights reserved.
 *
 */

#ifndef ___KAHANA_BUFFER___
#define ___KAHANA_BUFFER___

#include "kahana.h"

typedef struct kBuffer_t kBuffer_t;

apr_status_t kahanaBufCreate( kBuffer_t **newbo, apr_pool_t *p, size_t bytes, size_t nalloc );
void kahanaBufDestroy( kBuffer_t *bo );
int kahanaBufCompaction( kBuffer_t *bo );
const void *kahanaBufPtr( kBuffer_t *bo );
size_t kahanaBufBytes( kBuffer_t *bo );
size_t kahanaBufSize( kBuffer_t *bo );
size_t kahanaBufAllocSize( kBuffer_t *bo );
size_t kahanaBufNAllocSize( kBuffer_t *bo );
void kahanaBufSetNAllocSize( kBuffer_t *bo, size_t nalloc );


// MARK:  String
int kahanaBufStrCompaction( kBuffer_t *bo );
int kahanaBufStrSet( kBuffer_t *bo, const char *str );
int kahanaBufStrNSet( kBuffer_t *bo, const char *str, size_t len );
int kahanaBufStrShift( kBuffer_t *bo, size_t cur, size_t n, size_t padding );
int kahanaBufStrCat( kBuffer_t *bo, const char *str );
int kahanaBufStrNCat( kBuffer_t *bo, const char *str, size_t n );
int kahanaBufStrChrCat( kBuffer_t *bo, const char c );
int kahanaBufStrInsert( kBuffer_t *bo, const char *ins, size_t cur );
int kahanaBufStrReplace( kBuffer_t *bo, const char *rep, const char *str, size_t n );
int kahanaBufStrReplaceFromTo( kBuffer_t *bo, size_t from, size_t to, const char *str );

#endif

