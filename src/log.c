#include "kahana_log.h"
#include "kahana_private.h"


typedef struct {
	apr_pool_t *p;
	const char *label;
	unsigned int nitem;
	kLogItem_t *item;
	kLogItem_t *last;
	FILE *out;
} kLog_t;

// MARK:  Static
static apr_pool_t *GetRootPool( void )
{
	Kahana_t *kahana = _kahanaGlobal();
	return kahana->p;
}
static kLog_t *GetLog( const char *label )
{
	Kahana_t *kahana = _kahanaGlobal();
	
	kLog_t *log = (kLog_t*)apr_hash_get( kahana->logs, label, APR_HASH_KEY_STRING );
	
	return log;
}

static void SetLog( kLog_t *log )
{
	if( log ){
		Kahana_t *kahana = _kahanaGlobal();
		apr_hash_set( kahana->logs, log->label, APR_HASH_KEY_STRING, (void*)log );
	}
}

static bool IsDebug( void )
{
	Kahana_t *kahana = _kahanaGlobal();
	return kahana->debug;
}

// MARK:  Method
/*
int NEOERR2ERRNO( int neo_errno )
{
	int rc = APR_EGENERAL;
	
	// case NERR_PASS:
	// case NERR_ASSERT:
	// case NERR_DB:
	// case NERR_EXISTS:
	if( neo_errno == NERR_NOT_FOUND ){
		rc = ENOENT;
	}
	else if( neo_errno == NERR_DUPLICATE ){
		rc = EEXIST;
	}
	else if( neo_errno == NERR_NOMEM ){
		rc = ENOMEM;
	}
	else if( neo_errno == NERR_PARSE ){
		rc = EINVAL;
	}
	else if( neo_errno == NERR_OUTOFRANGE ){
		rc = EFAULT;
	}
	else if( neo_errno == NERR_SYSTEM || neo_errno == NERR_IO || neo_errno == NERR_LOCK )
	{
		rc = errno;
	}
	
	return rc;
}

	// case NERR_PASS:
	// case NERR_ASSERT:
	// case NERR_DB:
	// case NERR_EXISTS:
	if( ec == NERR_NOT_FOUND ){
		str = strerror( ENOENT );
	}
	else if( ec == NERR_DUPLICATE ){
		str = strerror( EEXIST );
	}
	else if( ec == NERR_NOMEM ){
		str = strerror( ENOMEM );
	}
	else if( ec == NERR_PARSE ){
		str = strerror( EINVAL );
	}
	else if( ec == NERR_OUTOFRANGE ){
		str = strerror( EFAULT );
	}
	else if( ec == NERR_SYSTEM || ec == NERR_IO || ec == NERR_LOCK ){
		str = strerror( errno );
	}
	else {
		char strbuf[HUGE_STRING_LEN];
	}
*/
const char *kahanaLogNEOERR2Str( NEOERR *nerr, bool trace, char *buf, size_t bufsize )
{
	STRING str;
	
	string_init( &str );
	if( trace ){
		nerr_error_traceback( nerr, &str );
	}
	else {
		nerr_error_string( nerr, &str );
	}
	
	memcpy( buf, str.buf, ( str.len > bufsize ) ? bufsize : str.len );
	string_clear( &str );
	
	return buf;
}


apr_status_t kahanaLogOutput( const char *label, CB_LOGOUTPUT callback )
{
	kLog_t *log = GetLog( ( label ) ? label : "default" );
	apr_status_t rc = APR_SUCCESS;
	
	if( log )
	{
		kLogItem_t *item = NULL;
		
		// custom output
		if( callback )
		{
			for( item = log->item; item; item = item->next )
			{
				if( ( rc = callback( log->p, item->timestamp, item->file, item->line, item->func, item->errstr ) ) != APR_SUCCESS ){
					break;
				}
			}
		}
		// default output
		else
		{
			char buf[APR_RFC822_DATE_LEN];
			bool debug = IsDebug();
			
			for( item = log->item; item; item = item->next )
			{
				if( ( rc = apr_rfc822_date( (char*)buf, item->timestamp ) ) ){
					fprintf( log->out, "[%s] ", STRERROR_APR( rc ) );
				}
				else {
					fprintf( log->out, "[%s] ", buf );
				}
				
				if( debug ){
					fprintf( log->out, "[%s:%d:%s] %s\n", item->file, item->line, item->func, item->errstr );
				}
				else {
					fprintf( log->out, "%s\n", item->errstr );
				}
			}
		}
	}
	
	return rc;
}


apr_status_t _kahanaLogItemCreate( kLogItem_t **newitem, apr_pool_t *p, const char *file, int line, const char *func, const char *errstr )
{
	apr_pool_t *sp;
	kLogItem_t *item = NULL;
	apr_status_t rc = kahanaMalloc( p, sizeof( kLogItem_t ), (void**)&item, &sp );
	
	if( rc == APR_SUCCESS )
	{
		*newitem = item;
		item->p = sp;
		item->timestamp = apr_time_now();
		item->file = apr_pstrdup( sp, file );
		item->line = line;
		item->func = apr_pstrdup( sp, func );
		item->errstr = NULL;
		item->prev = NULL;
		item->next = NULL;
		
		if( errstr ){
			item->errstr = apr_pstrdup( sp, errstr );
			free( (void*)errstr );
		}
	}
	
	return rc;
}


apr_status_t _kahanaLogPut( const char *label, apr_pool_t *p, const char *file, int line, const char *func, const char *errstr )
{
	apr_status_t rc = APR_SUCCESS;
	kLog_t *log = NULL;
	kLogItem_t *item = NULL;
	
	if( !label ){
		label = "default";
	}
	log = GetLog( label );
	
	// create Log_t if is null
	if( !log )
	{
		apr_pool_t *sp = NULL;
		
		if( !p ){
			p = GetRootPool();
		}
		// alloc
		if( ( rc = kahanaMalloc( p, sizeof( kLog_t ), (void**)&log, &sp ) ) ){
			fprintf( stderr, "[%s:%d:%s] failed to apr_pcalloc() reason: %s", __FILE__, __LINE__, __func__, STRERROR_APR( rc ) );
		}
		else {
			log->p = sp;
			log->label = apr_pstrdup( p, label );
			log->nitem = 0;
			log->item = NULL;
			log->last = NULL;
			log->out = stderr;
			SetLog( log );
		}
	}
	
	// create new LogItem_t
	if( rc == APR_SUCCESS &&
		( rc = _kahanaLogItemCreate( &item, log->p, file, line, func, errstr ) ) == APR_SUCCESS )
	{
		log->nitem++;
		if( !log->item ){
			log->item = item;
		}
		else {
			item->prev = log->last;
			log->last->next = item;
		}
		log->last = item;
	}
	
	return rc;
}
