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
	cbSocketOnRecv cb_recv;
	cbSocketOnSend cb_send;
	cbSocketOnAccept cb_accept;
	// event
	struct ev_io io_w;
	struct ev_loop *loop;
	struct ev_signal sig_w[NSIG];
	cbSocketOnSignal cb_signal[NSIG];
	cbSocketOnTimeout cb_timeout;
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

static void SocketTimeout( struct ev_loop *loop, ev_timer *w, int event )
{
	#pragma unused( loop, event )
	kSocketRequest_t *req = (kSocketRequest_t*)w->data;
	
	if( req->fd )
	{
		if( req->server->sock->cb_timeout )
		{
			const char *res = req->server->sock->cb_timeout( req );
		
			if( res ){
				write( req->fd, res, strlen( res ) );
			}
		}
		shutdown( req->fd, SHUT_RDWR );
	}
	apr_pool_destroy( req->p );
}


static void SocketRecv( struct ev_loop *loop, ev_io *io, int event )
{
	#pragma unused( event )
	kSocketRequest_t *req = (kSocketRequest_t*)io->data;
	const char *errstr = NULL;
	char buf[req->server->sock->buf_rcv];
	// buf CR+LF
	const ssize_t len = recv( req->fd, buf, sizeof( buf ), 0 );
	
	if( len == 0 ){
		fprintf( stderr, "recv(): nolen, %s\n", kahanaLogErr2Str( ETYPE_SYS, errno ) );
		apr_pool_destroy( req->p );
	}
	else if( len == -1 )
	{
		// EAGAIN if non-blocking mode
		switch( errno )
		{
			case EAGAIN:
				fprintf( stderr, "EAGAIN to recv(): %d, %s\n", errno, kahanaLogErr2Str( ETYPE_SYS, errno ) );
				return;
			
			default:
				fprintf( stderr, "failed to recv(): %d, %s\n", errno, kahanaLogErr2Str( ETYPE_SYS, errno ) );
				apr_pool_destroy( req->p );
		}
	}
	else
	{
		// stop timer if exists
		if( req->server->timeout ){
			ev_timer_stop( loop, &req->watch_timer );
		}
		
		if( ( errstr = req->server->sock->cb_recv( req, buf, len ) ) )
		{
			errno = 0;
			if( write( io->fd, errstr, strlen( errstr ) ) < 0 ){
				kahanaLogPut( NULL, NULL, "failed to write():", kahanaLogErr2Str( ETYPE_SYS, errno ) );
			}
			// apr_pool_destroy( req->p );
		}
		else if( req->server->timeout ){
			ev_timer_set( &req->watch_timer, req->server->timeout, 0 );
			ev_timer_start( loop, &req->watch_timer );
		}
		/*
		if( !req->server->sock->cb_send( req ) ){
			apr_pool_destroy( req->p );
		}
		*/
	}
}


static apr_status_t DestroyRequestThr( void *data )
{
	kSocketRequest_t *req = (kSocketRequest_t*)data;
	apr_status_t rc = APR_SUCCESS;
	
	fprintf( stderr, "destroy:%s\n", req->key );
	// remove from hash
	apr_hash_set( req->server->reqs, req->key, APR_HASH_KEY_STRING, NULL );
	ev_timer_stop( req->server->sock->loop, &req->watch_timer );
	ev_io_stop( req->server->sock->loop, &req->watch_fd );
	if( req->fd ){
		close( req->fd );
		req->fd = 0;
	}
	
	return rc;
}

static void SocketAccept( struct ev_loop *loop, ev_io *w, int event )
{
	#pragma unused( event )
	kSocket_t *sock = (kSocket_t*)w->data;
	kSocketServer_t *server = (kSocketServer_t*)sock->parent;
	struct sockaddr_in addr;
	socklen_t len = sizeof( struct sockaddr_in );
	void *ctx = NULL;
	// wait while client connection
	int fd = accept( sock->fd, (struct sockaddr*)&addr, &len );
	
	if( fd == -1 ){
		kahanaLogPut( NULL, NULL, "failed to accept(): %s", kahanaLogErr2Str( ETYPE_SYS, errno ) );
	}
	// ask allow this user
	else if( sock->cb_accept && 
			!sock->cb_accept( server, addr, &ctx ) ){
		// kahanaLogPut( NULL, NULL, "reqest deny from %s:%d", inet_ntoa( addr.sin_addr ), ntohs( addr.sin_port ) );
		close( fd );
	}
	// set non-blocking mode
	else if( SockOptSetNonBlock( fd ) != 0 ){
		kahanaLogPut( NULL, NULL, "failed to SockOptSetNonBlock(): %s", kahanaLogErr2Str( ETYPE_SYS, errno ) );
		close( fd );
	}
	else
	{
		apr_status_t rc;
		kSocketRequest_t *req = NULL;
		apr_pool_t *p = NULL;
		socklen_t len;
		
		// allocate Request_t
		if( ( rc = kahanaMalloc( sock->pp, sizeof( kSocketRequest_t ), (void**)&req, &p ) ) ){
			kahanaLogPut( NULL, NULL, "failed to kahanaMalloc() reasons %s", kahanaLogErr2Str( ETYPE_APR, rc ) );
		}
		// get default receive buffer size
		else if( ( ( len = sizeof( int ) ) && ( getsockopt( fd, SOL_SOCKET, SO_RCVBUF, (void*)&req->rcvbuf, &len ) == -1 ) ) ||
		// get default send buffer size
				 ( ( len = sizeof( int ) ) && ( getsockopt( fd, SOL_SOCKET, SO_SNDBUF, (void*)&req->sndbuf, &len ) == -1 ) ) ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", kahanaLogErr2Str( ETYPE_SYS, errno ) );
		}
		else
		{
			req->p = p;
			req->key = kahanaStrI2S( p, fd );
			req->server = (kSocketServer_t*)sock->parent;
			req->fd = fd;
			req->addr = addr;
			req->ctx = ctx;
			// set io event with priority
			req->watch_fd.data = (void*)req;
			ev_io_init( &req->watch_fd, SocketRecv, req->fd, EV_READ );
			ev_set_priority( &req->watch_fd, 0 );
			// set timeout event if server->sock->timeout > 0
			if( server->timeout ){
				req->watch_timer.data = (void*)req;
				ev_timer_init( &req->watch_timer, SocketTimeout, server->timeout, 0 );
				ev_timer_start( loop, &req->watch_timer );
			}
			else {
				req->watch_timer.data = NULL;
			}
			// set request hash
			apr_hash_set( req->server->reqs, req->key, APR_HASH_KEY_STRING, (void*)req );
			apr_pool_cleanup_register( req->p, (void*)req, DestroyRequestThr, apr_pool_cleanup_null );
			// run event loop
			ev_io_start( loop, &req->watch_fd );
		}
		if( rc ){
			close( fd );
			apr_pool_destroy( p );
		}
	}
}

static void SocketSignal( struct ev_loop *loop, ev_signal *w, int event )
{
	#pragma unused( loop )
	kSocket_t *sock = (kSocket_t*)w->data;
	cbSocketOnSignal callback = sock->cb_signal[w->signum];
	
	if( callback ){
		callback( w, event, sock->parent, sock->type );
	}
}

#pragma mark SOCKET OPTION

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
			kahanaLogPut( NULL, NULL, "failed to setsockopt(): %d", kahanaLogErr2Str( ETYPE_SYS, errno ) );
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

#pragma mark SOCKET CALLBACKS
/* REGISTER CALLBACKS */
apr_status_t kahanaSocketSetOnSignal( kSocket_t *sock, int signum, cbSocketOnSignal cb_signal )
{
	apr_status_t rc = APR_SUCCESS;
	
	if( !cb_signal || signum >= NSIG ){
		rc = APR_EINVAL;
	}
	else {
		sock->cb_signal[signum] = cb_signal;
		sock->sig_w[signum].data = (void*)sock;
		ev_signal_init( &sock->sig_w[signum], SocketSignal, signum );
		ev_signal_start( sock->loop, &sock->sig_w[signum] );
	}
	
	return rc;
}

void kahanaSocketSetOnAccept( kSocket_t *sock, cbSocketOnAccept cb_accept )
{
	sock->cb_accept = cb_accept;
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
	EVLOOP_NONBLOCK	1		do not block/wait
	EVLOOP_ONESHOT	2		block *once* only
*/
void kahanaSocketDispatch( kSocket_t *sock, int flg )
{
	ev_loop( sock->loop, flg );
}


int kahanaSocketCreate( apr_pool_t *p, kSocket_t **newsock, const char *host, const char *port, int ai_flags, int ai_family, int ai_socktype )
{
	int rc = 0;
	kSocket_t *sock;
	
	// invalid params
	if( !( sock = (kSocket_t*)apr_pcalloc( p, sizeof( kSocket_t ) ) ) ){
		rc = ENOMEM;
		kahanaLogPut( NULL, NULL, "failed to apr_pcalloc() reason %s", kahanaLogErr2Str( ETYPE_SYS, rc ) );
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
		sock->cb_accept = NULL;
		sock->cb_recv = NULL;
		sock->cb_send = NULL;
		sock->cb_timeout = NULL;
		sock->io_w.data = (void*)sock;
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
				kahanaLogPut( NULL, NULL, "failed to socket(): %s", kahanaLogErr2Str( ETYPE_SYS, errno ) );
			}
			else
			{
				// set address info
				memcpy( (void*)&sock->addr, (void*)ptr->ai_addr, ptr->ai_addrlen );
				// set sockopt SO_REUSEADDR
				if( setsockopt( sock->fd, SOL_SOCKET, SO_REUSEADDR, (char*)&sockopt, sizeof( sockopt ) ) != 0 ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to setsockopt(): %s", kahanaLogErr2Str( ETYPE_SYS, errno ) );
				}
				// get default receive buffer size
				else if( ( len = sizeof( int ) ) &&
						( getsockopt( sock->fd, SOL_SOCKET, SO_RCVBUF, (void*)&sock->buf_rcv, &len ) == -1 ) ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", kahanaLogErr2Str( ETYPE_SYS, errno ) );
				}
				// get default send buffer size
				else if( ( len = sizeof( int ) ) &&
						( getsockopt( sock->fd, SOL_SOCKET, SO_SNDBUF, (void*)&sock->buf_snd, &len ) == -1 ) ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", kahanaLogErr2Str( ETYPE_SYS, errno ) );
				}
				// get default receive timeout
				else if( ( len = sizeof( struct timeval ) ) &&
						( getsockopt( sock->fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&sock->tval_rcv, &len ) == -1 ) ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", kahanaLogErr2Str( ETYPE_SYS, errno ) );
				}
				// get default send timeout
				else if( ( len = sizeof( struct timeval ) ) &&
						( getsockopt( sock->fd, SOL_SOCKET, SO_SNDTIMEO, (void*)&sock->tval_snd, &len ) == -1 ) ){
					rc = errno;
					kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", kahanaLogErr2Str( ETYPE_SYS, errno ) );
				}
				// create event loop
				else if( !( sock->loop = ev_loop_new( EVFLAG_AUTO ) ) ){
					rc = APR_EGENERAL;
					kahanaLogPut( NULL, NULL, "failed to ev_loop_new()" );
				}
				// nothing problem
				else {
					ev_io_init( &sock->io_w, SocketAccept, sock->fd, EV_READ );
					ev_io_start( sock->loop, &sock->io_w );
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

#pragma mark SERVER SOCKET
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
			ev_unloop( server->sock->loop, EVUNLOOP_ALL );
			ev_loop_destroy( server->sock->loop );
			server->sock->loop = NULL;
		}
	}
	
	return APR_SUCCESS;
}

void kahanaSocketServerSetTimeout( kSocketServer_t *server, time_t timeout )
{
	server->timeout = timeout;
}


/*
create socket and bind, listen
APR_EGENERAL
*/
apr_status_t kahanaSocketServerCreate( apr_pool_t *pp, kSocketServer_t **newserver, const char *host, const char *port, time_t timeout, int ai_flags, int ai_family, int ai_socktype, void *userdata )
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
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", kahanaLogErr2Str( ETYPE_APR, rc ) );
	}
	// create socket
	else if( ( rc = kahanaSocketCreate( p, &server->sock, host, port, ai_flags, ai_family, ai_socktype ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", kahanaLogErr2Str( ETYPE_APR, rc ) );
		apr_pool_destroy( p );
	}
	else
	{
		server->p = p;
		server->sock->parent = (void*)server;
		server->sock->type = SOCKET_AS_SERVER;
		// server->timeout = 0;
		server->reqs = apr_hash_make( p );
		server->userdata = userdata;
		
		apr_pool_cleanup_register( p, (void*)server, SocketServerDestroy, apr_pool_cleanup_null );
		
		// bind
		if( bind( server->sock->fd, (struct sockaddr*)&(server->sock->addr), sizeof( server->sock->addr ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to bind(): %s", kahanaLogErr2Str( ETYPE_SYS, errno ) );
		}
		// Set the socket to non-blocking, this is essential in
		// event based programming with libevent.
		else if( ( rc = SockOptSetNonBlock( server->sock->fd ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to SockOptSetNonBlock(): %s", kahanaLogErr2Str( ETYPE_SYS, errno ) );
		}
		// listen
		else if( ( rc = listen( server->sock->fd, SOMAXCONN ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to listen(): %s", kahanaLogErr2Str( ETYPE_SYS, errno ) );
		}
		// nothing problem
		else
		{
			/*
			if( kahanaSocketOptSetTimeout( server->sock, SO_RCVTIMEO, 10, 10 ) != 0 ){
				kahanaLogPut( NULL, NULL, "failed to kahanaSocketOptSetTimeout(): %s", kahanaLogErr2Str( ETYPE_SYS, errno ) ); 
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

