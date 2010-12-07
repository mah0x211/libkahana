/* 
**
**  Modify : 8/20/2009
**  (C) Masatoshi Teruya
**
*/
#include "kahana_cmd.h"

static void ErrorFN( apr_pool_t *p, int rc, const char *description )
{
//	char strbuf[MAX_STRING_LEN];
	kahanaLogPut( NULL, NULL, "ERRORFN(%d): %s, %s", rc, description, strerror( rc ) );
//	kfDEBUG_LOG( "%s", apr_strerror( errno, strbuf, MAX_STRING_LEN ) );
}

apr_status_t kahanaCmdNewAttr( kCmdAttr_t **newattr, apr_pool_t *p, apr_cmdtype_e cmdtype )
{
	kCmdAttr_t *attr = NULL;
	apr_pool_t *sp;
	apr_status_t rc = kahanaMalloc( p, sizeof( kCmdAttr_t ), (void**)&attr, &sp );
	
	if( rc == APR_SUCCESS )
	{
		const char *tmpdir = NULL;
		
		*newattr = attr;
		attr->p = sp;
		attr->errcode = 0;
		attr->errstr = NULL;
		attr->in = attr->out = attr->err = NULL;
		
		// create temp file
		if( ( rc = apr_temp_dir_get( &tmpdir, sp ) ) ||
			( rc = apr_file_mktemp( &(attr->in), (char*)apr_pstrcat( sp, tmpdir, "/XXXXXX", NULL ), 0, sp ) ) ||
			( rc = apr_file_mktemp( &(attr->out), (char*)apr_pstrcat( sp, tmpdir, "/XXXXXX", NULL ), 0, sp ) ) ||
			( rc = apr_file_mktemp( &(attr->err), (char*)apr_pstrcat( sp, tmpdir, "/XXXXXX", NULL ), 0, sp ) ) ||
			// create attr attribute
			( rc = apr_procattr_create( &(attr->attr), sp ) ) ||
			( rc = apr_procattr_io_set( attr->attr, APR_CHILD_BLOCK, APR_CHILD_BLOCK, APR_CHILD_BLOCK ) ) ||
			( rc = apr_procattr_child_in_set( attr->attr, attr->in, NULL ) ) ||
			( rc = apr_procattr_child_out_set( attr->attr, attr->out, NULL ) ) ||
			( rc = apr_procattr_child_err_set( attr->attr, attr->err, NULL ) ) ||
			( rc = apr_procattr_child_errfn_set( attr->attr, ErrorFN ) ) ||
			( rc = apr_procattr_cmdtype_set( attr->attr, cmdtype ) ) ||
			( rc = apr_procattr_detach_set( attr->attr, 0 ) ) ||
			( rc = apr_procattr_addrspace_set( attr->attr, 0 ) ) )
		{
			apr_pool_destroy( sp );
		}
	}
	
	return rc;
}

void kahanaCmdDestroykAttr( kCmdAttr_t *attr )
{
	apr_status_t rc;
	
	if( attr )
	{
		if( attr->in && ( rc = apr_file_close( attr->in ) ) ){
			kahanaLogPut( NULL, NULL, "%s", kahanaLogErr2Str( ETYPE_APR, rc ) );
		}
		if( attr->out && ( rc = apr_file_close( attr->out ) ) ){
			kahanaLogPut( NULL, NULL, "%s", kahanaLogErr2Str( ETYPE_APR, rc ) );
		}
		if( attr->err && ( rc = apr_file_close( attr->err ) ) ){
			kahanaLogPut( NULL, NULL, "%s", kahanaLogErr2Str( ETYPE_APR, rc ) );
		}
		if( attr->p ){
			apr_pool_destroy( attr->p );
		}
	}
}


apr_status_t kahanaCmdRunProcess( kCmdAttr_t *attr, const char *program, const char * const *args, const char * const *env )
{
	apr_proc_t proc = {0};
	apr_off_t offin = 0, offout = 0, offerr = 0;
	const char *dir = kahanaStrGetPathParentComponent( attr->p, program );
	int ec = 0;
	
	if( ( ec = apr_file_seek( attr->in, APR_SET, &offin ) ) ||
		( ec = apr_file_seek( attr->out, APR_SET, &offout ) ) ||
		( ec = apr_file_seek( attr->err, APR_SET, &offerr ) ) )
	{
		attr->errcode = ec;
		attr->errstr = kahanaLogErr2Str( ETYPE_APR, ec );
		return attr->errcode;
	}
	offin = 0;
	offout = 0;
	offerr = 0;
	// processing
	if(	( attr->errcode = apr_procattr_dir_set( attr->attr, dir ) ) ||
		( attr->errcode = apr_proc_create( &proc, program, args, env, attr->attr, attr->p ) ) ){
		attr->errstr = kahanaLogErr2Str( ETYPE_APR, attr->errcode );
		return attr->errcode;
	}
	// wait while processing
	apr_proc_wait( &proc, &(attr->errcode), NULL, APR_WAIT );
	// seek
	if( ( ec = apr_file_seek( attr->out, APR_SET, &offout ) ) ||
		( ec = apr_file_seek( attr->err, APR_SET, &offerr ) ) ){
		attr->errcode = ec;
		attr->errstr = kahanaLogErr2Str( ETYPE_APR, ec );
		return attr->errcode;
	}
	
	// check error
	if( attr->errcode )
	{
		apr_status_t rc;
		char chunk[HUGE_STRING_LEN];
		apr_size_t chunk_size = HUGE_STRING_LEN;
		
		memset( chunk, 0, HUGE_STRING_LEN );
		attr->errstr = ( attr->errstr ) ? attr->errstr : "";
		while( ( rc = apr_file_read( attr->err, chunk, &chunk_size ) ) ){
			attr->errstr = (const char*)apr_pstrcat( attr->p, attr->errstr, chunk, NULL );
		}
		
		if( rc && APR_EOF != rc ){
			attr->errstr = kahanaLogErr2Str( ETYPE_APR, rc );
		}
		else {
			attr->errstr = (const char*)apr_pstrcat( attr->p, attr->errstr, chunk, NULL );
			attr->errstr = kahanaStrReplace( attr->p, attr->errstr, dir, "*****", 0 );
		}
	}
	
	return attr->errcode;
}





#pragma mark -
#pragma mark Catalyst
/*
		apr_pool_create( &sp, kahana->ep );
		fprintf( stderr, "Catalyst: Starting catalyst...\n" );
		if( !( kahana->catalyst = ap_bspawn_child( p, Catalyst, (void*)&kahana, kill_always, NULL, NULL, NULL ) ) )
		{
			fprintf( stderr, "Catalyst: Failure to create catalyst.\n" );
			fprintf( stderr, "Catalyst: Collapse...\n" );
		}
		apr_pool_destroy(sp);
		apr_pool_cleanup_register( p, NULL, KahanaCollapse, KahanaCollapse );
		ap_register_cleanup( p, NULL, KahanaCollapse, KahanaCollapse );
		ap_register_cleanup( p, NULL, KahanaCollapse, ap_null_cleanup );

static int Catalyst( void *data, child_info *ci )
{
	Kahana *g = *(Kahana**)data;
	const int pid = getpid();
	apr_pool_t *p = ap_make_sub_pool( g->p );
	int sock, csock;
	struct sockaddr_in server;
	struct sockaddr_in client;
	socklen_t slen;
	int flag = 1;
	
	fprintf( stderr, "Catalyst[%d] : .", pid );
	
	if( ( sock = ap_psocket( p, PF_INET, SOCK_STREAM, 0 ) ) < 0 )
	{
		fprintf( stderr, "..socket error! : %s[%d].\n", strerror(errno), errno );
		sleep(1);
		kill(g->pid, SIGTERM);
		return -1;
	}
	fprintf( stderr, "." );
	
	ap_note_cleanups_for_socket( p, sock );
	setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)  );
	bzero( (char*)&server, sizeof(server) );
	server.sin_family = PF_INET;
	server.sin_port = htons(50000);
	server.sin_addr.s_addr = htonl( INADDR_ANY );
	
	if( bind( sock, (struct sockaddr*)&server, sizeof(server) ) < 0 )
	{
		fprintf( stderr, "..bind error! : %s[%d].\n", strerror(errno), errno );
		sleep(1);
		kill(g->pid, SIGTERM);
		return -1;
	}
	fprintf( stderr, "." );
	
	if( listen( sock, 5 ) < 0 )
	{
		fprintf( stderr, "..listen error! : %s[%d].\n", strerror(errno), errno );
		sleep(1);
		kill(g->pid, SIGTERM);
		return -1;
	}
	
	fprintf( stderr, "woked up!\n" );
	while(1)
	{
		apr_pool_t *sp = ap_make_sub_pool( p );
		BUFF *buf = NULL;
		// ap_getline's two extra for \n\0
		char raw[DEFAULT_LIMIT_REQUEST_LINE + 2]; 
		const int limit = sizeof(raw);
		int rsize = 0, total = 0;
		int response = HTTP_OK;
		
		slen = sizeof(client);
		// wait while request connection
		if( ( csock = accept( sock, (struct sockaddr*)&client, &slen ) ) < 0 )
		{
			fprintf( stderr, "Catalyst : accept : %s[%d].\n", strerror(errno), errno );
			ap_pclosesocket( p, sock );
			sleep(1);
			kill(g->pid, SIGTERM);
			return -1;
		}
		ap_note_cleanups_for_socket( sp, csock );
		
		fprintf( stderr, "Catalyst : connect request from : %s, port : %d.\n", inet_ntoa( client.sin_addr), ntohs(client.sin_port) );
		
		// create read & write BUFF
		buf = ap_bcreate( sp, B_RDWR|B_SOCKET );
		ap_bpushfd( buf, csock, csock );
		// read request
		ap_block_alarms();
		while( ( rsize = ap_getline( raw, limit, buf, 0 ) ) > 0 )
		{
			total += rsize;
			if( total > limit )
			{
				response = HTTP_REQUEST_URI_TOO_LARGE;
				break;
			}
			
			fprintf( stderr, "Catalyst : request line:%d[%s]\n", rsize, raw );
		}
		ap_unblock_alarms();
		
		fprintf( stderr, "Catalyst : total request length : %d, limit : %d\n", total, sizeof(raw) );
		
		ap_bclose(buf);
		apr_pool_destroy(sp);
		close(csock);
		break;
	}
	
	apr_pool_destroy(p);
	ap_pclosesocket( g->p, sock );
	sleep(1);
	kill( g->pid, SIGUSR1 );
	
	return 0;
}
*/
/*
HTTP/1.1 414 Request-URI Too Large
Date: Wed, 03 Jan 2007 05:26:02 GMT
Server: Catalyst
Connection: close
Content-Type: text/plain; charset=iso-8859-1

<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">
<HTML><HEAD>
<TITLE>414 Request-URI Too Large</TITLE>
</HEAD><BODY>
<H1>Request-URI Too Large</H1>
The requested URL's length exceeds the capacity
limit for this server.<P>
request failed: URI too long<P>
</BODY></HTML>

--------

HTTP/1.1 200 OK
Date: Wed, 03 Jan 2007 05:34:30 GMT
Server: Apache
Last-Modified: Wed, 03 Jan 2007 02:56:18 GMT
ETag: "289c33-4-459b1b52"
Accept-Ranges: bytes
Content-Length: 4
Connection: close
Content-Type: text/plain

test

------

HTTP/1.1 414 Request-URI Too Large
Date: Wed, 03 Jan 2007 05:26:02 GMT
Server: Apache
Connection: close
Content-Type: text/html; charset=iso-8859-1

<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">
<HTML><HEAD>
<TITLE>414 Request-URI Too Large</TITLE>
</HEAD><BODY>
<H1>Request-URI Too Large</H1>
The requested URL's length exceeds the capacity
limit for this server.<P>
request failed: URI too long<P>
</BODY></HTML>

--------
*/
