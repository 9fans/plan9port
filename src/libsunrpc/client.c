/*
 * Sun RPC client.
 */
#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>

typedef struct Out Out;
struct Out
{
	char err[ERRMAX];	/* error string */
	Channel *creply;	/* send to finish rpc */
	uchar *p;			/* pending request packet */
	int n;				/* size of request */
	ulong tag;			/* flush tag of pending request */
	ulong xid;			/* xid of pending request */
	ulong st;			/* first send time */
	ulong t;			/* resend time */
	int nresend;		/* number of resends */
	SunRpc rpc;		/* response rpc */
};

static void
udpThread(void *v)
{
	uchar *p, *buf;
	Ioproc *io;
	int n;
	SunClient *cli;
	enum { BufSize = 65536 };

	cli = v;
	buf = emalloc(BufSize);
	io = ioproc();
	p = nil;
	for(;;){
		n = ioread(io, cli->fd, buf, BufSize);
		if(n <= 0)
			break;
		p = emalloc(4+n);
		memmove(p+4, buf, n);
		p[0] = n>>24;
		p[1] = n>>16;
		p[2] = n>>8;
		p[3] = n;
		if(sendp(cli->readchan, p) == 0)
			break;
		p = nil;
	}
	free(p);
	closeioproc(io);
	while(send(cli->dying, nil) == -1)
		;
}

static void
netThread(void *v)
{
	uchar *p, buf[4];
	Ioproc *io;
	uint n, tot;
	int done;
	SunClient *cli;

	cli = v;
	io = ioproc();
	tot = 0;
	p = nil;
	for(;;){
		n = ioreadn(io, cli->fd, buf, 4);
		if(n != 4)
			break;
		n = (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
		if(cli->chatty)
			fprint(2, "%.8ux...", n);
		done = n&0x80000000;
		n &= ~0x80000000;
		if(tot == 0){
			p = emalloc(4+n);
			tot = 4;
		}else
			p = erealloc(p, tot+n);
		if(ioreadn(io, cli->fd, p+tot, n) != n)
			break;
		tot += n;
		if(done){
			p[0] = tot>>24;
			p[1] = tot>>16;
			p[2] = tot>>8;
			p[3] = tot;
			if(sendp(cli->readchan, p) == 0)
				break;
			p = nil;
			tot = 0;
		}
	}
	free(p);
	closeioproc(io);
	while(send(cli->dying, 0) == -1)
		;
}

static void
timerThread(void *v)
{
	Ioproc *io;
	SunClient *cli;

	cli = v;
	io = ioproc();
	for(;;){
		if(iosleep(io, 200) < 0)
			break;
		if(sendul(cli->timerchan, 0) == 0)
			break;
	}
	closeioproc(io);
	while(send(cli->dying, 0) == -1)
		;
}

static ulong
msec(void)
{
	return nsec()/1000000;
}

static ulong
twait(ulong rtt, int nresend)
{
	ulong t;

	t = rtt;
	if(nresend <= 1)
		{}
	else if(nresend <= 3)
		t *= 2;
	else if(nresend <= 18)
		t <<= nresend-2;
	else
		t = 60*1000;
	if(t > 60*1000)
		t = 60*1000;

	return t;
}

static void
rpcMuxThread(void *v)
{
	uchar *buf, *p, *ep;
	int i, n, nout, mout;
	ulong t, xidgen, tag;
	Alt a[5];
	Out *o, **out;
	SunRpc rpc;
	SunClient *cli;

	cli = v;
	mout = 16;
	nout = 0;
	out = emalloc(mout*sizeof(out[0]));
	xidgen = truerand();

	a[0].op = CHANRCV;
	a[0].c = cli->rpcchan;
	a[0].v = &o;
	a[1].op = CHANNOP;
	a[1].c = cli->timerchan;
	a[1].v = nil;
	a[2].op = CHANRCV;
	a[2].c = cli->flushchan;
	a[2].v = &tag;
	a[3].op = CHANRCV;
	a[3].c = cli->readchan;
	a[3].v = &buf;
	a[4].op = CHANEND;

	for(;;){
		switch(alt(a)){
		case 0:	/* o = <-rpcchan */
			if(o == nil)
				goto Done;
			cli->nsend++;
			/* set xid */
			o->xid = ++xidgen;
			if(cli->needcount)
				p = o->p+4;
			else
				p = o->p;
			p[0] = xidgen>>24;
			p[1] = xidgen>>16;
			p[2] = xidgen>>8;
			p[3] = xidgen;
			if(write(cli->fd, o->p, o->n) != o->n){
				free(o->p);
				o->p = nil;
				snprint(o->err, sizeof o->err, "write: %r");
				sendp(o->creply, 0);
				break;
			}
			if(nout >= mout){
				mout *= 2;
				out = erealloc(out, mout*sizeof(out[0]));
			}
			o->st = msec();
			o->nresend = 0;
			o->t = o->st + twait(cli->rtt.avg, 0);
if(cli->chatty) fprint(2, "send %lux %lud %lud\n", o->xid, o->st, o->t);
			out[nout++] = o;
			a[1].op = CHANRCV;
			break;

		case 1:	/* <-timerchan */
			t = msec();
			for(i=0; i<nout; i++){
				o = out[i];
				if((int)(t - o->t) > 0){
if(cli->chatty) fprint(2, "resend %lux %lud %lud\n", o->xid, t, o->t);
					if(cli->maxwait && t - o->st >= cli->maxwait){
						free(o->p);
						o->p = nil;
						strcpy(o->err, "timeout");
						sendp(o->creply, 0);
						out[i--] = out[--nout];
						continue;
					}
					cli->nresend++;
					o->nresend++;
					o->t = t + twait(cli->rtt.avg, o->nresend);
					if(write(cli->fd, o->p, o->n) != o->n){
						free(o->p);
						o->p = nil;
						snprint(o->err, sizeof o->err, "rewrite: %r");
						sendp(o->creply, 0);
						out[i--] = out[--nout];
						continue;
					}
				}
			}
			/* stop ticking if no work; rpcchan will turn it back on */
			if(nout == 0)
				a[1].op = CHANNOP;
			break;

		case 2:	/* tag = <-flushchan */
			for(i=0; i<nout; i++){
				o = out[i];
				if(o->tag == tag){
					out[i--] = out[--nout];
					strcpy(o->err, "flushed");
					free(o->p);
					o->p = nil;
					sendp(o->creply, 0);
				}
			}
			break;

		case 3:	/* buf = <-readchan */
			p = buf;
			n = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
			p += 4;
			ep = p+n;
			if(sunrpcunpack(p, ep, &p, &rpc) != SunSuccess){
				fprint(2, "%s: in: %.*H unpack failed\n", argv0, n, buf+4);
				free(buf);
				break;
			}
			if(cli->chatty)
				fprint(2, "in: %B\n", &rpc);
			if(rpc.iscall){
				fprint(2, "did not get reply\n");
				free(buf);
				break;
			}
			o = nil;
			for(i=0; i<nout; i++){
				o = out[i];
				if(o->xid == rpc.xid)
					break;
			}
			if(i==nout){
				if(cli->chatty) fprint(2, "did not find waiting request\n");
				free(buf);
				break;
			}
			out[i] = out[--nout];
			free(o->p);
			o->p = nil;
			o->rpc = rpc;
			if(rpc.status == SunSuccess){
				o->p = buf;
			}else{
				o->p = nil;
				free(buf);
				sunerrstr(rpc.status);
				rerrstr(o->err, sizeof o->err);
			}
			sendp(o->creply, 0);
			break;
		}
	}
Done:
	free(out);
	sendp(cli->dying, 0);
}

SunClient*
sundial(char *address)
{
	int fd;
	SunClient *cli;

	if((fd = dial(address, 0, 0, 0)) < 0)
		return nil;

	cli = emalloc(sizeof(SunClient));
	cli->fd = fd;
	cli->maxwait = 15000;
	cli->rtt.avg = 1000;
	cli->dying = chancreate(sizeof(void*), 0);
	cli->rpcchan = chancreate(sizeof(Out*), 0);
	cli->timerchan = chancreate(sizeof(ulong), 0);
	cli->flushchan = chancreate(sizeof(ulong), 0);
	cli->readchan = chancreate(sizeof(uchar*), 0);
	if(strstr(address, "udp!")){
		cli->needcount = 0;
		cli->nettid = threadcreate(udpThread, cli, SunStackSize);
		cli->timertid = threadcreate(timerThread, cli, SunStackSize);
	}else{
		cli->needcount = 1;
		cli->nettid = threadcreate(netThread, cli, SunStackSize);
		/* assume reliable: don't need timer */
		/* BUG: netThread should know how to redial */
	}
	threadcreate(rpcMuxThread, cli, SunStackSize);

	return cli;
}

void
sunclientclose(SunClient *cli)
{
	int n;

	/*
	 * Threadints get you out of any stuck system calls
	 * or thread rendezvouses, but do nothing if the thread
	 * is in the ready state.  Keep interrupting until it takes.
	 */
	n = 0;
	if(!cli->timertid)
		n++;
	while(n < 2){
/*
		threadint(cli->nettid);
		if(cli->timertid)
			threadint(cli->timertid);
*/

		yield();
		while(nbrecv(cli->dying, nil) == 1)
			n++;
	}

	sendp(cli->rpcchan, 0);
	recvp(cli->dying);

	/* everyone's gone: clean up */
	close(cli->fd);
	chanfree(cli->flushchan);
	chanfree(cli->readchan);
	chanfree(cli->timerchan);
	free(cli);
}

void
sunclientflushrpc(SunClient *cli, ulong tag)
{
	sendul(cli->flushchan, tag);
}

void
sunclientprog(SunClient *cli, SunProg *p)
{
	if(cli->nprog%16 == 0)
		cli->prog = erealloc(cli->prog, (cli->nprog+16)*sizeof(cli->prog[0]));
	cli->prog[cli->nprog++] = p;
}

int
sunclientrpc(SunClient *cli, ulong tag, SunCall *tx, SunCall *rx, uchar **tofree)
{
	uchar *bp, *p, *ep;
	int i, n1, n2, n, nn;
	Out o;
	SunProg *prog;
	SunStatus ok;

	for(i=0; i<cli->nprog; i++)
		if(cli->prog[i]->prog == tx->rpc.prog && cli->prog[i]->vers == tx->rpc.vers)
			break;
	if(i==cli->nprog){
		werrstr("unknown sun rpc program %d version %d", tx->rpc.prog, tx->rpc.vers);
		return -1;
	}
	prog = cli->prog[i];

	if(cli->chatty){
		fprint(2, "out: %B\n", &tx->rpc);
		fprint(2, "\t%C\n", tx);
	}

	n1 = sunrpcsize(&tx->rpc);
	n2 = suncallsize(prog, tx);

	n = n1+n2;
	if(cli->needcount)
		n += 4;

	/*
	 * The dance with 100 is to leave some padding in case
	 * suncallsize is slightly underestimating.  If this happens,
	 * the pack will succeed and then we can give a good size
	 * mismatch error below.  Otherwise the pack fails with
	 * garbage args, which is less helpful.
	 */
	bp = emalloc(n+100);
	ep = bp+n+100;
	p = bp;
	if(cli->needcount){
		nn = n-4;
		p[0] = (nn>>24)|0x80;
		p[1] = nn>>16;
		p[2] = nn>>8;
		p[3] = nn;
		p += 4;
	}
	if((ok = sunrpcpack(p, ep, &p, &tx->rpc)) != SunSuccess
	|| (ok = suncallpack(prog, p, ep, &p, tx)) != SunSuccess){
		sunerrstr(ok);
		free(bp);
		return -1;
	}
	ep -= 100;
	if(p != ep){
		werrstr("rpc: packet size mismatch %d %ld %ld", n, ep-bp, p-bp);
		free(bp);
		return -1;
	}

	memset(&o, 0, sizeof o);
	o.creply = chancreate(sizeof(void*), 0);
	o.tag = tag;
	o.p = bp;
	o.n = n;

	sendp(cli->rpcchan, &o);
	recvp(o.creply);
	chanfree(o.creply);

	if(o.p == nil){
		werrstr("%s", o.err);
		return -1;
	}

	p = o.rpc.data;
	ep = p+o.rpc.ndata;
	rx->rpc = o.rpc;
	rx->rpc.proc = tx->rpc.proc;
	rx->rpc.prog = tx->rpc.prog;
	rx->rpc.vers = tx->rpc.vers;
	rx->type = (rx->rpc.proc<<1)|1;
	if(rx->rpc.status != SunSuccess){
		sunerrstr(rx->rpc.status);
		werrstr("unpack: %r");
		free(o.p);
		return -1;
	}

	if((ok = suncallunpack(prog, p, ep, &p, rx)) != SunSuccess){
		sunerrstr(ok);
		werrstr("unpack: %r");
		free(o.p);
		return -1;
	}

	if(cli->chatty){
		fprint(2, "in: %B\n", &rx->rpc);
		fprint(2, "in:\t%C\n", rx);
	}

	if(tofree)
		*tofree = o.p;
	else
		free(o.p);

	return 0;
}
