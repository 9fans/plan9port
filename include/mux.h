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
	int async;
};

struct Mux
{
	uint mintag;	/* to be filled by client */
	uint maxtag;
	int (*send)(Mux*, void*);
	void *(*recv)(Mux*);
	int (*nbrecv)(Mux*, void**);
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
int	muxrpccanfinish(Muxrpc*, void**);

/* private */
int	_muxsend(Mux*, void*);
int	_muxrecv(Mux*, int, void**);
void	_muxsendproc(void*);
void	_muxrecvproc(void*);
Muxqueue *_muxqalloc(void);
int _muxqsend(Muxqueue*, void*);
void *_muxqrecv(Muxqueue*);
void _muxqhangup(Muxqueue*);
int _muxnbqrecv(Muxqueue*, void**);

#if defined(__cplusplus)
}
#endif
#endif
