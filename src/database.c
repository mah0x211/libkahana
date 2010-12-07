/*
*  dbd.c
*  mod_kahana
*
*  created by Mah on 09/09/12.
*  mod by Mah on 10/07/10.
*  copyright (c) masatoshi teruya all rights reserved.
*
*/

#include "kahana_database.h"
#include "kahana_private.h"


typedef struct {
	apr_pool_t *p;
	const char *cid;
	const apr_dbd_driver_t *driver;
	apr_dbd_t *handle;
	apr_hash_t *results;
	const char *params;
	const char *errstr;
} Dbd;

struct kDb_t {
	apr_pool_t *p;
	apr_hash_t *conn_h;
	apr_hash_t *qry_h;
};

struct kDbConn_t {
	apr_pool_t *p;
	const char *id;
	const char *params;
	apr_dbd_t *handle;
	const apr_dbd_driver_t *driver;
};

struct kDbTxn_t {
	apr_pool_t *p;
	kDbConn_t *conn;
	apr_dbd_transaction_t *txn;
};


#pragma mark STATIC
static kDb_t *GetDb( void ){
	Kahana_t *kahana = _kahanaGlobal();
	return kahana->database;
}
static kDbConn_t *GetDbConn( const char *cid ){
	kDb_t *dbh = GetDb();
	return (kDbConn_t*)apr_hash_get( dbh->conn_h, cid, APR_HASH_KEY_STRING );
}

static apr_status_t CheckDbConn( kDbConn_t *conn ){
	return apr_dbd_check_conn( conn->driver, conn->p, conn->handle );
}

static const char *GetDbError( kDbConn_t *conn, apr_status_t ec ){
	return ( conn ) ? apr_dbd_error( conn->driver, conn->handle, ec ) : kahanaLogErr2Str( ETYPE_APR, APR_EGENERAL );
}

#pragma mark CALLBACK
static apr_status_t cbDbConnCleanup( void *ctx )
{
	if( ctx )
	{
		kDb_t *dbh = GetDb();
		kDbConn_t *conn = (kDbConn_t*)ctx;
		
		apr_hash_set( dbh->conn_h, conn->id, APR_HASH_KEY_STRING, NULL );
		if( conn->driver && conn->handle )
		{
			apr_status_t rc = apr_dbd_close( conn->driver, conn->handle );
			if( rc ){
				kahanaLogPut( NULL, NULL, "failed to apr_dbd_close() reason %s",
								apr_dbd_error( conn->driver, conn->handle, rc ) );
			}
		}
	}
	
	return APR_SUCCESS;
}

static apr_status_t cbDbQryCleanup( void *ctx )
{
	kDbQuery_t *qry = (kDbQuery_t*)ctx;
	
	if( qry && qry->cols ){
		free( qry->cols );
	}
	
	return APR_SUCCESS;
}

static apr_status_t cbDbTxnCleanup( void *ctx )
{
	kDbTxn_t *txn = (kDbTxn_t*)ctx;
	
	if( txn && txn->txn )
	{
		apr_status_t rc = apr_dbd_transaction_end( txn->conn->driver, txn->p, txn->txn );
		
		if( rc ) {
			kahanaLogPut( NULL, NULL, "failed to apr_dbd_transaction_end() reason %s",
							apr_dbd_error( txn->conn->driver, txn->conn->handle, rc ) );
		}
	}
	
	return APR_SUCCESS;
}


#pragma mark GET
const char *kahanaDbError( const char *cid, apr_status_t ec )
{
	return GetDbError( GetDbConn( cid ), ec );
}

const char *kahanaDbDriverName( const char *cid )
{
	kDbConn_t *conn = GetDbConn( cid );
	return ( conn ) ? apr_dbd_name( conn->driver ) : NULL;
}

#pragma mark QUERY

apr_status_t kahanaDbQryNew( const char **qid, const char *cid, apr_pool_t *p, const char *query )
{
	apr_status_t rc = APR_ENOENT;
	kDb_t *dbh = GetDb();
	kDbConn_t *conn = GetDbConn( cid );
	
	if( conn )
	{
		kDbQuery_t *qry = NULL;
		apr_pool_t *sp = NULL;
		
		rc = kahanaMalloc( p, sizeof( kDbQuery_t ), (void**)&qry, &sp );
		if( rc == APR_SUCCESS )
		{
			if( ( rc = kahanaUniqueId( &qry->id, sp ) ) ||
				( rc = apr_dbd_prepare( conn->driver, sp, conn->handle, query, NULL, &qry->pstmt ) ) ){
				apr_pool_destroy( sp );
			}
			else
			{
				qry->p = sp;
				qry->conn = conn;
				qry->stmt = (const char*)apr_pstrdup( sp, query );
				qry->cols = NULL;
				qry->ncols = -1;
				qry->nrows = 0;
				qry->result = NULL;
				qry->cache = apr_hash_make( sp );
				apr_hash_set( dbh->qry_h, qry->id, APR_HASH_KEY_STRING, (void*)qry );
				*qid = qry->id;
				apr_pool_cleanup_register( sp, (void*)qry, cbDbQryCleanup, apr_pool_cleanup_null );
			}
		}
	}
	
	return rc;
}

void kahanaDbQryDelete( const char *qid )
{
	kDbQuery_t *qry = kahanaDbQry( qid );
	
	if( qry )
	{
		kDb_t *dbh = GetDb();
		
		apr_hash_set( dbh->qry_h, qry->id, APR_HASH_KEY_STRING, NULL );
		apr_pool_destroy( qry->p );
	}
}

kDbQuery_t *kahanaDbQry( const char *qid )
{
	kDb_t *dbh = GetDb();
	return (kDbQuery_t*)apr_hash_get( dbh->qry_h, qid, APR_HASH_KEY_STRING );
}

const apr_dbd_driver_t *kahanaDbQryDriver( kDbQuery_t *qry )
{
	return qry->conn->driver;
}

apr_dbd_t *kahanaDbQryDbd( kDbQuery_t *qry )
{
	return qry->conn->handle;
}

const char *kahanaDbQryResValForColAtRow( kDbQuery_t *qry, const char *colname, unsigned int rownum )
{
	const char *val = NULL;
	apr_dbd_row_t *row = NULL;
	int col = kfStoreGetIValForKey( dr->records, (const char*)apr_psprintf( p, "cols.%s", colname ) );
	
	// if no parsed yet
	if( qry->ncols == -1 && 
		( qry->ncols = apr_dbd_num_cols( qry->conn->driver, qry->result ) ) )
	{
		// return -1 if asynchronous
		qry->nrows = apr_dbd_num_tuples( qry->conn->driver, qry->result );
		qry->cols = (const char**)calloc( qry->ncols, sizeof( const char* ) );
		for( i = 0; i < qry->ncols; i++ ){
			asprintf( (char**)&qry->cols[i], "%s", apr_dbd_get_name( qry->conn->driver, qry->result, i ) );
		}
	}
	
	if( ( col >= 0 ) && 
		( apr_dbd_get_row( dr->dbd->driver, p, dr->result, &row, rownum ) == 0 ) )
	{
		if( ( val = (const char*)apr_pstrdup( p, apr_dbd_get_entry( dr->dbd->driver, row, col ) ) ) ){
			kfStoreSetValForKey( dr->records, val, (const char*)apr_psprintf( dr->p, "%s.%d", colname, rownum ) );
		}
	}
	
	return val;
}


#pragma mark TRANSACTION

// apr_dbd_error( conn->driver, conn->handle, rc ) );
apr_status_t kfDbTxnStart( kDbTxn_t **newtxn, const char *cid, apr_pool_t *p )
{
	apr_status_t rc = APR_ENOENT;
	kDbConn_t *conn = GetDbConn( cid );
	
	if( conn && ( rc = CheckDbConn( conn ) ) == APR_SUCCESS )
	{
		kDbTxn_t *txn = NULL;
		apr_pool_t *sp = NULL;
		
		rc = kahanaMalloc( p, sizeof( kDbTxn_t ), (void**)&txn, &sp );
		if( rc == APR_SUCCESS )
		{
			txn->p = sp;
			txn->conn = conn;
			txn->txn = NULL;
			if( ( rc = apr_dbd_transaction_start( conn->driver, sp, conn->handle, &txn->txn ) ) ){
				apr_pool_destroy( sp );
			}
			else {
				*newtxn = txn;
				apr_dbd_transaction_mode_set( conn->driver, txn->txn, APR_DBD_TRANSACTION_ROLLBACK );
				apr_pool_cleanup_register( sp, (void*)txn, cbDbTxnCleanup, apr_pool_cleanup_null );
			}
		}
	}
	
	return rc;
}

apr_status_t kahanaDbTxnEnd( kDbTxn_t *txn )
{
	apr_status_t rc = apr_dbd_transaction_end( txn->conn->driver, txn->p, txn->txn );
	
	apr_pool_destroy( txn->p );
	
	return rc;
}

apr_status_t kahanaDbTxnCommit( kDbTxn_t *txn )
{
	apr_dbd_transaction_mode_set( txn->conn->driver, txn->txn, APR_DBD_TRANSACTION_COMMIT );
	return kahanaDbTxnEnd( txn );
}

apr_status_t kfDbTxnRollback( kDbTxn_t *txn )
{
	apr_dbd_transaction_mode_set( txn->conn->driver, txn->txn, APR_DBD_TRANSACTION_ROLLBACK );
	return kahanaDbTxnEnd( txn );
}

apr_status_t kfDbTxnSavePt( kDbTxn_t *txn, const char *savept )
{
	apr_status_t rc;
	char *stmt = NULL;
	
	asprintf( &stmt, "SAVEPOINT %s", savept );
	rc = apr_dbd_query( txn->conn->driver, txn->conn->handle, NULL, stmt );
	free( stmt );
	
	return rc;
}

apr_status_t kfDbTxnSavePtRelease( kDbTxn_t *txn, const char *savept )
{
	apr_status_t rc;
	char *stmt = NULL;
	
	asprintf( &stmt, "RELEASE SAVEPOINT %s", savept );
	rc = apr_dbd_query( txn->conn->driver, txn->conn->handle, NULL, stmt );
	free( stmt );
	
	return rc;
}

apr_status_t kfDbTxnSavePtRollback( kDbTxn_t *txn, const char *savept )
{
	apr_status_t rc;
	char *stmt = NULL;
	
	asprintf( &stmt, "ROLLBACK TO SAVEPOINT %s", savept );
	rc = apr_dbd_query( txn->conn->driver, txn->conn->handle, NULL, stmt );
	free( stmt );
	
	return rc;
}


#pragma mark CONNECTION

apr_status_t kahanaDbConnect( const char *cid, const char *driver, const char *params )
{
	apr_status_t rc = APR_SUCCESS;
	kDbConn_t *conn = GetDbConn( cid );
	
	if( !conn )
	{
		kDb_t *dbh = GetDb();
		apr_pool_t *p = NULL;
		
		rc = kahanaMalloc( dbh->p, sizeof( kDbConn_t ), (void**)&conn, &p );
		if( rc == APR_SUCCESS )
		{
			const char *errstr = NULL;
			
			conn->p = p;
			conn->id = apr_pstrdup( p, cid );
			conn->params = NULL;
			conn->driver = NULL;
			conn->handle = NULL;
			
			if( ( rc = apr_dbd_get_driver( p, driver, &conn->driver ) ) ){
				kahanaLogPut( NULL, NULL, "failed to apr_dbd_get_driver() reason %s", kahanaLogErr2Str( ETYPE_APR, rc ) );
				apr_pool_destroy( p );
			}
			else if( ( rc = apr_dbd_open_ex( conn->driver, p, params, &conn->handle, &errstr ) ) ){
				kahanaLogPut( NULL, NULL, "failed to apr_dbd_open_ex() reason %s", errstr );
				apr_pool_destroy( p );
			}
			else {
				conn->params = apr_pstrdup( p, params );
				apr_pool_cleanup_register( p, (void*)conn, cbDbConnCleanup, apr_pool_cleanup_null );
				apr_hash_set( dbh->conn_h, conn->id, APR_HASH_KEY_STRING, (void*)conn );
			}
		}
	}
	
	return rc;
}

void kahanaDbDisconnect( const char *cid )
{
	kDbConn_t *conn = GetDbConn( cid );
	
	if( conn ){
		apr_pool_destroy( conn->p );
	}
}

apr_status_t kahanaDbConnReConn( const char *cid )
{
	apr_status_t rc = APR_ENOENT;
	kDbConn_t *conn = GetDbConn( cid );
	
	if( conn )
	{
		const char *errstr = NULL;
		
		if( ( rc = apr_dbd_open_ex( conn->driver, conn->p, conn->params, &conn->handle, &errstr ) ) ){
			kahanaLogPut( NULL, NULL, "failed to apr_dbd_open_ex() reason %s", errstr );
		}
	}
	
	return rc;
}

apr_status_t kahanaDbConnCheck( const char *cid )
{
	apr_status_t rc = APR_ENOENT;
	kDbConn_t *conn = GetDbConn( cid );
	
	if( conn ){
		rc = CheckDbConn( conn );
	}
	
	return rc;
}



#pragma mark Initialize
apr_status_t _kahanaDbInit( Kahana_t *kahana )
{
	apr_pool_t *p = NULL;
	apr_status_t rc = kahanaMalloc( kahana->p, sizeof( kDb_t ), (void**)&kahana->database, &p );
	
	if( rc == APR_SUCCESS ){
		kahana->database->p = p;
		kahana->database->conn_h = apr_hash_make( p );
		kahana->database->qry_h = apr_hash_make( p );
	}
	
	return rc;
}
