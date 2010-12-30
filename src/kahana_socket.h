#ifndef ___KAHANA_SOCKET___
#define ___KAHANA_SOCKET___

#include "kahana.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include "event.h"


#define cbSocketNULL NULL

/* opaque: socket structure */
typedef struct kSocket_t kSocket_t;
typedef struct kSocketServer_t kSocketServer_t;
typedef struct kSocketRequest_t kSocketRequest_t;

typedef enum {
	SOCKET_AS_UNKNOWN = -1,
	SOCKET_AS_CLIENT = 0,
	SOCKET_AS_SERVER,
	SOCKET_AS_REQUEST
} kSocketType_e;

/* callbacks */
typedef const char *(*cbSocketOnErr)( void );
typedef bool (*cbSocketOnRecv)( kSocketRequest_t *req, const char *buf, ssize_t len );
typedef int (*cbSocketOnSend)( kSocketRequest_t *req );
typedef const char *(*cbSocketOnTimeout)( kSocketRequest_t *req );
typedef const char *(*cbSocketOnSignal)( int signum, short event, void *sock, kSocketType_e type );

/* server socket */
struct kSocketServer_t {
	apr_pool_t *p;
	kSocket_t *sock;
	apr_hash_t *reqs;
	uint64_t nbytes;
	struct timeval timeout;
	// thread pool
	apr_thread_pool_t *thdp;
	// user data
	void *userdata;
};

/* request socket */
struct kSocketRequest_t{
	apr_thread_t *thd;
	apr_pool_t *p;
	kSocketServer_t *server;
	const char *key;
	struct sockaddr_in addr;
	int fd;
	int rcvbuf;
	int sndbuf;
	// event
	struct event_base *loop;
	struct event io_ev;
	struct event timer_ev;
	// context
	void *ctx;
};



/* kSocket_t */
/* ai_flags = AI_PASSIVE, ai_family = AF_INET, ai_socktype = SOCK_STREAM */
apr_status_t kahanaSocketCreate( apr_pool_t *p, kSocket_t **newsock, const char *host, const char *port, int ai_flags, int ai_family, int ai_socktype );
/* set and get socket options */
/* recv size */
int kahanaSocketOptGetRcvBuf( kSocket_t *sock );
int kahanaSocketOptSetRcvBuf( kSocket_t *sock, int bytes );
struct timeval kahanaSocketOptGetRcvTimeout( kSocket_t *sock );
int kahanaSocketOptSetRcvTimeout( kSocket_t *sock, time_t sec, time_t usec );
/* send size */
int kahanaSocketOptGetSndBuf( kSocket_t *sock );
struct timeval kahanaSocketOptGetSndTimeout( kSocket_t *sock );
int kahanaSocketOptSetSndTimeout( kSocket_t *sock, time_t sec, time_t usec );
/* timeout */
struct timeval kahanaSocketOptGetTimeout( kSocket_t *sock, int opt );
int kahanaSocketOptSetTimeout( kSocket_t *sock, int opt, time_t sec, time_t usec );
/* callbacks */
// apr_status_t kahanaSocketSetOnSignal( kSocket_t *sock, int signum, cbSocketOnSignal cb_signal );
void kahanaSocketSetOnRecv( kSocket_t *sock, cbSocketOnRecv cb_recv );
void kahanaSocketSetOnSend( kSocket_t *sock, cbSocketOnSend cb_send );
void kahanaSocketSetOnTimeout( kSocket_t *sock, cbSocketOnTimeout cb_timeout );
void kahanaSocketSetOnErr( kSocket_t *sock, cbSocketOnErr cb_err );
/* dispatch */
int kahanaSocketLoop( kSocket_t *sock, int flg );
int kahanaSocketUnloop( kSocket_t *sock );

/* kSocketServer_t */
apr_status_t kahanaSocketServerCreate( apr_pool_t *pp, kSocketServer_t **newserver, const char *host, const char *port, size_t thd_max, int ai_flags, int ai_family, int ai_socktype, void *userdata );
void kahanaSocketServerDestroy( kSocketServer_t *server );
/* request timeout */
void kahanaSocketServerSetTimeout( kSocketServer_t *server, struct timeval timeout );

#endif
