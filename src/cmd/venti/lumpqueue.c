#include "stdinc.h"
#include "dat.h"
#include "fns.h"

typedef struct LumpQueue	LumpQueue;
typedef struct WLump		WLump;

enum
{
	MaxLumpQ	= 1 << 3	/* max. lumps on a single write queue, must be pow 2 */
};

struct WLump
{
	Lump	*u;
	Packet	*p;
	int	creator;
	int	gen;
};

struct LumpQueue
{
	QLock	lock;
	Rendez 	flush;
	Rendez	full;
	Rendez	empty;
	WLump	q[MaxLumpQ];
	int	w;
	int	r;
};

static LumpQueue	*lumpqs;
static int		nqs;

static QLock		glk;
static int		gen;

static void	queueproc(void *vq);

int
initlumpqueues(int nq)
{
	LumpQueue *q;

	int i;
	nqs = nq;

	lumpqs = MKNZ(LumpQueue, nq);

	for(i = 0; i < nq; i++){
		q = &lumpqs[i];
		q->full.l = &q->lock;
		q->empty.l = &q->lock;
		q->flush.l = &q->lock;

		if(vtproc(queueproc, q) < 0){
			seterr(EOk, "can't start write queue slave: %r");
			return -1;
		}
	}

	return 0;
}

/*
 * queue a lump & it's packet data for writing
 */
int
queuewrite(Lump *u, Packet *p, int creator)
{
	LumpQueue *q;
	int i;

	i = indexsect(mainindex, u->score);
	if(i < 0 || i >= nqs){
		seterr(EBug, "internal error: illegal index section in queuewrite");
		return -1;
	}

	q = &lumpqs[i];

	qlock(&q->lock);
	while(q->r == ((q->w + 1) & (MaxLumpQ - 1)))
		rsleep(&q->full);

	q->q[q->w].u = u;
	q->q[q->w].p = p;
	q->q[q->w].creator = creator;
	q->q[q->w].gen = gen;
	q->w = (q->w + 1) & (MaxLumpQ - 1);

	rwakeup(&q->empty);

	qunlock(&q->lock);

	return 0;
}

void
flushqueue(void)
{
	int i;
	LumpQueue *q;

	qlock(&glk);
	gen++;
	qunlock(&glk);

	for(i=0; i<mainindex->nsects; i++){
		q = &lumpqs[i];
		qlock(&q->lock);
		while(q->w != q->r && gen - q->q[q->r].gen > 0)
			rsleep(&q->flush);
		qunlock(&q->lock);
	}
}
		
static void
queueproc(void *vq)
{
	LumpQueue *q;
	Lump *u;
	Packet *p;
	int creator;

	q = vq;
	for(;;){
		qlock(&q->lock);
		while(q->w == q->r)
			rsleep(&q->empty);

		u = q->q[q->r].u;
		p = q->q[q->r].p;
		creator = q->q[q->r].creator;

		rwakeup(&q->full);

		qunlock(&q->lock);

		if(writeqlump(u, p, creator) < 0)
			fprint(2, "failed to write lump for %V: %r", u->score);

		qlock(&q->lock);
		q->r = (q->r + 1) & (MaxLumpQ - 1);
		rwakeup(&q->flush);
		qunlock(&q->lock);

		putlump(u);
	}
}
