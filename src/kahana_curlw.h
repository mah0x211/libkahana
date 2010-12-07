/*
 *  kahana_net.h
 *
 *  Created by Mah on 09/08/27.
 *  Copyright 2009 Masatoshi Teruya. All rights reserved.
 *
 */
 
#ifndef ___KAHANA_CURL_WRAP___
#define ___KAHANA_CURL_WRAP___

#include "kahana.h"

typedef struct {
	apr_pool_t *p;
	CURL *curl;
	CURLcode res;
	const char *url;
	apr_hash_t *query;
	apr_status_t ec;
	const char *errstr;
	struct kBuffer_t *rcvd;
} kCurlWrap_t;

apr_status_t kahanaCurlWrapCreate( kCurlWrap_t **newcw, apr_pool_t *p );
apr_status_t kahanaCurlWrapSetQuery( kCurlWrap_t *cw, const char *key, const char *val, bool escape );
apr_status_t kahanakCurlWrapRequest( kCurlWrap_t *cw, bool post, const char *url );

#endif

