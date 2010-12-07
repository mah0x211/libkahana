#ifndef ___KAHANA_NET___
#define ___KAHANA_NET___

#include "kahana.h"

/* CIDR */
typedef struct {
	uint32_t mask;
	uint32_t from;
	uint32_t to;
} kCIDR_t;

apr_status_t kahanaNetCIDR( apr_pool_t *p, kCIDR_t *cidr_def, const char *cidr );

#endif
