#include <u.h>
#include <libc.h>
#include <venti.h>

int ventidoublechecksha1 = 1;

static int
vtfcallrpc(VtConn *z, VtFcall *ou, VtFcall *in)
{
	Packet *p;

	p = vtfcallpack(ou);
	if(p == nil)
		return -1;
	if((p = _vtrpc(z, p, ou)) == nil)
		return -1;
	if(vtfcallunpack(in, p) < 0){
		packetfree(p);
		return -1;
	}
	if(chattyventi)
		fprint(2, "%s <- %F\n", argv0, in);
	if(in->msgtype == VtRerror){
		werrstr(in->error);
		vtfcallclear(in);
		packetfree(p);
		return -1;
	}
	if(in->msgtype != ou->msgtype+1){
		werrstr("type mismatch: sent %c%d got %c%d",
			"TR"[ou->msgtype&1], ou->msgtype>>1,
			"TR"[in->msgtype&1], in->msgtype>>1);
		vtfcallclear(in);
		packetfree(p);
		return -1;
	}
	packetfree(p);
	return 0;
}

int
vthello(VtConn *z)
{
	VtFcall tx, rx;

	memset(&tx, 0, sizeof tx);
	tx.msgtype = VtThello;
	tx.version = z->version;
	tx.uid = z->uid;
	if(tx.uid == nil)
		tx.uid = "anonymous";
	if(vtfcallrpc(z, &tx, &rx) < 0)
		return -1;
	z->sid = rx.sid;
	rx.sid = 0;
	vtfcallclear(&rx);
	return 0;
}

Packet*
vtreadpacket(VtConn *z, uchar score[VtScoreSize], uint type, int n)
{
	VtFcall tx, rx;

	if(memcmp(score, vtzeroscore, VtScoreSize) == 0)
		return packetalloc();

	if(z == nil){
		werrstr("not connected");
		return nil;
	}

	if(z->version[1] == '2' && n >= (1<<16)) {
		werrstr("read count too large for protocol");
		return nil;
	}
	memset(&tx, 0, sizeof tx);
	tx.msgtype = VtTread;
	tx.blocktype = type;
	tx.count = n;
	memmove(tx.score, score, VtScoreSize);
	if(vtfcallrpc(z, &tx, &rx) < 0)
		return nil;
	if(packetsize(rx.data) > n){
		werrstr("read returned too much data");
		packetfree(rx.data);
		return nil;
	}
	if(ventidoublechecksha1){
		packetsha1(rx.data, tx.score);
		if(memcmp(score, tx.score, VtScoreSize) != 0){
			werrstr("read asked for %V got %V", score, tx.score);
			packetfree(rx.data);
			return nil;
		}
	}
	return rx.data;
}

int
vtread(VtConn *z, uchar score[VtScoreSize], uint type, uchar *buf, int n)
{
	int nn;
	Packet *p;

	if((p = vtreadpacket(z, score, type, n)) == nil)
		return -1;
	nn = packetsize(p);
	if(packetconsume(p, buf, nn) < 0)
		abort();
	packetfree(p);
	return nn;
}

int
vtwritepacket(VtConn *z, uchar score[VtScoreSize], uint type, Packet *p)
{
	VtFcall tx, rx;

	if(packetsize(p) == 0){
		memmove(score, vtzeroscore, VtScoreSize);
		return 0;
	}
	tx.msgtype = VtTwrite;
	tx.blocktype = type;
	tx.data = p;
	if(ventidoublechecksha1)
		packetsha1(p, score);
	if(vtfcallrpc(z, &tx, &rx) < 0)
		return -1;
	if(ventidoublechecksha1){
		if(memcmp(score, rx.score, VtScoreSize) != 0){
			werrstr("sha1 hash mismatch: want %V got %V", score, rx.score);
			return -1;
		}
	}else
		memmove(score, rx.score, VtScoreSize);
	return 0;
}

int
vtwrite(VtConn *z, uchar score[VtScoreSize], uint type, uchar *buf, int n)
{
	Packet *p;
	int nn;

	p = packetforeign(buf, n, 0, nil);
	nn = vtwritepacket(z, score, type, p);
	packetfree(p);
	return nn;
}

int
vtsync(VtConn *z)
{
	VtFcall tx, rx;

	tx.msgtype = VtTsync;
	return vtfcallrpc(z, &tx, &rx);
}

int
vtping(VtConn *z)
{
	VtFcall tx, rx;

	tx.msgtype = VtTping;
	return vtfcallrpc(z, &tx, &rx);
}

int
vtconnect(VtConn *z)
{
	if(vtversion(z) < 0)
		return -1;
	if(vthello(z) < 0)
		return -1;
	return 0;
}

int
vtgoodbye(VtConn *z)
{
	VtFcall tx, rx;

	tx.msgtype = VtTgoodbye;
	vtfcallrpc(z, &tx, &rx);	/* always fails: no VtRgoodbye */
	return 0;
}
