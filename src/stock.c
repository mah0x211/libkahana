/*
 *  stock.c
 *  mod_kahana
 *
 *  Created by Maasatoshi Teruya on 07/08/12.
 *  Copyright 2006 Masatoshi Teruya All rights reserved.
 *
 */
#include "kahana_stock.h"


struct kStock_t {
	apr_pool_t *p;
	apr_hash_t *items;
};


// MARK:  CALLBACK
static apr_status_t ItemCleanup( void *ctx )
{
	if( ctx )
	{
		kStockItem_t *item = (kStockItem_t*)ctx;
		
		if( item->cleanup ){
			item->cleanup( item->key, item->val, item->as );
		}
		else if( item->as != STOCK_AS_PTR ){
			free( item->val );
		}
		apr_hash_set( item->s->items, item->key, APR_HASH_KEY_STRING, NULL );
	}
	
	return APR_SUCCESS;
}

// MARK:  STATIC

// key = "[^\\w\\.]";
static apr_status_t SetValForKeyAs( kStock_t *s, const char *key, void *val, kStockValType_e as, kStockCbItemCleanup cleanup )
{
	apr_status_t rc = APR_SUCCESS;
	kStockItem_t *sibling = (kStockItem_t*)apr_hash_get( s->items, key, APR_HASH_KEY_STRING );
	kStockItem_t *item = NULL;
	apr_pool_t *p = NULL;
	
	if( ( rc = kahanaMalloc( ( sibling ) ? sibling->p : s->p, sizeof( kStockItem_t ), (void**)&item, &p ) ) == APR_SUCCESS )
	{
		item->p = p;
		item->s = s;
		item->key = apr_pstrdup( p, key );
		item->val = NULL;
		item->cleanup = ( cleanup ) ? cleanup : NULL;
		item->next = NULL;
		item->last = item;
		
		errno = 0;
		switch ( as )
		{
			case STOCK_AS_PTR:
				item->val = val;
			break;
			
			case STOCK_AS_CHAR:
				// item->val = apr_pstrdup( p, (char*)val );
				asprintf( (char**)&item->val, "%s", (char*)val );
			break;
			
			case STOCK_AS_INT:
				if( ( item->val = malloc( sizeof( int ) ) ) ){
					(*(int*)item->val) = (*(int*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%d", (*(int*)val) );
			break;

			case STOCK_AS_UINT:
				if( ( item->val = malloc( sizeof( unsigned int ) ) ) ){
					(*(unsigned int*)item->val) = (*(unsigned int*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%u", (*(unsigned int*)val) );
			break;
			
			case STOCK_AS_SHORT:
				if( ( item->val = malloc( sizeof( short ) ) ) ){
					(*(short*)item->val) = (*(short*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%hd", (*(short*)val) );
			break;

			case STOCK_AS_USHORT:
				if( ( item->val = malloc( sizeof( unsigned short ) ) ) ){
					(*(unsigned short*)item->val) = (*(unsigned short*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%hu", (*(unsigned short*)val) );
			break;
			
			case STOCK_AS_LONG:
				if( ( item->val = malloc( sizeof( long ) ) ) ){
					(*(long*)item->val) = (*(long*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%ld", (*(long*)val) );
			break;
			
			case STOCK_AS_ULONG:
				if( ( item->val = malloc( sizeof( unsigned long ) ) ) ){
					(*(unsigned long*)item->val) = (*(unsigned long*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%lu", (*(unsigned long*)val) );
			break;
			
			case STOCK_AS_LLONG:
				if( ( item->val = malloc( sizeof( long long ) ) ) ){
					(*(long long*)item->val) = (*(long long*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%lld", (*(long long*)val) );
			break;
			
			case STOCK_AS_ULLONG:
				if( ( item->val = malloc( sizeof( unsigned long long ) ) ) ){
					(*(unsigned long long*)item->val) = (*(unsigned long long*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%llu", (*(unsigned long long*)val) );
			break;
			
			case STOCK_AS_DOUBLE:
				if( ( item->val = malloc( sizeof( double ) ) ) ){
					(*(double*)item->val) = (*(double*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%f", (*(double*)val) );
			break;
			
			case STOCK_AS_LDOUBLE:
				if( ( item->val = malloc( sizeof( long double ) ) ) ){
					(*(long double*)item->val) = (*(long double*)val);
				}
				// bytes = asprintf( (char**)&item->val, "%Lf", (*(long double*)val) );
			break;
		}
		
		// failed
		if( errno ){
			apr_pool_destroy( p );
			rc = errno;
		}
		else
		{
			apr_pool_cleanup_register( p, (void*)item, ItemCleanup, apr_pool_cleanup_null );
			if( sibling ){
				sibling->last->next = item;
				sibling->last = item;
			}
			else{
				apr_hash_set( s->items, item->key, APR_HASH_KEY_STRING, (void*)item );
			}
		}
	}
	
	return rc;
}

static apr_status_t GetValForKeyAs( kStock_t *s, void *val, const char *key, kStockValType_e as, void *null )
{
	apr_status_t rc = APR_SUCCESS;
	kStockItem_t *item = NULL;
	
	// invalid stock or key-value
	if( !s || !key || key[0] == '\0' ){
		rc = APR_EINVAL;
	}
	// no entry
	else if( !( item = (kStockItem_t*)apr_hash_get( s->items, key, APR_HASH_KEY_STRING ) ) || as != item->as ){
		rc = APR_ENOENT;
	}
	else
	{
		switch ( as )
		{
			case STOCK_AS_PTR:
				(*(void**)val) = ( item->val ) ? item->val : null;
			break;
			
			case STOCK_AS_CHAR:
				(*(char**)val) = ( item->val ) ? (char*)item->val : (char*)null;
			break;

			case STOCK_AS_INT:
				(*(int*)val) = ( item->val ) ? (*(int*)item->val) : (*(int*)null);
			break;
			
			case STOCK_AS_UINT:
				(*(unsigned int*)val) = ( item->val ) ? (*(unsigned int*)item->val ) : (*(unsigned int*)null);
			break;

			case STOCK_AS_SHORT:
				(*(short*)val) = ( item->val ) ? (*(short*)item->val) : (*(short*)null);
			break;
			
			case STOCK_AS_USHORT:
				(*(unsigned short*)val) = ( item->val ) ? (*(unsigned short*)item->val) : (*(unsigned short*)null);
			break;
			
			case STOCK_AS_LONG:
				(*(long*)val) = ( item->val ) ? (*(long*)item->val) : (*(long*)null);
			break;
			
			case STOCK_AS_ULONG:
				(*(unsigned long*)val) = ( item->val ) ? (*(unsigned long*)item->val) : (*(unsigned long*)null);
			break;
			
			case STOCK_AS_LLONG:
				(*(long long*)val) = ( item->val ) ? (*(long long*)item->val) : (*(long long*)null);
			break;
			
			case STOCK_AS_ULLONG:
				(*(unsigned long long*)val) = ( item->val ) ? (*(unsigned long long*)item->val) : (*(unsigned long long*)null);
			break;
			
			case STOCK_AS_DOUBLE:
				(*(double*)val) = ( item->val ) ? (*(double*)item->val) : (*(double*)null);
			break;
			
			case STOCK_AS_LDOUBLE:
				(*(long double*)val) = ( item->val ) ? (*(long double*)item->val) : (*(long double*)null);
			break;
		}
	}
	
	return rc;
}

static apr_status_t RemoveItem( apr_pool_t *p, kStockItem_t *item, void *udata )
{
	apr_pool_destroy( item->p );
	return APR_SUCCESS;
}


// MARK:  CREATE
apr_status_t kahanaStockCreate( kStock_t **newstock, apr_pool_t *p )
{
	apr_pool_t *sp = NULL;
	kStock_t *s = NULL;
	apr_status_t rc = kahanaMalloc( p, sizeof( kStock_t ), (void**)&s, &sp );
	
	if( rc == APR_SUCCESS ){
		s->p = sp;
		s->items = apr_hash_make( sp );
	}
	
	return rc;
}

void kahanaStockDestroy( kStock_t *s )
{
	if( s ){
		apr_pool_destroy( s->p );
	}
}

// MARK:  PROPERTY
bool kahanaStockHasKey( kStock_t *s, const char *key )
{
	return ( s && key && key[0] != '\0' && apr_hash_get( s->items, key, APR_HASH_KEY_STRING ) );
}

unsigned int kahanaStockNumberOfItems( kStock_t *s, const char *key )
{
	unsigned int nitem = 0;
	
	if( s )
	{
		if( key && key[0] != '\0' )
		{
			apr_pool_t *p = NULL;
			apr_status_t rc = apr_pool_create( &p, NULL );
			
			if( rc == APR_SUCCESS )
			{
				apr_hash_index_t *idx = NULL;
				const char *ikey;
				
				for( idx = apr_hash_first( p, s->items ); idx; idx = apr_hash_next( idx ) )
				{
					apr_hash_this( idx, (const void**)&ikey, NULL, NULL );
					if( kahanaStrCmpHead( ikey, key ) ){
						nitem++;
					}
				}
				apr_pool_destroy( p );
			}
		}
		else {
			nitem = apr_hash_count( s->items );
		}
	}
	
	return nitem;
}

// MARK:  SET
// set as void*
apr_status_t kahanaStockSetValForKey( kStock_t *s, void *val, const char *key, kStockCbItemCleanup cleanup ){
	return SetValForKeyAs( s, key, val, STOCK_AS_PTR, cleanup );
}
// set as char*
apr_status_t kahanaStockSetCValForKey( kStock_t *s, const char *val, const char *key ){
	return SetValForKeyAs( s, key, (void*)val, STOCK_AS_CHAR, NULL );
}
// set as int
apr_status_t kahanaStockSetIValForKey( kStock_t *s, int val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_INT, NULL );
}
// set as unsigned int
apr_status_t kahanaStockSetUIValForKey( kStock_t *s, unsigned int val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_UINT, NULL );
}
// set as short
apr_status_t kahanaStockSetSValForKey( kStock_t *s, short val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_SHORT, NULL );
}
// set as unsigned short
apr_status_t kahanaStockSetUSValForKey( kStock_t *s, unsigned short val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_USHORT, NULL );
}
// set as long
apr_status_t kahanaStockSetLValForKey( kStock_t *s, long val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_LONG, NULL );
}
// set as unsigned long
apr_status_t kahanaStockSetULValForKey( kStock_t *s, unsigned long val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_ULONG, NULL );
}
// set as long long
apr_status_t kahanaStockSetLLValForKey( kStock_t *s, long long val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_LLONG, NULL );
}
// set as unsinged lon long
apr_status_t kahanaStockSetULLValForKey( kStock_t *s, unsigned long long val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_ULLONG, NULL );
}
// set as double
apr_status_t kahanaStockSetDValForKey( kStock_t *s, double val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_DOUBLE, NULL );
}
// set as long double
apr_status_t kahanaStockSetLDValForKey( kStock_t *s, long double val, const char *key ){
	return SetValForKeyAs( s, key, (void*)&val, STOCK_AS_LDOUBLE, NULL );
}


// MARK:  GET
// get as void*
apr_status_t kahanaStockGetValForKey( kStock_t *s, void **val, const char *key, void *null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_PTR, null );
}
// get as const char*
apr_status_t kahanaStockGetCValForKey( kStock_t *s, const char **val, const char *key, const char *null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_CHAR, (void*)null );
}
// get as int
apr_status_t kahanaStockGetIValForKey( kStock_t *s, int *val, const char *key, int null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_INT, (void*)&null );
}
// get as unsigned int
apr_status_t kahanaStockGetUIValForKey( kStock_t *s, unsigned int *val, const char *key, unsigned int null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_UINT, (void*)&null );
}
// get as short
apr_status_t kahanaStockGetSValForKey( kStock_t *s, short *val, const char *key, short null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_SHORT, (void*)&null );
}
// get as unsigned short
apr_status_t kahanaStockGetUSValForKey( kStock_t *s, unsigned short *val, const char *key, unsigned short null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_USHORT, (void*)&null );
}
// get as long
apr_status_t kahanaStockGetLValForKey( kStock_t *s, long *val, const char *key, long null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_LONG, (void*)&null );
}
// get as unsigned long
apr_status_t kahanaStockGetULValForKey( kStock_t *s, unsigned long *val, const char *key, unsigned long null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_ULONG, (void*)&null );
}
// get as long long
apr_status_t kahanaStockGetLLValForKey( kStock_t *s, long long *val, const char *key, long null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_LLONG, (void*)&null );
}
// get as unsigned long long
apr_status_t kahanaStockGetULLValForKey( kStock_t *s, unsigned long long *val, const char *key, unsigned long long null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_ULLONG, (void*)&null );
}
// get as double
apr_status_t kahanaStockGetDValForKey( kStock_t *s, double *val, const char *key, double null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_DOUBLE, (void*)&null );
}
// get as long double
apr_status_t kahanaStockGetUDValForKey( kStock_t *s, long double *val, const char *key, long double null ){
	return GetValForKeyAs( s, (void*)val, key, STOCK_AS_LDOUBLE, (void*)&null );
}

// traverse
apr_status_t kahanaStockTraverseForKey( kStock_t *s, const char *key, kStockCbTraverse callback, void *udata )
{
	apr_status_t rc = APR_SUCCESS;
	apr_pool_t *p = NULL;
	
	if( s && key && key[0] != '\0' && callback && 
		( rc = apr_pool_create( &p, NULL ) ) == APR_SUCCESS )
	{
		apr_hash_index_t *idx = NULL;
		kStockItem_t *item = NULL;
		
		for( idx = apr_hash_first( p, s->items ); idx; idx = apr_hash_next( idx ) )
		{
			item = NULL;
			apr_hash_this( idx, NULL, NULL, (void*)&item );
			if( kahanaStrCmpHead( item->key, key ) )
			{
				if( ( rc = callback( p, item, udata ) ) ){
					break;
				}
			}
		}
	
		apr_pool_destroy( p );
	}
	
	return rc;
}

// MARK:  NODE CONTROL
apr_status_t kahanaStockRemoveNodeForKey( kStock_t *s, const char *key )
{
	apr_status_t rc = APR_SUCCESS;
	
	if( s && key ){
		rc = kahanaStockTraverseForKey( s, key, RemoveItem, NULL );
	}
	
	return rc;
}
