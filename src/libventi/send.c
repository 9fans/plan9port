#include <u.h>
#include <libc.h>
#include <venti.h>
#include "queue.h"

long ventisendbytes, ventisendpackets;
long ventirecvbytes, ventirecvpackets;

static int
_vtsend(VtConn *z, Packet *p)
{
	IOchunk ioc;
	int n, tot;
	uchar buf[4];

	if(z->state != VtStateConnected) {
		werrstr("session not connected");
		return -1;
	}

	/* add framing */
	n = packetsize(p);
	if(z->version[1] == '2') {
		if(n >= (1<<16)) {
			werrstr("packet too large");
			packetfree(p);
			return -1;
		}
		buf[0] = n>>8;
		buf[1] = n;
		packetprefix(p, buf, 2);
		ventisendbytes += n+2;
	} else {
		buf[0] = n>>24;
		buf[1] = n>>16;
		buf[2] = n>>8;
		buf[3] = n;
		packetprefix(p, buf, 4);
		ventisendbytes += n+4;
	}
	ventisendpackets++;

	tot = 0;
	for(;;){
		n = packetfragments(p, &ioc, 1, 0);
		if(n == 0)
			break;
		if(write(z->outfd, ioc.addr, ioc.len) < ioc.len){
			vtlog(VtServerLog, "<font size=-1>%T %s:</font> sending packet %p: %r<br>\n", z->addr, p);
			packetfree(p);
			return -1;
		}
		packetconsume(p, nil, ioc.len);
		tot += ioc.len;
	}
	vtlog(VtServerLog, "<font size=-1>%T %s:</font> sent packet %p (%d bytes)<br>\n", z->addr, p, tot);
	packetfree(p);
	return 1;
}

static int
interrupted(void)
{
	char e[ERRMAX];

	rerrstr(e, sizeof e);
	return strstr(e, "interrupted") != nil;
}


static Packet*
_vtrecv(VtConn *z)
{
	uchar buf[10], *b;
	int n, need;
	Packet *p;
	int size, len;

	if(z->state != VtStateConnected) {
		werrstr("session not connected");
		return nil;
	}

	p = z->part;
	/* get enough for head size */
	size = packetsize(p);
	need = z->version[1] - '0';	// 2 or 4
	while(size < need) {
		b = packettrailer(p, need);
		assert(b != nil);
		if(0) fprint(2, "%d read hdr\n", getpid());
		n = read(z->infd, b, need);
		if(0) fprint(2, "%d got %d (%r)\n", getpid(), n);
		if(n==0 || (n<0 && !interrupted()))
			goto Err;
		size += n;
		packettrim(p, 0, size);
	}

	if(packetconsume(p, buf, need) < 0)
		goto Err;
	if(z->version[1] == '2') {
		len = (buf[0] << 8) | buf[1];
		size -= 2;
	} else {
		len = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
		size -= 4;
	}

	while(size < len) {
		n = len - size;
		if(n > MaxFragSize)
			n = MaxFragSize;
		b = packettrailer(p, n);
		if(0) fprint(2, "%d read body %d\n", getpid(), n);
		n = read(z->infd, b, n);
		if(0) fprint(2, "%d got %d (%r)\n", getpid(), n);
		if(n > 0)
			size += n;
		packettrim(p, 0, size);
		if(n==0 || (n<0 && !interrupted()))
			goto Err;
	}
	ventirecvbytes += len;
	ventirecvpackets++;
	p = packetsplit(p, len);
	vtlog(VtServerLog, "<font size=-1>%T %s:</font> read packet %p len %d<br>\n", z->addr, p, len);
	return p;
Err:
	vtlog(VtServerLog, "<font size=-1>%T %s:</font> error reading packet: %r<br>\n", z->addr);
	return nil;
}

/*
 * If you fork off two procs running vtrecvproc and vtsendproc,
 * then vtrecv/vtsend (and thus vtrpc) will never block except on
 * rendevouses, which is nice when it's running in one thread of many.
 */
void
vtrecvproc(void *v)
{
	Packet *p;
	VtConn *z;
	Queue *q;

	z = v;
	q = _vtqalloc();

	qlock(&z->lk);
	z->readq = q;
	qlock(&z->inlk);
	rwakeup(&z->rpcfork);
	qunlock(&z->lk);

	while((p = _vtrecv(z)) != nil)
		if(_vtqsend(q, p) < 0){
			packetfree(p);
			break;
		}
	qunlock(&z->inlk);
	qlock(&z->lk);
	_vtqhangup(q);
	while((p = _vtnbqrecv(q)) != nil)
		packetfree(p);
	_vtqdecref(q);
	z->readq = nil;
	rwakeup(&z->rpcfork);
	qunlock(&z->lk);
	vthangup(z);
}

void
vtsendproc(void *v)
{
	Queue *q;
	Packet *p;
	VtConn *z;

	z = v;
	q = _vtqalloc();

	qlock(&z->lk);
	z->writeq = q;
	qlock(&z->outlk);
	rwakeup(&z->rpcfork);
	qunlock(&z->lk);

	while((p = _vtqrecv(q)) != nil)
		if(_vtsend(z, p) < 0)
			break;
	qunlock(&z->outlk);
	qlock(&z->lk);
	_vtqhangup(q);
	while((p = _vtnbqrecv(q)) != nil)
		packetfree(p);
	_vtqdecref(q);
	z->writeq = nil;
	rwakeup(&z->rpcfork);
	qunlock(&z->lk);
	return;
}

Packet*
vtrecv(VtConn *z)
{
	Packet *p;
	Queue *q;

	qlock(&z->lk);
	if(z->state != VtStateConnected){
		werrstr("not connected");
		qunlock(&z->lk);
		return nil;
	}
	if(z->readq){
		q = _vtqincref(z->readq);
		qunlock(&z->lk);
		p = _vtqrecv(q);
		_vtqdecref(q);
		return p;
	}

	qlock(&z->inlk);
	qunlock(&z->lk);
	p = _vtrecv(z);
	qunlock(&z->inlk);
	if(!p)
		vthangup(z);
	return p;
}

int
vtsend(VtConn *z, Packet *p)
{
	Queue *q;

	qlock(&z->lk);
	if(z->state != VtStateConnected){
		packetfree(p);
		werrstr("not connected");
		qunlock(&z->lk);
		return -1;
	}
	if(z->writeq){
		q = _vtqincref(z->writeq);
		qunlock(&z->lk);
		if(_vtqsend(q, p) < 0){
			_vtqdecref(q);
			packetfree(p);
			return -1;
		}
		_vtqdecref(q);
		return 0;
	}

	qlock(&z->outlk);
	qunlock(&z->lk);
	if(_vtsend(z, p) < 0){
		qunlock(&z->outlk);
		vthangup(z);
		return -1;
	}
	qunlock(&z->outlk);
	return 0;
}
