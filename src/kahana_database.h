#ifndef ___KAHANA_DATABASE___
#define ___KAHANA_DATABASE___

#include "kahana.h"

#define KDBQRY_MAX_ARGS	255

typedef struct kDbConn_t kDbConn_t;
typedef struct kDbTxn_t kDbTxn_t;
typedef struct {
	apr_pool_t *p;
	kDbConn_t *conn;
	const char *id;
	const char *stmt;
	int ncols;
	int nrows;
	apr_dbd_prepared_t *pstmt;
	apr_dbd_results_t *result;
	kStore_t *cache;
} kDbQuery_t;


/** apr_dbd_open_ex: open a connection to a backend
 *
 *  @remarks PostgreSQL: the params is passed directly to the PQconnectdb()
 *  function (check PostgreSQL documentation for more details on the syntax).
 *  @remarks SQLite2: the params is split on a colon, with the first part used
 *  as the filename and second part converted to an integer and used as file
 *  mode.
 *  @remarks SQLite3: the params is passed directly to the sqlite3_open()
 *  function as a filename to be opened (check SQLite3 documentation for more
 *  details).
 *  @remarks Oracle: the params can have "user", "pass", "dbname" and "server"
 *  keys, each followed by an equal sign and a value. Such key/value pairs can
 *  be delimited by space, CR, LF, tab, semicolon, vertical bar or comma.
 *  @remarks MySQL: the params can have "host", "port", "user", "pass",
 *  "dbname", "sock", "flags" "fldsz", "group" and "reconnect" keys, each
 *  followed by an equal sign and a value. Such key/value pairs can be
 *  delimited by space, CR, LF, tab, semicolon, vertical bar or comma. For
 *  now, "flags" can only recognise CLIENT_FOUND_ROWS (check MySQL manual for
 *  details). The value associated with "fldsz" determines maximum amount of
 *  memory (in bytes) for each of the fields in the result set of prepared
 *  statements. By default, this value is 1 MB. The value associated with
 *  "group" determines which group from configuration file to use (see
 *  MYSQL_READ_DEFAULT_GROUP option of mysql_options() in MySQL manual).
 *  Reconnect is set to 1 by default (i.e. true).
 *  @remarks FreeTDS: the params can have "username", "password", "appname",
 *  "dbname", "host", "charset", "lang" and "server" keys, each followed by an
 *  equal sign and a value.
 */
apr_status_t kahanaDbConnect( const char *conn, const char *driver, const char *params );
void kahanaDbDisconnect( const char *cid );
apr_status_t kahanaDbConnReConn( const char *cid );
apr_status_t kahanaDbConnCheck( const char *cid );

const char *kahanaDbError( const char *cid, apr_status_t ec );
const char *kahanaDbDriverName( const char *cid );


/*
	TRANSACTION MODE:
	// commit the transaction
	APR_DBD_TRANSACTION_COMMIT
	// rollback the transaction
	APR_DBD_TRANSACTION_ROLLBACK
	// ignore transaction errors
	APR_DBD_TRANSACTION_IGNORE_ERRORS
*/
apr_status_t kfDbTxnStart( kDbTxn_t **newtxn, const char *cid, apr_pool_t *p );
apr_status_t kahanaDbTxnEnd( kDbTxn_t *txn );
apr_status_t kahanaDbTxnCommit( kDbTxn_t *txn );
apr_status_t kahanaDbTxnRollback( kDbTxn_t *txn );
apr_status_t kahanaDbTxnSavePt( kDbTxn_t *txn, const char *savept );
apr_status_t kahanaDbTxnSavePtRelease( kDbTxn_t *txn, const char *savept );
apr_status_t kahanaDbTxnSavePtRollback( kDbTxn_t *txn, const char *savept );


apr_status_t kahanaDbQryNew( const char **qid, const char *cid, apr_pool_t *p, const char *query );
void kahanaDbQryDelete( const char *qid );
kDbQuery_t *kahanaDbQry( const char *qid );
const apr_dbd_driver_t *kahanaDbQryDriver( kDbQuery_t *qry );
apr_dbd_t *kahanaDbQryDbd( kDbQuery_t *qry );

#define kahanaDbExecute( nrows, qid, ... )({ \
	kDbQuery_t *qry = kahanaDbQry( qid ); \
	apr_dbd_pquery( kahanaDbQryDriver( qry ), qry->p, kahanaDbQryDbd( qry ), nrows, qry->pstmt, ##__VA_ARGS__ );\
})

#define kahanaDbFetch( qid, ... )({ \
	kDbQuery_t *qry = kahanaDbQry( qid ); \
	apr_dbd_pvselect( kahanaDbQryDriver( qry ), qry->p, kahanaDbQryDbd( qry ), &qry->result, qry->pstmt, 1, ##__VA_ARGS__ ); \
})


#endif
