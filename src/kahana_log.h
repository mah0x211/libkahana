#ifndef ___KAHANA_LOG___
#define ___KAHANA_LOG___

#include "kahana.h"

typedef struct kLogItem_t kLogItem_t;
struct kLogItem_t {
	apr_pool_t *p;
	apr_time_t timestamp;
	const char *file;
	int line;
	const char *func;
	const char *errstr;
	kLogItem_t *prev;
	kLogItem_t *next;
};

apr_status_t _kahanaLogItemCreate( kLogItem_t **newitem, apr_pool_t *p, const char *file, int line, const char *func, const char *errstr );
apr_status_t _kahanaLogPut( const char *label, apr_pool_t *p, const char *file, int line, const char *func, const char *errstr );
#define kahanaLogPut( label, p, fmt, ... )({ \
	char *log; \
	asprintf( &log, fmt, ##__VA_ARGS__ ); \
	_kahanaLogPut( label, p, __FILE__, __LINE__, __func__, log ); \
})

#define kahanaLogItemCreate( newitem, p, fmt, ... )({ \
	char *log; \
	asprintf( &log, fmt, ##__VA_ARGS__ ); \
	_kahanaLogItemCreate( newitem, p, __FILE__, __LINE__, __func__, log ); \
})

const char *kahanaLogNEOERR2Str( NEOERR *nerr, bool trace, char *buf, size_t bufsize );
typedef apr_status_t (*CB_LOGOUTPUT)( apr_pool_t *p, apr_time_t timestamp, const char *file, int line, const char *func, const char *errstr );
apr_status_t kahanaLogOutput( const char *label, CB_LOGOUTPUT callback );

#endif

