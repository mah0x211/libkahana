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
typedef struct kSockWorker_t kSockWorker_t;

typedef enum {
	SOCKET_AS_UNKNOWN = -1,
	SOCKET_AS_CLIENT = 0,
	SOCKET_AS_SERVER,
	SOCKET_AS_REQUEST
} kSocketType_e;

/* callbacks */
typedef const char *(*cbSocketOnErr)( void );
typedef bool (*cbSocketOnRecv)( kSockWorker_t *req, const char *buf, ssize_t len );
typedef int (*cbSocketOnSend)( kSockWorker_t *req );
typedef const char *(*cbSocketOnTimeout)( kSockWorker_t *req );
typedef const char *(*cbSocketOnSignal)( int signum, kSocket_t *sock, void *userdata );

/* server socket */
/*
// typedef struct kSockServer_t kSockServer_t;
struct kSockServer_t {
	apr_pool_t *p;
	kSocket_t *sock;
	apr_hash_t *reqs;
	struct timeval timeout;
	// thread pool
	apr_thread_pool_t *thdp;
	// user data
	void *userdata;
};
*/

/* request socket */
struct kSockWorker_t{
	apr_pool_t *p;
	apr_size_t nth;
	kSocket_t *sock;
	kSockWorker_t *next;
	apr_thread_t *thd;
	// event
	struct event_base *loop;
	struct event sig_ev;
	struct event sock_ev;
	struct event req_ev;
	// client socket
	struct sockaddr_in addr;
	int fd;
	int rcvbuf;
	int sndbuf;
	// context
	void *ctx;
};



/* kSocket_t */
/* ai_flags = AI_PASSIVE, ai_family = AF_INET, ai_socktype = SOCK_STREAM */
apr_status_t kahanaSockCreate( apr_pool_t *p, kSocket_t **newsock, const char *host, const char *port, int ai_flags, int ai_family, int ai_socktype, void *userdata );
void kahanaSockDestroy( kSocket_t *sock );
/* dispatch */
apr_status_t kahanaSockLoop( kSocket_t *sock, apr_size_t nth, int flg );
int kahanaSockUnloop( kSocket_t *sock );

/* set and get socket options */
/* recv size */
int kahanaSockOptGetRcvBuf( kSocket_t *sock );
int kahanaSockOptSetRcvBuf( kSocket_t *sock, int bytes );
struct timeval kahanaSockOptGetRcvTimeout( kSocket_t *sock );
int kahanaSockOptSetRcvTimeout( kSocket_t *sock, time_t sec, time_t usec );
/* send size */
int kahanaSockOptGetSndBuf( kSocket_t *sock );
struct timeval kahanaSockOptGetSndTimeout( kSocket_t *sock );
int kahanaSockOptSetSndTimeout( kSocket_t *sock, time_t sec, time_t usec );
/* timeout */
struct timeval kahanaSockOptGetTimeout( kSocket_t *sock, int opt );
int kahanaSockOptSetTimeout( kSocket_t *sock, int opt, time_t sec, time_t usec );
/* callbacks */
apr_status_t kahanaSockSetOnSignal( kSocket_t *sock, int signum, cbSocketOnSignal cb_signal );
void kahanaSockSetOnRecv( kSocket_t *sock, cbSocketOnRecv cb_recv );
void kahanaSockSetOnSend( kSocket_t *sock, cbSocketOnSend cb_send );
void kahanaSockSetOnTimeout( kSocket_t *sock, cbSocketOnTimeout cb_timeout );
void kahanaSockSetOnErr( kSocket_t *sock, cbSocketOnErr cb_err );

/* kSockServer_t */
//apr_status_t kahanaSockServerCreate( apr_pool_t *pp, kSockServer_t **newserver, const char *host, const char *port, size_t thd_max, int ai_flags, int ai_family, int ai_socktype, void *userdata );
//void kahanaSockServerDestroy( kSockServer_t *server );
/* request timeout */
//void kahanaSockServerSetTimeout( kSockServer_t *server, struct timeval timeout );

#endif
