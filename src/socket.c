/*
	printLog( NULL, "_SC_NPROCESSORS_ONLN: %d", sysconf( _SC_NPROCESSORS_ONLN ) );
	printLog( NULL, "_SC_CHILD_MAX: %d", sysconf( _SC_CHILD_MAX ) );
	printLog( NULL, "_SC_OPEN_MAX: %d", sysconf( _SC_OPEN_MAX ) );
	printLog( NULL, "_SC_LINE_MAX: %d", sysconf( _SC_LINE_MAX ) );
*/

#include "kahana_socket.h"

// BSD socket
struct kSocket_t {
	apr_pool_t *p;
	kSocketType_e type;
	int fd;
	struct sockaddr_in addr;
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
	// thread pool
	apr_thread_pool_t *thp;
	// event
	int evflg;
	struct event_base *loop;
	struct event sig_ev[NSIG];
	// user data
	void *userdata;
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

static apr_status_t DestroySocket( void *data )
{
	kSocket_t *sock = (kSocket_t*)data;
	
	if( sock )
	{
		if( apr_thread_pool_threads_count( sock->thp ) ){
			raise( SIGUSR1 );
		}
		
		// socket descriptor
		if( sock->fd ){
			close( sock->fd );
			sock->fd = 0;
		}
		// event loop
		if( sock->loop ){
			event_base_free( sock->loop );
			sock->loop = NULL;
		}
		// thread pool
		if( sock->thp ){
			apr_thread_pool_destroy( sock->thp );
		}
	}
	
	return APR_SUCCESS;
}


apr_status_t kahanaSockCreate( apr_pool_t *pp, kSocket_t **newsock, const char *host, const char *port, int ai_flags, int ai_family, int ai_socktype, void *userdata )
{
	int rc = 0;
	kSocket_t *sock = NULL;
	apr_pool_t *p = NULL;
	struct addrinfo hints, *res;
	
	memset( (void*)&hints, 0, sizeof( hints ) );
	hints.ai_family = ai_family;
	hints.ai_socktype = ai_socktype;
	hints.ai_flags = ai_flags;
	hints.ai_protocol = 0;
	
	// allocate
	if( ( rc = kahanaMalloc( pp, sizeof( kSocket_t ), (void*)&sock, &p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
	}
	// create event loop
	else if( !( sock->loop = event_base_new() ) ){
		kahanaLogPut( NULL, NULL, "failed to event_base_new()" );
		rc = APR_EGENERAL;
		apr_pool_destroy( p );
	}
	// get address info
	else if( ( rc = getaddrinfo( host, port, &hints, &res ) ) != 0 ){
		kahanaLogPut( NULL, NULL, "failed to getaddrinfo(): %s", gai_strerror( rc ) );
		apr_pool_destroy( p );
	}
	else
	{
		struct addrinfo *ptr = res;
		int sockopt = 1;
		
		// find addrinfo
		do
		{
			// create socket descriptor
			if( ( sock->fd = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol ) ) != -1 ){
				// set address info
				memcpy( (void*)&sock->addr, (void*)ptr->ai_addr, ptr->ai_addrlen );
				break;
			}
		} while( ( ptr = ptr->ai_next ) );
		freeaddrinfo( res );
		
		if( sock->fd == -1 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to socket(): %s", strerror( errno ) );
		}
		// set sockopt SO_REUSEADDR
		else if( setsockopt( sock->fd, SOL_SOCKET, SO_REUSEADDR, (char*)&sockopt, sizeof( sockopt ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to setsockopt(): %s", strerror( errno ) );
		}
		// bind
		else if( bind( sock->fd, (struct sockaddr*)&(sock->addr), sizeof( sock->addr ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to bind(): %s", strerror( errno ) );
		}
		/*
		// Set the socket to non-blocking, this is essential in
		// event based programming with libevent.
		else if( ( rc = SockOptSetNonBlock( sock->fd ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to SockOptSetNonBlock(): %s", strerror( errno ) );
		}
		*/
		// listen
		else if( ( rc = listen( sock->fd, SOMAXCONN ) ) != 0 ){
			rc = errno;
			kahanaLogPut( NULL, NULL, "failed to listen(): %s", strerror( errno ) );
		}
		// no problem
		// create socket structure
		else
		{
			socklen_t len = 0;
			
			// initialize
			sock->p = p;
			sock->type = SOCKET_AS_UNKNOWN;
			sock->cb_err = NULL;
			sock->cb_recv = NULL;
			sock->cb_send = NULL;
			sock->cb_timeout = NULL;
			sock->evflg = 0;
			sock->thp = NULL;
			sock->userdata = userdata;
			apr_pool_cleanup_register( p, (void*)sock, DestroySocket, apr_pool_cleanup_null );
			
			// get default receive buffer size
			if( ( len = sizeof( int ) ) &&
				( getsockopt( sock->fd, SOL_SOCKET, SO_RCVBUF, (void*)&sock->buf_rcv, &len ) == -1 ) ){
				kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", strerror( errno ) );
			}
			// get default send buffer size
			else if( ( len = sizeof( int ) ) &&
					 ( getsockopt( sock->fd, SOL_SOCKET, SO_SNDBUF, (void*)&sock->buf_snd, &len ) == -1 ) ){
				kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", strerror( errno ) );
			}
			// get default receive timeout
			else if( ( len = sizeof( struct timeval ) ) &&
					 ( getsockopt( sock->fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&sock->tval_rcv, &len ) == -1 ) ){
				kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", strerror( errno ) );
			}
			// get default send timeout
			else if( ( len = sizeof( struct timeval ) ) &&
					 ( getsockopt( sock->fd, SOL_SOCKET, SO_SNDTIMEO, (void*)&sock->tval_snd, &len ) == -1 ) ){
				kahanaLogPut( NULL, NULL, "failed to getsockopt(): %d", strerror( errno ) );
			}
			/*
			if( kahanaSockOptSetTimeout( sock, SO_RCVTIMEO, 10, 10 ) != 0 ){
				kahanaLogPut( NULL, NULL, "failed to kahanaSockOptSetTimeout(): %s", strerror( errno ) ); 
			}
			fprintf( stderr, "buf_rcv -> %d\nbuf_snd -> %d\ntval_rcv -> %lu.%u\ntval_snd -> %lu.%u\nSOMAXCONN -> %d\n", 
					sock->buf_rcv, sock->buf_snd, 
					sock->tval_rcv.tv_sec, sock->tval_rcv.tv_usec,
					sock->tval_snd.tv_sec, sock->tval_snd.tv_usec,
					SOMAXCONN );
			*/
			*newsock = sock;
		}
		
		if( rc ){
			close( sock->fd );
			apr_pool_destroy( p );
		}
	}
	
	return rc;
}

void kahanaSockDestroy( kSocket_t *sock )
{
	if( sock ){
		apr_pool_destroy( sock->p );
	}
}


// MARK: Request Handler

static void SocketRecv( int fd, short event, void *arg )
{
	kSockWorker_t *worker = (kSockWorker_t*)arg;
	char buf[worker->sock->buf_rcv];
	// buf CR+LF
	ssize_t len = 0;
	
	if( ( len = recv( worker->fd, buf, sizeof( buf ), 0 ) ) == 0 ){
		fprintf( stderr, "recv(): nolen(): %d, %s\n", errno, strerror( errno ) );
		close( worker->fd );
		event_del( &worker->req_ev );
		worker->fd = 0;
		// reschedule sock event
		event_add( &worker->sock_ev, NULL );
	}
	else if( len == -1 )
	{
		fprintf( stderr, "recv(): len %lu\n", len );
		// EAGAIN if non-blocking mode
		switch( errno )
		{
			case EAGAIN:
				// reschedule req event
				event_add( &worker->req_ev, NULL );
			break;
			
			default:
				fprintf( stderr, "failed to recv(): %d, %s\n", errno, strerror( errno ) );
				close( worker->fd );
				event_del( &worker->req_ev );
				worker->fd = 0;
				// reschedule sock event
				event_add( &worker->sock_ev, NULL );
		}
	}
	// no more receive
	else if( !worker->sock->cb_recv( worker, buf, len ) ){
		shutdown( worker->fd, SHUT_RD );
		worker->sock->cb_send( worker );
		close( worker->fd );
		event_del( &worker->req_ev );
		worker->fd = 0;
		// reschedule sock event
		event_add( &worker->sock_ev, NULL );
	}
	// reschedule request event
	else{
		event_add( &worker->req_ev, NULL );
	}
}

static void SocketAccept( int sfd, short event, void *arg )
{
	kSockWorker_t *worker = (kSockWorker_t*)arg;
	apr_status_t rc = APR_SUCCESS;
	socklen_t len = sizeof( struct sockaddr_in );
	
	// wait while client connection
	if( ( worker->fd = accept( worker->sock->fd, (struct sockaddr*)&worker->addr, &len ) ) == -1 ){
		// reschedule sock event
		event_add( &worker->sock_ev, NULL );
	}
	// Set the socket to non-blocking, this is essential in
	// event based programming with libevent.
	else if( SockOptSetNonBlock( worker->fd ) != 0 ){
		kahanaLogPut( NULL, NULL, "failed to SockOptSetNonBlock(): %s", strerror( errno ) );
		shutdown( worker->fd, SHUT_RD );
		// call error callback
		if( worker->sock->cb_err ){
			const char *errstr = worker->sock->cb_err();
			send( worker->fd, errstr, strlen( errstr ), 0 );
		}
		close( worker->fd );
		worker->fd = 0;
		// reschedule sock event
		event_add( &worker->sock_ev, NULL );
	}
	else
	{
		/*
		// get default receive/send buffer size
		len = sizeof( int );
		if( getsockopt( worker->fd, SOL_SOCKET, SO_RCVBUF, (void*)&worker->rcvbuf, &len ) == -1 ){
			worker->rcvbuf = HUGE_STRING_LEN;
		}
		// get default receive/send buffer size
		len = sizeof( int );
		if( getsockopt( worker->fd, SOL_SOCKET, SO_SNDBUF, (void*)&worker->sndbuf, &len ) == -1 ){
			worker->sndbuf = HUGE_STRING_LEN;
		}
		*/
		// init req event
		event_set( &worker->req_ev, worker->fd, EV_READ, SocketRecv, arg );
		event_base_set( worker->loop, &worker->req_ev );
		// add with no timeout
		if( ( rc = event_add( &worker->req_ev, NULL ) ) ){
			kahanaLogPut( NULL, NULL, "failed to event_add()" );
		}
	}
}


static apr_status_t DestroyWorker( void *data )
{
	kSockWorker_t *worker = (kSockWorker_t*)data;
	
	if( worker->loop ){
		raise( SIGUSR1 );
	}
	
	return APR_SUCCESS;
}

static void WorkerSignal( int signum, short event, void *arg )
{
	kSockWorker_t *worker = (kSockWorker_t*)arg;
	event_base_loopbreak( worker->loop );
}

apr_status_t SetWorkerSignal( kSockWorker_t *worker, int signum )
{
	apr_status_t rc = APR_SUCCESS;
	
	if( signum >= NSIG ){
		rc = APR_EINVAL;
	}
	else
	{
		// set io event
		event_set( &worker->sig_ev, signum, EV_SIGNAL|EV_PERSIST, WorkerSignal, (void*)worker );
		event_base_set( worker->loop, &worker->sig_ev );
		// add no timeout
		if( ( rc = event_add( &worker->sig_ev, NULL ) ) ){
			kahanaLogPut( NULL, NULL, "failed to event_add()" );
		}
		else {
			rc = APR_SUCCESS;
		}
	}
	
	return rc;
}

static void *WorkerThread( apr_thread_t *thd, void *arg )
{
	kSockWorker_t *worker = (kSockWorker_t*)arg;
	apr_status_t rc = APR_SUCCESS;
	
	worker->thd = thd;
	// add signal events
	if( ( rc = SetWorkerSignal( worker, SIGUSR1 ) ) ){
		kahanaLogPut( NULL, NULL, "failed to SetWorkerSignal(): %s\n", STRERROR_APR( rc ) );
	}
	else
	{
		// init sock event
		event_set( &worker->sock_ev, worker->sock->fd, EV_READ, SocketAccept, arg );
		// ev_set_priority( &req->watch_fd, 0 );
		event_base_set( worker->loop, &worker->sock_ev );
		// add with no timeout
		if( event_add( &worker->sock_ev, NULL ) == -1 ){
			kahanaLogPut( NULL, NULL, "failed to event_add()" );
		}
		// block dispatch
		else{
			apr_pool_cleanup_register( worker->p, (void*)worker, DestroyWorker, apr_pool_cleanup_null );
			event_base_loop( worker->loop, 0 );
		}
		if( worker->fd ){
			close( worker->fd );
			worker->fd = 0;
		}
		event_del( &worker->sig_ev );
		event_del( &worker->req_ev );
		event_del( &worker->sock_ev );
		event_base_free( worker->loop );
		worker->loop = NULL;
	}
	
	return NULL;
}

// MARK: Socket Loop And Unloop
static void SocketSignal( int signum, short event, void *arg )
{
	kSocket_t *sock = (kSocket_t*)arg;
	cbSocketOnSignal callback = sock->cb_signal[signum];
	
	if( callback ){
		callback( signum, sock, sock->userdata );
	}
}

/*
int flg: read ev.h
	0,				blocking
	EVLOOP_ONCE		block *once* only
	EVLOOP_NONBLOCK	do not block/wait
*/
apr_status_t kahanaSockLoop( kSocket_t *sock, apr_size_t nth, int flg )
{
	apr_status_t rc = APR_SUCCESS;
	
	// create thread-pool
	nth = ( nth ) ? nth : 1;
	if( ( rc = apr_thread_pool_create( &sock->thp, 0, nth, sock->p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to apr_thread_pool_create(): %s", STRERROR_APR( rc ) );
	}
	else
	{
		apr_size_t i;
		
		sock->evflg = flg;
		for( i = 1; i <= nth; i++ )
		{
			kSockWorker_t *nworker = NULL;
			apr_pool_t *p = NULL;
			
			// allocate kSockWorker_t
			if( ( rc = kahanaMalloc( sock->p, sizeof( kSockWorker_t ), (void**)&nworker, &p ) ) ){
				kahanaLogPut( NULL, NULL, "failed to kahanaMalloc() reasons %s", STRERROR_APR( rc ) );
				break;
			}
			// create event loop
			else if( !( nworker->loop = event_base_new() ) ){
				kahanaLogPut( NULL, NULL, "failed to event_base_new()" );
				rc = APR_EGENERAL;
				apr_pool_destroy( p );
				break;
			}
			else
			{
				nworker->p = p;
				nworker->nth = i;
				nworker->sock = sock;
				nworker->next = NULL;
				// create worker thread
				if( ( rc = apr_thread_pool_push( sock->thp, WorkerThread, (void*)nworker, APR_THREAD_TASK_PRIORITY_NORMAL, NULL ) ) ){
					kahanaLogPut( NULL, NULL, "failed to apr_thread_pool_push(): %s", STRERROR_APR( rc ) );
					event_base_free( nworker->loop );
					apr_pool_destroy( p );
					break;
				}
			}
		}
		
		if( rc ){
			raise( SIGUSR1 );
			apr_thread_pool_destroy( sock->thp );
		}
		// block dispatch
		else if( ( rc = event_base_loop( sock->loop, 0 ) ) == -1 ){
			kahanaLogPut( NULL, NULL, "failed to event_base_loop()" );
		}
	}
	
	return rc;
}

int kahanaSockUnloop( kSocket_t *sock )
{
	raise( SIGUSR1 );
	apr_thread_pool_destroy( sock->thp );
	return event_base_loopbreak( sock->loop );
}


// MARK: Socket Option
int kahanaSockOptGetRcvBuf( kSocket_t *sock )
{
	return sock->buf_rcv;
}

int kahanaSockOptSetRcvBuf( kSocket_t *sock, int bytes )
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

struct timeval kahanaSockOptGetRcvTimeout( kSocket_t *sock )
{
	return sock->tval_rcv;
}

int kahanaSockOptSetRcvTimeout( kSocket_t *sock, time_t sec, time_t usec )
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

int kahanaSockOptGetSndBuf( kSocket_t *sock )
{
	return sock->buf_snd;
}
struct timeval kahanaSockOptGetSndTimeout( kSocket_t *sock )
{
	return sock->tval_snd;
}

int kahanaSockOptSetSndTimeout( kSocket_t *sock, time_t sec, time_t usec )
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
apr_status_t kahanaSockSetOnSignal( kSocket_t *sock, int signum, cbSocketOnSignal cb_signal )
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

void kahanaSockSetOnErr( kSocket_t *sock, cbSocketOnErr cb_err )
{
	sock->cb_err = cb_err;
}

void kahanaSockSetOnRecv( kSocket_t *sock, cbSocketOnRecv cb_recv )
{
	sock->cb_recv = cb_recv;
}

void kahanaSockSetOnSend( kSocket_t *sock, cbSocketOnSend cb_send )
{
	sock->cb_send = cb_send;
}
void kahanaSockSetOnTimeout( kSocket_t *sock, cbSocketOnTimeout cb_timeout )
{
	sock->cb_timeout = cb_timeout;
}

/*
// MARK:  SERVER SOCKET

void kahanaSockServerSetTimeout( kSockServer_t *server, struct timeval timeout )
{
	server->timeout = timeout;
}

static apr_status_t SocketServerDestroy( void *data )
{
	kSockServer_t *server = (kSockServer_t*)data;
	
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

*/

/*
create socket and bind, listen
APR_EGENERAL
*/
/*
apr_status_t kahanaSockServerCreate( apr_pool_t *pp, kSockServer_t **newserver, const char *host, const char *port, size_t thd_max, int ai_flags, int ai_family, int ai_socktype, void *userdata )
{
	apr_status_t rc = APR_SUCCESS;
	apr_pool_t *p = NULL;
	kSockServer_t *server = NULL;
	
	// invalid params
	if( !host || !port ){
		rc = APR_EINVAL;
	}
	// allocate
	else if( ( rc = kahanaMalloc( pp, sizeof( kSockServer_t ), (void*)&server, &p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
	}
	// create thread-pool
	else if( ( rc = apr_thread_pool_create( &server->thdp, 0, thd_max, p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to apr_thread_pool_create(): %s", STRERROR_APR( rc ) );
		apr_pool_destroy( p );
	}
	// create socket
	else if( ( rc = kahanaSockCreate( p, &server->sock, host, port, ai_flags, ai_family, ai_socktype ) ) ){
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
			*newserver = server;
		}
		
		if( rc ){
			apr_pool_destroy( server->p );
		}
	}
	
	return rc;
}

void kahanaSockServerDestroy( kSockServer_t *server )
{
	if( server ){
		apr_pool_destroy( server->p );
	}
}
*/

