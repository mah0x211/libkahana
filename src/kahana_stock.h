/*
 *  stock.h
 *  mod_kahana
 *
 *  Created by Mah on 07/08/21.
 *  Copyright 2007 Masatoshi Teruya All rights reserved.
 *
 CFLAGS="$(CFLAGS) -Wc,-Wall"
 */

#ifndef ___KAHANA_STOCK___
#define ___KAHANA_STOCK___
#include "kahana.h"

typedef enum {
	STOCK_AS_PTR = 1,
	STOCK_AS_CHAR,
	STOCK_AS_INT,
	STOCK_AS_UINT,
	STOCK_AS_SHORT,
	STOCK_AS_USHORT,
	STOCK_AS_LONG,
	STOCK_AS_ULONG,
	STOCK_AS_LLONG,
	STOCK_AS_ULLONG,
	STOCK_AS_DOUBLE,
	STOCK_AS_LDOUBLE
} kStockValType_e;

typedef enum {
	FROM_HDF = 1
} kStockCreateFrom_e;


// CALLBACK
typedef apr_status_t (*kStockCbItemCleanup)( const char *key, void *val, kStockValType_e as );

// STRUCTURE
typedef struct kStock_t kStock_t;
typedef struct kStockItem_t kStockItem_t;
struct kStockItem_t {
	kStock_t *s;
	apr_pool_t *p;
	const char *key;
	void *val;
	kStockValType_e as;
	kStockCbItemCleanup cleanup;
	kStockItem_t *next;
	kStockItem_t *last;
};

// CREATE
apr_status_t kahanaStockCreate( kStock_t **newstock, apr_pool_t *p );
void kahanaStockDestroy( kStock_t *s );

// PROPERTY
bool kahanaStockHasKey( kStock_t *s, const char *key );
unsigned int kahanaStockNumberOfItems( kStock_t *s, const char *key );

// SET
// set as void*
apr_status_t kahanaStockSetValForKey( kStock_t *s, void *val, const char *key, kStockCbItemCleanup cleanup );
apr_status_t kahanaStockSetCValForKey( kStock_t *s, const char *val, const char *key );
apr_status_t kahanaStockSetIValForKey( kStock_t *s, int val, const char *key );
apr_status_t kahanaStockSetUIValForKey( kStock_t *s, unsigned int val, const char *key );
apr_status_t kahanaStockSetSValForKey( kStock_t *s, short val, const char *key );
apr_status_t kahanaStockSetUSValForKey( kStock_t *s, unsigned short val, const char *key );
apr_status_t kahanaStockSetLValForKey( kStock_t *s, long val, const char *key );
apr_status_t kahanaStockSetULValForKey( kStock_t *s, unsigned long val, const char *key );
apr_status_t kahanaStockSetLLValForKey( kStock_t *s, long long val, const char *key );
apr_status_t kahanaStockSetULLValForKey( kStock_t *s, unsigned long long val, const char *key );
apr_status_t kahanaStockSetDValForKey( kStock_t *s, double val, const char *key );
apr_status_t kahanaStockSetLDValForKey( kStock_t *s, long double val, const char *key );

// GET
apr_status_t kahanaStockGetValForKey( kStock_t *s, void **val, const char *key, void *null );
apr_status_t kahanaStockGetCValForKey( kStock_t *s, const char **val, const char *key, const char *null );
apr_status_t kahanaStockGetIValForKey( kStock_t *s, int *val, const char *key, int null );
apr_status_t kahanaStockGetUIValForKey( kStock_t *s, unsigned int *val, const char *key, unsigned int null );
apr_status_t kahanaStockGetSValForKey( kStock_t *s, short *val, const char *key, short null );
apr_status_t kahanaStockGetUSValForKey( kStock_t *s, unsigned short *val, const char *key, unsigned short null );
apr_status_t kahanaStockGetLValForKey( kStock_t *s, long *val, const char *key, long null );
apr_status_t kahanaStockGetULValForKey( kStock_t *s, unsigned long *val, const char *key, unsigned long null );
apr_status_t kahanaStockGetLLValForKey( kStock_t *s, long long *val, const char *key, long null );
apr_status_t kahanaStockGetULLValForKey( kStock_t *s, unsigned long long *val, const char *key, unsigned long long null );
apr_status_t kahanaStockGetDValForKey( kStock_t *s, double *val, const char *key, double null );
apr_status_t kahanaStockGetUDValForKey( kStock_t *s, long double *val, const char *key, long double null );

// TRAVERSE
// CALLBACK
typedef apr_status_t (*kStockCbTraverse)( apr_pool_t *p, kStockItem_t *item, void *udata );
apr_status_t kahanaStockTraverseForKey( kStock_t *s, const char *key, kStockCbTraverse callback, void *udata );

// REMOVE
apr_status_t kahanaStockRemoveNodeForKey( kStock_t *s, const char *key );


#endif
