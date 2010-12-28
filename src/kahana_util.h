#ifndef ___KAHANA_UTIL___
#define ___KAHANA_UTIL___

#include "kahana.h"

typedef enum {
	CHKSUM_STR = 1,
	CHKSUM_FILE
} kCheckSumType_e;

apr_status_t kahanaUtilRand( void *rnd, size_t size );
const char *kahanaUtilHMACSHA1( apr_pool_t *p, unsigned char *key, size_t ksize, const unsigned char *msg, size_t msize );
apr_status_t kahanaUtilChecksum( apr_pool_t *p, kCheckSumType_e type, const char **hex_digest, const char *msg, ... );

#endif
