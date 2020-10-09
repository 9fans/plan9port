#include "stdinc.h"

#include "9.h"
#include "dat.h"
#include "fns.h"

enum {
	NConInit	= 128,
	NMsgInit	= 384,
	NMsgProcInit	= 64,
	NMsizeInit	= 8192+IOHDRSZ,
};

static struct {
	QLock	alock;			/* alloc */
	Msg*	ahead;
	Rendez	arendez;

	int	maxmsg;
	int	nmsg;
	int	nmsgstarve;

	QLock	rlock;			/* read */
	Msg*	rhead;
	Msg*	rtail;
	Rendez	rrendez;

	int	maxproc;
	int	nproc;
	int	nprocstarve;

	u32int	msize;			/* immutable */
} mbox;

static struct {
	QLock	alock;			/* alloc */
	Con*	ahead;
	Rendez	arendez;

	RWLock	clock;
	Con*	chead;
	Con*	ctail;

	int	maxcon;
	int	ncon;
	int	nconstarve;

	u32int	msize;
} cbox;

static void
conFree(Con* con)
{
	assert(con->version == nil);
	assert(con->mhead == nil);
	assert(con->whead == nil);
	assert(con->nfid == 0);
	assert(con->state == ConMoribund);

	if(con->fd >= 0){
		close(con->fd);
		con->fd = -1;
	}
	con->state = ConDead;
	con->aok = 0;
	con->flags = 0;
	con->isconsole = 0;

	qlock(&cbox.alock);
	if(con->cprev != nil)
		con->cprev->cnext = con->cnext;
	else
		cbox.chead = con->cnext;
	if(con->cnext != nil)
		con->cnext->cprev = con->cprev;
	else
		cbox.ctail = con->cprev;
	con->cprev = con->cnext = nil;

	if(cbox.ncon > cbox.maxcon){
		if(con->name != nil)
			vtfree(con->name);
		vtfree(con->data);
		vtfree(con);
		cbox.ncon--;
		qunlock(&cbox.alock);
		return;
	}
	con->anext = cbox.ahead;
	cbox.ahead = con;
	if(con->anext == nil)
		rwakeup(&cbox.arendez);
	qunlock(&cbox.alock);
}

static void
msgFree(Msg* m)
{
	assert(m->rwnext == nil);
	assert(m->flush == nil);

	qlock(&mbox.alock);
	if(mbox.nmsg > mbox.maxmsg){
		vtfree(m->data);
		vtfree(m);
		mbox.nmsg--;
		qunlock(&mbox.alock);
		return;
	}
	m->anext = mbox.ahead;
	mbox.ahead = m;
	if(m->anext == nil)
		rwakeup(&mbox.arendez);
	qunlock(&mbox.alock);
}

static Msg*
msgAlloc(Con* con)
{
	Msg *m;

	qlock(&mbox.alock);
	while(mbox.ahead == nil){
		if(mbox.nmsg >= mbox.maxmsg){
			mbox.nmsgstarve++;
			rsleep(&mbox.arendez);
			continue;
		}
		m = vtmallocz(sizeof(Msg));
		m->data = vtmalloc(mbox.msize);
		m->msize = mbox.msize;
		mbox.nmsg++;
		mbox.ahead = m;
		break;
	}
	m = mbox.ahead;
	mbox.ahead = m->anext;
	m->anext = nil;
	qunlock(&mbox.alock);

	m->con = con;
	m->state = MsgR;
	m->nowq = 0;

	return m;
}

static void
msgMunlink(Msg* m)
{
	Con *con;

	con = m->con;

	if(m->mprev != nil)
		m->mprev->mnext = m->mnext;
	else
		con->mhead = m->mnext;
	if(m->mnext != nil)
		m->mnext->mprev = m->mprev;
	else
		con->mtail = m->mprev;
	m->mprev = m->mnext = nil;
}

void
msgFlush(Msg* m)
{
	Con *con;
	Msg *flush, *old;

	con = m->con;

	if(Dflag)
		fprint(2, "msgFlush %F\n", &m->t);

	/*
	 * If this Tflush has been flushed, nothing to do.
	 * Look for the message to be flushed in the
	 * queue of all messages still on this connection.
	 * If it's not found must assume Elvis has already
	 * left the building and reply normally.
	 */
	qlock(&con->mlock);
	if(m->state == MsgF){
		qunlock(&con->mlock);
		return;
	}
	for(old = con->mhead; old != nil; old = old->mnext)
		if(old->t.tag == m->t.oldtag)
			break;
	if(old == nil){
		if(Dflag)
			fprint(2, "msgFlush: cannot find %d\n", m->t.oldtag);
		qunlock(&con->mlock);
		return;
	}

	if(Dflag)
		fprint(2, "\tmsgFlush found %F\n", &old->t);

	/*
	 * Found it.
	 * There are two cases where the old message can be
	 * truly flushed and no reply to the original message given.
	 * The first is when the old message is in MsgR state; no
	 * processing has been done yet and it is still on the read
	 * queue. The second is if old is a Tflush, which doesn't
	 * affect the server state. In both cases, put the old
	 * message into MsgF state and let MsgWrite toss it after
	 * pulling it off the queue.
	 */
	if(old->state == MsgR || old->t.type == Tflush){
		old->state = MsgF;
		if(Dflag)
			fprint(2, "msgFlush: change %d from MsgR to MsgF\n",
				m->t.oldtag);
	}

	/*
	 * Link this flush message and the old message
	 * so multiple flushes can be coalesced (if there are
	 * multiple Tflush messages for a particular pending
	 * request, it is only necessary to respond to the last
	 * one, so any previous can be removed) and to be
	 * sure flushes wait for their corresponding old
	 * message to go out first.
	 * Waiting flush messages do not go on the write queue,
	 * they are processed after the old message is dealt
	 * with. There's no real need to protect the setting of
	 * Msg.nowq, the only code to check it runs in this
	 * process after this routine returns.
	 */
	if((flush = old->flush) != nil){
		if(Dflag)
			fprint(2, "msgFlush: remove %d from %d list\n",
				old->flush->t.tag, old->t.tag);
		m->flush = flush->flush;
		flush->flush = nil;
		msgMunlink(flush);
		msgFree(flush);
	}
	old->flush = m;
	m->nowq = 1;

	if(Dflag)
		fprint(2, "msgFlush: add %d to %d queue\n",
			m->t.tag, old->t.tag);
	qunlock(&con->mlock);
}

static void
msgProc(void* v)
{
	Msg *m;
	char e[ERRMAX];
	Con *con;

	USED(v);
	threadsetname("msgProc");

	for(;;){
		/*
		 * If surplus to requirements, exit.
		 * If not, wait for and pull a message off
		 * the read queue.
		 */
		qlock(&mbox.rlock);
		if(mbox.nproc > mbox.maxproc){
			mbox.nproc--;
			qunlock(&mbox.rlock);
			break;
		}
		while(mbox.rhead == nil)
			rsleep(&mbox.rrendez);
		m = mbox.rhead;
		mbox.rhead = m->rwnext;
		m->rwnext = nil;
		qunlock(&mbox.rlock);

		con = m->con;
		*e = 0;

		/*
		 * If the message has been flushed before
		 * any 9P processing has started, mark it so
		 * none will be attempted.
		 */
		qlock(&con->mlock);
		if(m->state == MsgF)
			strcpy(e, "flushed");
		else
			m->state = Msg9;
		qunlock(&con->mlock);

		if(*e == 0){
			/*
			 * explain this
			 */
			qlock(&con->lock);
			if(m->t.type == Tversion){
				con->version = m;
				con->state = ConDown;
				while(con->mhead != m)
					rsleep(&con->rendez);
				assert(con->state == ConDown);
				if(con->version == m){
					con->version = nil;
					con->state = ConInit;
				}
				else
					strcpy(e, "Tversion aborted");
			}
			else if(con->state != ConUp)
				strcpy(e, "connection not ready");
			qunlock(&con->lock);
		}

		/*
		 * Dispatch if not error already.
		 */
		m->r.tag = m->t.tag;
		if(*e == 0 && !(*rFcall[m->t.type])(m))
			rerrstr(e, sizeof e);
		if(*e != 0){
			m->r.type = Rerror;
			m->r.ename = e;
		}
		else
			m->r.type = m->t.type+1;

		/*
		 * Put the message (with reply) on the
		 * write queue and wakeup the write process.
		 */
		if(!m->nowq){
			qlock(&con->wlock);
			if(con->whead == nil)
				con->whead = m;
			else
				con->wtail->rwnext = m;
			con->wtail = m;
			rwakeup(&con->wrendez);
			qunlock(&con->wlock);
		}
	}
}

static void
msgRead(void* v)
{
	Msg *m;
	Con *con;
	int eof, fd, n;

	threadsetname("msgRead");

	con = v;
	fd = con->fd;
	eof = 0;

	while(!eof){
		m = msgAlloc(con);

		n = read9pmsg(fd, m->data, con->msize);
		if(n <= 0){
			m->t.type = Tversion;
			m->t.fid = NOFID;
			m->t.tag = NOTAG;
			m->t.msize = con->msize;
			m->t.version = "9PEoF";
			eof = 1;
		}
		else if(convM2S(m->data, n, &m->t) != n){
			if(Dflag)
				fprint(2, "msgRead: convM2S error: %s\n",
					con->name);
			msgFree(m);
			continue;
		}
		if(Dflag)
			fprint(2, "msgRead %p: t %F\n", con, &m->t);

		qlock(&con->mlock);
		if(con->mtail != nil){
			m->mprev = con->mtail;
			con->mtail->mnext = m;
		}
		else{
			con->mhead = m;
			m->mprev = nil;
		}
		con->mtail = m;
		qunlock(&con->mlock);

		qlock(&mbox.rlock);
		if(mbox.rhead == nil){
			mbox.rhead = m;
			if(!rwakeup(&mbox.rrendez)){
				if(mbox.nproc < mbox.maxproc){
					if(proccreate(msgProc, nil, STACK) > 0)
						mbox.nproc++;
				}
				else
					mbox.nprocstarve++;
			}
			/*
			 * don't need this surely?
			rwakeup(&mbox.rrendez);
			 */
		}
		else
			mbox.rtail->rwnext = m;
		mbox.rtail = m;
		qunlock(&mbox.rlock);
	}
}

static void
msgWrite(void* v)
{
	Con *con;
	int eof, n;
	Msg *flush, *m;

	threadsetname("msgWrite");

	con = v;
	if(proccreate(msgRead, con, STACK) < 0){
		conFree(con);
		return;
	}

	for(;;){
		/*
		 * Wait for and pull a message off the write queue.
		 */
		qlock(&con->wlock);
		while(con->whead == nil)
			rsleep(&con->wrendez);
		m = con->whead;
		con->whead = m->rwnext;
		m->rwnext = nil;
		assert(!m->nowq);
		qunlock(&con->wlock);

		eof = 0;

		/*
		 * Write each message (if it hasn't been flushed)
		 * followed by any messages waiting for it to complete.
		 */
		qlock(&con->mlock);
		while(m != nil){
			msgMunlink(m);

			if(Dflag)
				fprint(2, "msgWrite %d: r %F\n",
					m->state, &m->r);

			if(m->state != MsgF){
				m->state = MsgW;
				qunlock(&con->mlock);

				n = convS2M(&m->r, con->data, con->msize);
				if(write(con->fd, con->data, n) != n)
					eof = 1;

				qlock(&con->mlock);
			}

			if((flush = m->flush) != nil){
				assert(flush->nowq);
				m->flush = nil;
			}
			msgFree(m);
			m = flush;
		}
		qunlock(&con->mlock);

		qlock(&con->lock);
		if(eof && con->fd >= 0){
			close(con->fd);
			con->fd = -1;
		}
		if(con->state == ConDown)
			rwakeup(&con->rendez);
		if(con->state == ConMoribund && con->mhead == nil){
			qunlock(&con->lock);
			conFree(con);
			break;
		}
		qunlock(&con->lock);
	}
}

Con*
conAlloc(int fd, char* name, int flags)
{
	Con *con;
	char buf[128], *p;
	int rfd, n;

	qlock(&cbox.alock);
	while(cbox.ahead == nil){
		if(cbox.ncon >= cbox.maxcon){
			cbox.nconstarve++;
			rsleep(&cbox.arendez);
			continue;
		}
		con = vtmallocz(sizeof(Con));
		con->rendez.l = &con->lock;
		con->data = vtmalloc(cbox.msize);
		con->msize = cbox.msize;
		con->mrendez.l = &con->mlock;
		con->wrendez.l = &con->wlock;

		cbox.ncon++;
		cbox.ahead = con;
		break;
	}
	con = cbox.ahead;
	cbox.ahead = con->anext;
	con->anext = nil;

	if(cbox.ctail != nil){
		con->cprev = cbox.ctail;
		cbox.ctail->cnext = con;
	}
	else{
		cbox.chead = con;
		con->cprev = nil;
	}
	cbox.ctail = con;

	assert(con->mhead == nil);
	assert(con->whead == nil);
	assert(con->fhead == nil);
	assert(con->nfid == 0);

	con->state = ConNew;
	con->fd = fd;
	if(con->name != nil){
		vtfree(con->name);
		con->name = nil;
	}
	if(name != nil)
		con->name = vtstrdup(name);
	else
		con->name = vtstrdup("unknown");
	con->remote[0] = 0;
	snprint(buf, sizeof buf, "%s/remote", con->name);
	if((rfd = open(buf, OREAD)) >= 0){
		n = read(rfd, buf, sizeof buf-1);
		close(rfd);
		if(n > 0){
			buf[n] = 0;
			if((p = strchr(buf, '\n')) != nil)
				*p = 0;
			strecpy(con->remote, con->remote+sizeof con->remote, buf);
		}
	}
	con->flags = flags;
	con->isconsole = 0;
	qunlock(&cbox.alock);

	if(proccreate(msgWrite, con, STACK) < 0){
		conFree(con);
		return nil;
	}

	return con;
}

static int
cmdMsg(int argc, char* argv[])
{
	char *p;
	char *usage = "usage: msg [-m nmsg] [-p nproc]";
	int maxmsg, nmsg, nmsgstarve, maxproc, nproc, nprocstarve;

	maxmsg = maxproc = 0;

	ARGBEGIN{
	default:
		return cliError(usage);
	case 'm':
		p = ARGF();
		if(p == nil)
			return cliError(usage);
		maxmsg = strtol(argv[0], &p, 0);
		if(maxmsg <= 0 || p == argv[0] || *p != '\0')
			return cliError(usage);
		break;
	case 'p':
		p = ARGF();
		if(p == nil)
			return cliError(usage);
		maxproc = strtol(argv[0], &p, 0);
		if(maxproc <= 0 || p == argv[0] || *p != '\0')
			return cliError(usage);
		break;
	}ARGEND
	if(argc)
		return cliError(usage);

	qlock(&mbox.alock);
	if(maxmsg)
		mbox.maxmsg = maxmsg;
	maxmsg = mbox.maxmsg;
	nmsg = mbox.nmsg;
	nmsgstarve = mbox.nmsgstarve;
	qunlock(&mbox.alock);

	qlock(&mbox.rlock);
	if(maxproc)
		mbox.maxproc = maxproc;
	maxproc = mbox.maxproc;
	nproc = mbox.nproc;
	nprocstarve = mbox.nprocstarve;
	qunlock(&mbox.rlock);

	consPrint("\tmsg -m %d -p %d\n", maxmsg, maxproc);
	consPrint("\tnmsg %d nmsgstarve %d nproc %d nprocstarve %d\n",
		nmsg, nmsgstarve, nproc, nprocstarve);

	return 1;
}

static int
scmp(Fid *a, Fid *b)
{
	if(a == 0)
		return 1;
	if(b == 0)
		return -1;
	return strcmp(a->uname, b->uname);
}

static Fid*
fidMerge(Fid *a, Fid *b)
{
	Fid *s, **l;

	l = &s;
	while(a || b){
		if(scmp(a, b) < 0){
			*l = a;
			l = &a->sort;
			a = a->sort;
		}else{
			*l = b;
			l = &b->sort;
			b = b->sort;
		}
	}
	*l = 0;
	return s;
}

static Fid*
fidMergeSort(Fid *f)
{
	int delay;
	Fid *a, *b;

	if(f == nil)
		return nil;
	if(f->sort == nil)
		return f;

	a = b = f;
	delay = 1;
	while(a && b){
		if(delay)	/* easy way to handle 2-element list */
			delay = 0;
		else
			a = a->sort;
		if(b = b->sort)
			b = b->sort;
	}

	b = a->sort;
	a->sort = nil;

	a = fidMergeSort(f);
	b = fidMergeSort(b);

	return fidMerge(a, b);
}

static int
cmdWho(int argc, char* argv[])
{
	char *usage = "usage: who";
	int i, l1, l2, l;
	Con *con;
	Fid *fid, *last;

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND

	if(argc > 0)
		return cliError(usage);

	rlock(&cbox.clock);
	l1 = 0;
	l2 = 0;
	for(con=cbox.chead; con; con=con->cnext){
		if((l = strlen(con->name)) > l1)
			l1 = l;
		if((l = strlen(con->remote)) > l2)
			l2 = l;
	}
	for(con=cbox.chead; con; con=con->cnext){
		consPrint("\t%-*s %-*s", l1, con->name, l2, con->remote);
		qlock(&con->fidlock);
		last = nil;
		for(i=0; i<NFidHash; i++)
			for(fid=con->fidhash[i]; fid; fid=fid->hash)
				if(fid->fidno != NOFID && fid->uname){
					fid->sort = last;
					last = fid;
				}
		fid = fidMergeSort(last);
		last = nil;
		for(; fid; last=fid, fid=fid->sort)
			if(last==nil || strcmp(fid->uname, last->uname) != 0)
				consPrint(" %q", fid->uname);
		qunlock(&con->fidlock);
		consPrint("\n");
	}
	runlock(&cbox.clock);
	return 1;
}

void
msgInit(void)
{
	mbox.arendez.l = &mbox.alock;

	mbox.rrendez.l = &mbox.rlock;

	mbox.maxmsg = NMsgInit;
	mbox.maxproc = NMsgProcInit;
	mbox.msize = NMsizeInit;

	cliAddCmd("msg", cmdMsg);
}

static int
cmdCon(int argc, char* argv[])
{
	char *p;
	Con *con;
	char *usage = "usage: con [-m ncon]";
	int maxcon, ncon, nconstarve;

	maxcon = 0;

	ARGBEGIN{
	default:
		return cliError(usage);
	case 'm':
		p = ARGF();
		if(p == nil)
			return cliError(usage);
		maxcon = strtol(argv[0], &p, 0);
		if(maxcon <= 0 || p == argv[0] || *p != '\0')
			return cliError(usage);
		break;
	}ARGEND
	if(argc)
		return cliError(usage);

	wlock(&cbox.clock);
	if(maxcon)
		cbox.maxcon = maxcon;
	maxcon = cbox.maxcon;
	ncon = cbox.ncon;
	nconstarve = cbox.nconstarve;
	wunlock(&cbox.clock);

	consPrint("\tcon -m %d\n", maxcon);
	consPrint("\tncon %d nconstarve %d\n", ncon, nconstarve);

	rlock(&cbox.clock);
	for(con = cbox.chead; con != nil; con = con->cnext){
		consPrint("\t%s\n", con->name);
	}
	runlock(&cbox.clock);

	return 1;
}

void
conInit(void)
{
	cbox.arendez.l = &cbox.alock;

	cbox.maxcon = NConInit;
	cbox.msize = NMsizeInit;

	cliAddCmd("con", cmdCon);
	cliAddCmd("who", cmdWho);
}
