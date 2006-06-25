#ifndef _MUX_H_
#define _MUX_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

AUTOLIB(mux)

typedef struct Mux Mux;
typedef struct Muxrpc Muxrpc;
typedef struct Muxqueue Muxqueue;

struct Muxrpc
{
	Mux *mux;
	Muxrpc *next;
	Muxrpc *prev;
	Rendez r;
	uint tag;
	void *p;
	int waiting;
};

struct Mux
{
	uint mintag;	/* to be filled by client */
	uint maxtag;
	int (*send)(Mux*, void*);
	void *(*recv)(Mux*);
	void *(*nbrecv)(Mux*);
	int (*gettag)(Mux*, void*);
	int (*settag)(Mux*, void*, uint);
	void *aux;	/* for private use by client */

/* private */
	QLock lk;	/* must be first for muxinit */
	QLock inlk;
	QLock outlk;
	Rendez tagrend;
	Rendez rpcfork;
	Muxqueue *readq;
	Muxqueue *writeq;
	uint nwait;
	uint mwait;
	uint freetag;
	Muxrpc **wait;
	Muxrpc *muxer;
	Muxrpc sleep;
};

void	muxinit(Mux*);
void*	muxrpc(Mux*, void*);
void	muxprocs(Mux*);
Muxrpc*	muxrpcstart(Mux*, void*);
void*	muxrpccanfinish(Muxrpc*);

/* private */
int	_muxsend(Mux*, void*);
void*	_muxrecv(Mux*, int);
void	_muxsendproc(void*);
void	_muxrecvproc(void*);
Muxqueue *_muxqalloc(void);
int _muxqsend(Muxqueue*, void*);
void *_muxqrecv(Muxqueue*);
void _muxqhangup(Muxqueue*);
void *_muxnbqrecv(Muxqueue*);

#if defined(__cplusplus)
}
#endif
#endif
