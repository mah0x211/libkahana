/*
	printLog( NULL, "_SC_NPROCESSORS_ONLN: %d", sysconf( _SC_NPROCESSORS_ONLN ) );
	printLog( NULL, "_SC_CHILD_MAX: %d", sysconf( _SC_CHILD_MAX ) );
	printLog( NULL, "_SC_OPEN_MAX: %d", sysconf( _SC_OPEN_MAX ) );
	printLog( NULL, "_SC_LINE_MAX: %d", sysconf( _SC_LINE_MAX ) );
*/

#include "kahana_socket.h"


// BSD socket
struct kSocket_t {
	apr_pool_t *pp;
	void *parent;
	kSocketType_e type;
	int fd;
	struct sockaddr_in addr;
	int ai_flags;
	int ai_family;
	int ai_socktype;
	// bufsize
	int buf_rcv;
	int buf_snd;
	// timeout
	struct timeval tval_rcv;
	struct timeval tval_snd;
	// callbacks
	cbSocketOnErr cb_err;
	cbSocketOnRecv cb_recv;
	cbSocketOnSend cb_send;
	cbSocketOnTimeout cb_timeout;
	cbSocketOnSignal cb_signal[NSIG];
	// event
	struct event_base *loop;
	struct event io_ev;
	struct event sig_ev[NSIG];
};

static int SockOptSetNonBlock( int fd )
{
	int flg = 0;
	
	errno = 0;
	// get flags
	if( ( flg = fcntl( fd, F_GETFL ) ) < 0 ){
		return errno;
	}
	// set flag
	flg |= O_NONBLOCK;
	if( fcntl( fd, F_SETFL, flg ) < 0 ){
		return errno;
	}
	
	return APR_SUCCESS;
}

static void RequestTimeout( int fd, short event, void *arg )
{
	kSocketRequest_t *req = (kSocketRequest_t*)arg;
	
	if( req->fd )
	{
		shutdown( req->fd, SHUT_RD );
		if( req->server->sock->cb_timeout )
		{
			const char *res = req->server->sock->cb_timeout( req );
			
			if( res ){
				send( req->fd, res, strlen( res ), 0 );
			}
		}
		shutdown( req->fd, SHUT_WR );
	}
	apr_pool_destroy( req->p );
}


static void RequestRecv( int fd, short event, void *arg )
{
	kSocketRequest_t *req = (kSocketRequest_t*)arg;
	char buf[req->server->sock->buf_rcv];
	// buf CR+LF
	ssize_t len = 0;
	
	// stop timer if exists
	if( req->server->timeout.tv_sec ){
		evtimer_del( &req->timer_ev );
	}
	
	if( ( len = recv( req->fd, buf, sizeof( buf ), 0 ) ) == 0 ){
		fprintf( stderr, "recv(): nolen(): %d, %s\n", errno, strerror( errno ) );
		event_base_loopbreak( req->loop );
	}
	else if( len == -1 )
	{
		// EAGAIN if non-blocking mode
		switch( errno )
		{
			case EAGAIN:
				fprintf( stderr, "EAGAIN to recv(): %d, %s\n", errno, strerror( errno ) );
				return;
			
			default:
				fprintf( stderr, "failed to recv(): %d, %s\n", errno, strerror( errno ) );
				event_base_loopbreak( req->loop );
		}
	}
	else if( !req->server->sock->cb_recv( req, buf, len ) ){
		req->server->sock->cb_send( req );
		event_base_loopbreak( req->loop );
	}
	else if( req->server->timeout.tv_sec )
	{
		int rc;
		
		evtimer_set( &req->timer_ev, RequestTimeout, (void*)req );
		if( ( rc = event_base_set( req->loop, &req->timer_ev ) ) ){
			kahanaLogPut( NULL, NULL, "failed to event_base_set()" );
		}
		else if( ( rc = evtimer_add( &req->timer_ev, &req->server->timeout ) ) ){
			kahanaLogPut( NULL, NULL, "failed to evtimer_add()" );
		}
	}
}


static apr_status_t RequestThreadDestroy( void *data )
{
	kSocketRequest_t *req = (kSocketRequest_t*)data;
	apr_status_t rc = APR_SUCCESS;
	
	// remove from hash
	apr_hash_set( req->server->reqs, req->key, APR_HASH_KEY_STRING, NULL );
	evtimer_del( &req->timer_ev );
	event_del( &req->io_ev );
	event_base_free( req->loop );
	if( req->fd ){
		close( req->fd );
		req->fd = 0;
	}
	
	return rc;
}

static void *RequestThread( apr_thread_t *thd, void *arg )
{
	kSocketRequest_t *req = (kSocketRequest_t*)arg;
	int rc;
	
	req->thd = thd;
	// set request hash
	apr_hash_set( req->server->reqs, req->key, APR_HASH_KEY_STRING, arg );
	
	// set io event
	event_set( &req->io_ev, req->fd, EV_READ|EV_PERSIST, RequestRecv, arg );
	// set priority
	// ev_set_priority( &req->watch_fd, 0 );
	if( ( rc = event_base_set( req->loop, &req->io_ev ) ) ){
		kahanaLogPut( NULL, NULL, "failed to event_base_set()" );
	}
	// add with no timeout
	else if( ( rc = event_add( &req->io_ev, NULL ) ) ){
		kahanaLogPut( NULL, NULL, "failed to event_add()" );
	}
	// set timeout event if server->sock->timeout.tv_sec > 0
	else if( req->server->timeout.tv_sec )
	{
		evtimer_set( &req->timer_ev, RequestTimeout, (void*)req );
		if( ( rc = event_base_set( req->loop, &req->timer_ev ) ) ){
			kahanaLogPut( NULL, NULL, "failed to event_base_set()" );
		}
		else if( ( rc = evtimer_add( &req->timer_ev, NULL ) ) ){
			kahanaLogPut( NULL, NULL, "failed to evtimer_add()" );
		}
	}
	// register cleanup
	apr_pool_cleanup_register( req->p, (void*)req, RequestThreadDestroy, apr_pool_cleanup_null );
	
	// block dispatch
	if( rc == 0 && ( rc = event_base_dispatch( req->loop ) ) == -1 ){
		kahanaLogPut( NULL, NULL, "failed to event_base_loop()" );
	}
	// cleanup after dispatch
	apr_pool_destroy( req->p );
	
	return NULL;
}

static void SocketAccept( int sfd, short event, void *arg )
{
	kSocket_t *sock = (kSocket_t*)arg;
	struct sockaddr_in addr;
	socklen_t len = sizeof( struct sockaddr_in );
	// wait while client connection
	int fd = accept( sock->fd, (struct sockaddr*)&addr, &len );
	
	if( fd == -1 ){
		kahanaLogPut( NULL, NULL, "failed to accept(): %s", strerror( errno ) );
	}
	else
	{
		apr_status_t rc;
		kSocketServer_t *server = (kSocketServer_t*)sock->parent;
		kSocketRequest_t *req = NULL;
		apr_pool_t *p = NULL;
		
		// set non-blocking mode
		if( SockOptSetNonBlock( fd ) != 0 ){
			kahanaLogPut( NULL, NULL, "failed to SockOptSetNonBlock(): %s", strerror( errno ) );
			rc = errno;
		}
		// allocate Request_t
		else if( ( rc = kahanaMalloc( sock->pp, sizeof( kSocketRequest_t ), (void**)&req, &p ) ) ){
			kahanaLogPut( NULL, NULL, "failed to kahanaMalloc() reasons %s", STRERROR_APR( rc ) );
		}
		// get default receive/send buffer size
		else if( ( ( len = sizeof( int ) ) && ( getsockopt( fd, SOL_SOCKET, SO_RCVBUF, (void*)&req->rcvbuf, &len ) == -1 ) ) ||
				 ( ( len = sizeof( int ) ) && ( getsockopt( fd, SOL_SOCKET, SO_SNDBUF, (void*)&req->sndbuf, &len ) == -1 ) ) ){
			kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", strerror( errno ) );
			rc = errno;
			apr_pool_destroy( p );
		}
		// create event loop
		else if( !( req->loop = event_base_new() ) ){
			kahanaLogPut( NULL, NULL, "failed to event_base_new()" );
			rc = APR_EGENERAL;
		}
		else
		{
			req->thd = NULL;
			req->p = p;
			req->fd = fd;
			req->key = kahanaStrI2S( p, fd );
			req->server = server;
			req->ctx = NULL;
			memcpy( &req->addr, &addr, sizeof( addr ) );
			// create request thread
			if( ( rc = apr_thread_pool_push( server->thdp, RequestThread, (void*)req, APR_THREAD_TASK_PRIORITY_NORMAL, NULL ) ) ){
				kahanaLogPut( NULL, NULL, "failed to apr_thread_pool_push(): %s", STRERROR_APR( rc ) );
				event_base_free( req->loop );
				apr_pool_destroy( p );
			}
		}
		
		if( rc )
		{
			shutdown( fd, SHUT_RD );
			if( sock->cb_err ){
				const char *errstr = sock->cb_err();
				send( fd, errstr, strlen( errstr ), 0 );
			}
			close( fd );
		}
	}
}

static void SocketSignal( int signum, short event, void *arg )
{
	kSocket_t *sock = (kSocket_t*)arg;
	cbSocketOnSignal callback = sock->cb_signal[signum];
	
	if( callback ){
		callback( signum, event, sock->parent, sock->type );
	}
}

// MARK:  SOCKET OPTION
int kahanaSocketOptGetRcvBuf( kSocket_t *sock )
{
	return sock->buf_rcv;
}

int kahanaSocketOptSetRcvBuf( kSocket_t *sock, int bytes )
{
	// set receive buffer size
	if( bytes > 0 )
	{
		if( setsockopt( sock->fd, SOL_SOCKET, SO_RCVBUF, (void*)&bytes, sizeof( bytes ) ) == -1 ){
			kahanaLogPut( NULL, NULL, "failed to setsockopt(): %d", strerror( errno ) );
			return -1;
		}
		else {
			sock->buf_rcv = bytes;
		}
	}
	
	return 0;
}

struct timeval kahanaSocketOptGetRcvTimeout( kSocket_t *sock )
{
	return sock->tval_rcv;
}

int kahanaSocketOptSetRcvTimeout( kSocket_t *sock, time_t sec, time_t usec )
{
	int rc = 0;
	struct timeval tval;
	socklen_t len;

	tval.tv_sec = sec;
	tval.tv_usec = usec;
	len = sizeof( tval );
	
	if( ( rc = setsockopt( sock->fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&tval, len ) ) == 0 ){
		sock->tval_rcv = tval;
	}
	
	return rc;
}



int kahanaSocketOptGetSndBuf( kSocket_t *sock )
{
	return sock->buf_snd;
}
struct timeval kahanaSocketOptGetSndTimeout( kSocket_t *sock )
{
	return sock->tval_snd;
}

int kahanaSocketOptSetSndTimeout( kSocket_t *sock, time_t sec, time_t usec )
{
	int rc = 0;
	struct timeval tval;
	socklen_t len;

	tval.tv_sec = sec;
	tval.tv_usec = usec;
	len = sizeof( tval );
	
	if( ( rc = setsockopt( sock->fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&tval, len ) ) == 0 ){
		sock->tval_snd = tval;
	}
	
	return rc;
}

// MARK: Register Socket Callback
apr_status_t kahanaSocketSetOnSignal( kSocket_t *sock, int signum, cbSocketOnSignal cb_signal )
{
	apr_status_t rc = APR_SUCCESS;
	
	if( !cb_signal || signum >= NSIG ){
		rc = APR_EINVAL;
	}
	else
	{
		// set io event
		event_set( &sock->sig_ev[signum], signum, EV_SIGNAL|EV_PERSIST, SocketSignal, (void*)sock );
		if( ( rc = event_base_set( sock->loop, &sock->sig_ev[signum] ) ) ){
			kahanaLogPut( NULL, NULL, "failed to event_base_set()" );
		}
		// add no timeout
		else if( ( rc = event_add( &sock->sig_ev[signum], NULL ) ) ){
			kahanaLogPut( NULL, NULL, "failed to event_add()" );
		}
		else {
			sock->cb_signal[signum] = cb_signal;
		}
	}
	
	return rc;
}

void kahanaSocketSetOnErr( kSocket_t *sock, cbSocketOnErr cb_err )
{
	sock->cb_err = cb_err;
}
void kahanaSocketSetOnRecv( kSocket_t *sock, cbSocketOnRecv cb_recv )
{
	sock->cb_recv = cb_recv;
}

void kahanaSocketSetOnSend( kSocket_t *sock, cbSocketOnSend cb_send )
{
	sock->cb_send = cb_send;
}
void kahanaSocketSetOnTimeout( kSocket_t *sock, cbSocketOnTimeout cb_timeout )
{
	sock->cb_timeout = cb_timeout;
}


/*
int flg: read ev.h
	0,				blocking
	EVLOOP_ONCE		block *once* only
	EVLOOP_NONBLOCK	do not block/wait
*/
int kahanaSocketLoop( kSocket_t *sock, int flg )
{
	int rc;
	// set io event
	event_set( &sock->io_ev, sock->fd, EV_READ|EV_PERSIST, SocketAccept, (void*)sock );
	if( ( rc = event_base_set( sock->loop, &sock->io_ev ) ) ){
		kahanaLogPut( NULL, NULL, "failed to event_base_set()" );
	}
	// add no timeout
	else if( ( rc = event_add( &sock->io_ev, NULL ) ) ){
		kahanaLogPut( NULL, NULL, "failed to event_add()" );
	}
	else if( ( rc = event_base_loop( sock->loop, flg ) ) == -1 ){
		kahanaLogPut( NULL, NULL, "failed to event_base_loop()" );
	}
	
	return rc;
}

int kahanaSocketUnloop( kSocket_t *sock )
{
	return event_base_loopbreak( sock->loop );
}


int kahanaSocketCreate( apr_pool_t *p, kSocket_t **newsock, const char *host, const char *port, int ai_flags, int ai_family, int ai_socktype )
{
	int rc = 0;
	kSocket_t *sock;
	
	// invalid params
	if( !( sock = (kSocket_t*)apr_pcalloc( p, sizeof( kSocket_t ) ) ) ){
		rc = ENOMEM;
		kahanaLogPut( NULL, NULL, "failed to apr_pcalloc() reason %s", strerror( rc ) );
	}
	else
	{
		struct addrinfo hints, *res;
		sock->pp = p;
		sock->parent = NULL;
		sock->type = SOCKET_AS_UNKNOWN;
		sock->ai_flags = ai_flags;
		sock->ai_family = ai_family;
		sock->ai_socktype = ai_socktype;
		sock->cb_err = NULL;
		sock->cb_recv = NULL;
		sock->cb_send = NULL;
		sock->cb_timeout = NULL;
		memset( &sock->io_ev, 0, sizeof( sock->io_ev ) );
		sock->loop = NULL;
		memset( (void*)&sock->addr, 0, sizeof( sock->addr ) );
		
		memset( (void*)&hints, 0, sizeof( hints ) );
		hints.ai_family = ai_family;
		hints.ai_socktype = ai_socktype;
		hints.ai_flags = ai_flags;
		hints.ai_protocol = 0;
		// get address info
		if( ( rc = getaddrinfo( host, port, &hints, &res ) ) != 0 ){
			kahanaLogPut( NULL, NULL, "failed to getaddrinfo(): %s", gai_strerror( rc ) );
		}
		else
		{
			struct addrinfo *ptr = res;
			int sockopt = 1;
			socklen_t len = 0;
			
			// find addrinfo
			do
			{
				if( ( sock->fd = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol ) ) != -1 ){
					break;
				}
			} while( ( ptr = ptr->ai_next ) );
			
			if( sock->fd == -1 ){
				rc = errno;
				kahanaLogPut( NULL, NULL, "failed to socket(): %s", strerror( errno ) );
			}
			else
			{
				// set address info
				memcpy( (void*)&sock->addr, (void*)ptr->ai_addr, ptr->ai_addrlen );
				// set sockopt SO_REUSEADDR
				if( setsockopt( sock->fd, SOL_SOCKET, SO_REUSEADDR, (char*)&sockopt, sizeof( sockopt ) ) != 0 ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to setsockopt(): %s", strerror( errno ) );
				}
				// get default receive buffer size
				else if( ( len = sizeof( int ) ) &&
						( getsockopt( sock->fd, SOL_SOCKET, SO_RCVBUF, (void*)&sock->buf_rcv, &len ) == -1 ) ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", strerror( errno ) );
				}
				// get default send buffer size
				else if( ( len = sizeof( int ) ) &&
						( getsockopt( sock->fd, SOL_SOCKET, SO_SNDBUF, (void*)&sock->buf_snd, &len ) == -1 ) ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", strerror( errno ) );
				}
				// get default receive timeout
				else if( ( len = sizeof( struct timeval ) ) &&
						( getsockopt( sock->fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&sock->tval_rcv, &len ) == -1 ) ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", strerror( errno ) );
				}
				// get default send timeout
				else if( ( len = sizeof( struct timeval ) ) &&
						( getsockopt( sock->fd, SOL_SOCKET, SO_SNDTIMEO, (void*)&sock->tval_snd, &len ) == -1 ) ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", strerror( errno ) );
				}
				// create event loop
				else if( !( sock->loop = event_base_new() ) ){
					rc = APR_EGENERAL;
					kahanaLogPut( NULL, NULL, "failed to event_base_new()" );
				}
				// nothing problem
				else {
					*newsock = sock;
				}
				
				if( rc ){
					close( sock->fd );
				}
			}
			freeaddrinfo( res );
		}
	}
	
	return rc;
}

// MARK:  SERVER SOCKET
static apr_status_t SocketServerDestroy( void *data )
{
	kSocketServer_t *server = (kSocketServer_t*)data;
	
	if( server )
	{
		if( server->sock && server->sock->fd ){
			close( server->sock->fd );
			server->sock->fd = 0;
		}
		if( server->sock->loop ){
			event_base_free( server->sock->loop );
			server->sock->loop = NULL;
		}
		// thread pool
		if( server->thdp ){
			apr_thread_pool_destroy( server->thdp );
		}
	}
	
	return APR_SUCCESS;
}

void kahanaSocketServerSetTimeout( kSocketServer_t *server, struct timeval timeout )
{
	server->timeout = timeout;
}

/*
create socket and bind, listen
APR_EGENERAL
*/
apr_status_t kahanaSocketServerCreate( apr_pool_t *pp, kSocketServer_t **newserver, const char *host, const char *port, size_t thd_max, int ai_flags, int ai_family, int ai_socktype, void *userdata )
{
	apr_status_t rc = APR_SUCCESS;
	apr_pool_t *p = NULL;
	kSocketServer_t *server = NULL;
	
	// invalid params
	if( !host || !port ){
		rc = APR_EINVAL;
	}
	// allocate
	else if( ( rc = kahanaMalloc( pp, sizeof( kSocketServer_t ), (void*)&server, &p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
	}
	// create thread-pool
	else if( ( rc = apr_thread_pool_create( &server->thdp, 0, thd_max, p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to apr_thread_pool_create(): %s", STRERROR_APR( rc ) );
		apr_pool_destroy( p );
	}
	// create socket
	else if( ( rc = kahanaSocketCreate( p, &server->sock, host, port, ai_flags, ai_family, ai_socktype ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
		apr_pool_destroy( p );
	}
	else
	{
		server->p = p;
		server->sock->parent = (void*)server;
		server->sock->type = SOCKET_AS_SERVER;
		memset( &server->timeout, 0, sizeof( server->timeout ) );
		server->reqs = apr_hash_make( p );
		server->userdata = userdata;
		apr_pool_cleanup_register( p, (void*)server, SocketServerDestroy, apr_pool_cleanup_null );
		
		// bind
		if( bind( server->sock->fd, (struct sockaddr*)&(server->sock->addr), sizeof( server->sock->addr ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to bind(): %s", strerror( errno ) );
		}
		// Set the socket to non-blocking, this is essential in
		// event based programming with libevent.
		else if( ( rc = SockOptSetNonBlock( server->sock->fd ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to SockOptSetNonBlock(): %s", strerror( errno ) );
		}
		// listen
		else if( ( rc = listen( server->sock->fd, SOMAXCONN ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to listen(): %s", strerror( errno ) );
		}
		// nothing problem
		else
		{
			/*
			if( kahanaSocketOptSetTimeout( server->sock, SO_RCVTIMEO, 10, 10 ) != 0 ){
				kahanaLogPut( NULL, NULL, "failed to kahanaSocketOptSetTimeout(): %s", strerror( errno ) ); 
			}
			fprintf( stderr, "buf_rcv -> %d, buf_snd -> %d, tval_rcv -> %u.%u, tval_snd -> %u.%u, SOMAXCONN -> %d\n", 
					server->sock->buf_rcv, server->sock->buf_snd, 
					server->sock->tval_rcv.tv_sec, server->sock->tval_rcv.tv_usec,
					server->sock->tval_snd.tv_sec, server->sock->tval_snd.tv_usec,
					SOMAXCONN );
			*/
			*newserver = server;
		}
		
		if( rc ){
			apr_pool_destroy( server->p );
		}
	}
	
	return rc;
}

void kahanaSocketServerDestroy( kSocketServer_t *server )
{
	if( server ){
		apr_pool_destroy( server->p );
	}
}

