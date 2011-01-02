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
typedef struct kSockWorkerCore_t kSockWorkerCore_t;

typedef enum {
	SOCK_OPT_NONBLOCK = 100,
	SOCKET_AS_UNKNOWN = -1,
	SOCKET_AS_CLIENT = 0,
	SOCKET_AS_SERVER,
	SOCKET_AS_REQUEST
} kSockErrno_e;

/* callbacks */
typedef void (*cbSocketOnErr)( kSockWorker_t *req, int fd, kSockErrno_e errnum );
typedef bool (*cbSocketOnRecv)( kSockWorker_t *req, const char *buf, ssize_t len );
typedef void (*cbSocketOnSend)( kSockWorker_t *req, int fd );
typedef void (*cbSocketOnTimeout)( kSockWorker_t *req, int fd );
typedef void (*cbSocketOnSignal)( int signum, kSocket_t *sock, void *userdata );

/* socket worker */
struct kSockWorker_t{
	apr_pool_t *p;
	apr_size_t nth;
	kSocket_t *sock;
	apr_thread_t *thd;
	kSockWorkerCore_t *core;
	// context
	void *ctx;
};

/* kSocket_t */
/* ai_flags = AI_PASSIVE, ai_family = AF_INET, ai_socktype = SOCK_STREAM */
apr_status_t kahanaSockCreate( apr_pool_t *p, kSocket_t **newsock, const char *host, const char *port, int ai_flags, int ai_family, int ai_socktype, void *userdata );
void kahanaSockDestroy( kSocket_t *sock );
/* dispatch */
apr_status_t kahanaSockLoop( kSocket_t *sock, apr_size_t nth, int sigth, int flg );
void kahanaSockUnloop( kSocket_t *sock );

/* set and get socket options */
/* recv size/timeout */
int kahanaSockOptGetRcvBuf( kSocket_t *sock );
int kahanaSockOptSetRcvBuf( kSocket_t *sock, int bytes );
struct timeval kahanaSockOptGetRcvTimeout( kSocket_t *sock );
int kahanaSockOptSetRcvTimeout( kSocket_t *sock, time_t sec, time_t usec );

/* send size/tuneiyt */
int kahanaSockOptGetSndBuf( kSocket_t *sock );
struct timeval kahanaSockOptGetSndTimeout( kSocket_t *sock );
int kahanaSockOptSetSndTimeout( kSocket_t *sock, time_t sec, time_t usec );

/* timeout */
struct timeval kahanaSockOptGetTimeout( kSocket_t *sock, int opt );
int kahanaSockOptSetTimeout( kSocket_t *sock, int opt, time_t sec, time_t usec );

/* callbacks */
apr_status_t kahanaSockSetOnSignal( kSocket_t *sock, int signum, cbSocketOnSignal cb_signal );
void kahanaSockSetOnErr( kSocket_t *sock, cbSocketOnErr cb_err );
void kahanaSockSetOnRecv( kSocket_t *sock, cbSocketOnRecv cb_recv );
void kahanaSockSetOnSend( kSocket_t *sock, cbSocketOnSend cb_send );
void kahanaSockSetOnTimeout( kSocket_t *sock, cbSocketOnTimeout cb_timeout );

#endif
