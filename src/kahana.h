#ifndef ___KAHANA___
#define ___KAHANA___

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <execinfo.h>
#include <arpa/inet.h>
#include <curl/curl.h>

#define ONIG_ESCAPE_REGEX_T_COLLISION
#include "oniguruma.h"
#include "ClearSilver.h"
#include "iconv.h"

#include "apr.h"
#include "apr_getopt.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_time.h"
#include "apr_base64.h"
#include "apr_file_info.h"
#include "apr_file_io.h"
#include "apr_thread_pool.h"
#include "apr_hash.h"
#include "apr_tables.h"
#include "apr_dso.h"
#include "apr_dbd.h"
#include "apr_sha1.h"
#include "apr_mmap.h"

#include "kahana_log.h"
#include "kahana_util.h"
#include "kahana_buffer.h"
#include "kahana_cmd.h"
#include "kahana_string.h"
#include "kahana_io.h"
#include "kahana_geo.h"
#include "kahana_stock.h"
#include "kahana_net.h"
#include "kahana_module.h"
#include "kahana_socket.h"
#include "kahana_curlw.h"
//#include "kahana_database.h"

apr_status_t kahanaInitilize( void );
void kahanaTerminate( void );
apr_status_t kahanaMalloc( apr_pool_t *p, size_t size, void **newobj, apr_pool_t **newp );
const char **kahanaStackTrace( unsigned int level );

#define STRERROR_APR(ec)({\
	char _strbuf[HUGE_STRING_LEN]; \
	apr_strerror( ec, _strbuf, HUGE_STRING_LEN ); \
})


#endif
