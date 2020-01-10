/*
 * Simple read-only NFS v3 server.
 * Runs every request in its own thread.
 * Expects client to provide the fsxxx routines in nfs3srv.h.
 */
#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>
#include "nfs3srv.h"

int			insecure = 0;

static SunStatus
authunixunpack(SunRpc *rpc, SunAuthUnix *au)
{
	uchar *p, *ep;
	SunAuthInfo *ai;

	ai = &rpc->cred;
	if(ai->flavor != SunAuthSys)
		return SunAuthTooWeak;
	p = ai->data;
	ep = p+ai->ndata;
	if(sunauthunixunpack(p, ep, &p, au) < 0)
		return SunGarbageArgs;
	if(!insecure){
		if(au->uid == 0)
			au->uid = -1;
		if(au->gid == 0)
			au->gid = -1;
	}

	return SunSuccess;
}

static int
rnull(SunMsg *m)
{
	NfsMount3RNull rx;

	memset(&rx, 0, sizeof rx);
	return sunmsgreply(m, &rx.call);
}

static int
rmnt(SunMsg *m)
{
	Nfs3Handle nh;
	NfsMount3RMnt rx;
	SunAuthUnix au;
	int ok;

	if((ok = authunixunpack(&m->rpc, &au)) != SunSuccess)
		return sunmsgreplyerror(m, ok);

	/* ignore file system path and return the dump tree */

	memset(&rx, 0, sizeof rx);
	rx.nauth = 0;
	rx.status = 0;
	memset(&nh, 0, sizeof nh);
	fsgetroot(&nh);
	rx.handle = nh.h;
	rx.len = nh.len;

	return sunmsgreply(m, &rx.call);
}

static int
rumnt(SunMsg *m)
{
	NfsMount3RUmnt rx;

	/* ignore */

	memset(&rx, 0, sizeof rx);
	return sunmsgreply(m, &rx.call);
}

static int
rumntall(SunMsg *m)
{
	NfsMount3RUmntall rx;

	/* ignore */

	memset(&rx, 0, sizeof rx);
	return sunmsgreply(m, &rx.call);
}

static int
rexport(SunMsg *m)
{
	NfsMount3RExport rx;

	/* ignore */

	memset(&rx, 0, sizeof rx);
	rx.count = 0;
	return sunmsgreply(m, &rx.call);
}

static void
rmount3(void *v)
{
	SunMsg *m;

	m = v;
	switch(m->call->type){
	default:
		sunmsgreplyerror(m, SunProcUnavail);
	case NfsMount3CallTNull:
		rnull(m);
		break;
	case NfsMount3CallTMnt:
		rmnt(m);
		break;
	case NfsMount3CallTDump:
		rmnt(m);
		break;
	case NfsMount3CallTUmnt:
		rumnt(m);
		break;
	case NfsMount3CallTUmntall:
		rumntall(m);
		break;
	case NfsMount3CallTExport:
		rexport(m);
		break;
	}
}

void
mount3proc(void *v)
{
	Channel *c;
	SunMsg *m;

	threadsetname("mount1");
	c = v;
	while((m=recvp(c)) != nil)
		threadcreate(rmount3, m, SunStackSize);
}

static int
senderror(SunMsg *m, SunCall *rc, Nfs3Status status)
{
	/* knows that status is first field in all replies */
	((Nfs3RGetattr*)rc)->status = status;
	return sunmsgreply(m, rc);
}

static int
rnull0(SunMsg *m)
{
	Nfs3RNull rx;

	memset(&rx, 0, sizeof rx);
	return sunmsgreply(m, &rx.call);
}

static int
rgetattr(SunMsg *m)
{
	Nfs3TGetattr *tx = (Nfs3TGetattr*)m->call;
	Nfs3RGetattr rx;
	SunAuthUnix au;
	int ok;

	if((ok = authunixunpack(&m->rpc, &au)) != SunSuccess)
		return sunmsgreplyerror(m, ok);

	memset(&rx, 0, sizeof rx);
	rx.status = fsgetattr(&au, &tx->handle, &rx.attr);
	return sunmsgreply(m, &rx.call);
}

static int
rlookup(SunMsg *m)
{
	Nfs3TLookup *tx = (Nfs3TLookup*)m->call;
	Nfs3RLookup rx;
	SunAuthUnix au;
	int ok;

	if((ok = authunixunpack(&m->rpc, &au)) != SunSuccess)
		return sunmsgreplyerror(m, ok);

	memset(&rx, 0, sizeof rx);
	rx.status = fsgetattr(&au, &tx->handle, &rx.dirAttr);
	if(rx.status != Nfs3Ok)
		return sunmsgreply(m, &rx.call);
	rx.haveDirAttr = 1;
	rx.status = fslookup(&au, &tx->handle, tx->name, &rx.handle);
	if(rx.status != Nfs3Ok)
		return sunmsgreply(m, &rx.call);
	rx.status = fsgetattr(&au, &rx.handle, &rx.attr);
	if(rx.status != Nfs3Ok)
		return sunmsgreply(m, &rx.call);
	rx.haveAttr = 1;
	return sunmsgreply(m, &rx.call);
}

static int
raccess(SunMsg *m)
{
	Nfs3TAccess *tx = (Nfs3TAccess*)m->call;
	Nfs3RAccess rx;
	SunAuthUnix au;
	int ok;

	if((ok = authunixunpack(&m->rpc, &au)) != SunSuccess)
		return sunmsgreplyerror(m, ok);

	memset(&rx, 0, sizeof rx);
	rx.haveAttr = 1;
	rx.status = fsaccess(&au, &tx->handle, tx->access, &rx.access, &rx.attr);
	return sunmsgreply(m, &rx.call);
}

static int
rreadlink(SunMsg *m)
{
	Nfs3RReadlink rx;
	Nfs3TReadlink *tx = (Nfs3TReadlink*)m->call;
	SunAuthUnix au;
	int ok;

	if((ok = authunixunpack(&m->rpc, &au)) != SunSuccess)
		return sunmsgreplyerror(m, ok);

	memset(&rx, 0, sizeof rx);
	rx.haveAttr = 0;
	rx.data = nil;
	rx.status = fsreadlink(&au, &tx->handle, &rx.data);
	sunmsgreply(m, &rx.call);
	free(rx.data);
	return 0;
}

static int
rread(SunMsg *m)
{
	Nfs3TRead *tx = (Nfs3TRead*)m->call;
	Nfs3RRead rx;
	SunAuthUnix au;
	int ok;

	if((ok = authunixunpack(&m->rpc, &au)) != SunSuccess)
		return sunmsgreplyerror(m, ok);

	memset(&rx, 0, sizeof rx);
	rx.haveAttr = 0;
	rx.data = nil;
	rx.status = fsreadfile(&au, &tx->handle, tx->count, tx->offset, &rx.data, &rx.count, &rx.eof);
	if(rx.status == Nfs3Ok)
		rx.ndata = rx.count;

	sunmsgreply(m, &rx.call);
	free(rx.data);
	return 0;
}

static int
rreaddir(SunMsg *m)
{
	Nfs3TReadDir *tx = (Nfs3TReadDir*)m->call;
	Nfs3RReadDir rx;
	SunAuthUnix au;
	int ok;

	if((ok = authunixunpack(&m->rpc, &au)) != SunSuccess)
		return sunmsgreplyerror(m, ok);

	memset(&rx, 0, sizeof rx);
	rx.status = fsreaddir(&au, &tx->handle, tx->count, tx->cookie, &rx.data, &rx.count, &rx.eof);
	sunmsgreply(m, &rx.call);
	free(rx.data);
	return 0;
}

static int
rreaddirplus(SunMsg *m)
{
	Nfs3RReadDirPlus rx;

	memset(&rx, 0, sizeof rx);
	rx.status = Nfs3ErrNotSupp;
	sunmsgreply(m, &rx.call);
	return 0;
}

static int
rfsstat(SunMsg *m)
{
	Nfs3RFsStat rx;

	/* just make something up */
	memset(&rx, 0, sizeof rx);
	rx.status = Nfs3Ok;
	rx.haveAttr = 0;
	rx.totalBytes = 1000000000;
	rx.freeBytes = 0;
	rx.availBytes = 0;
	rx.totalFiles = 100000;
	rx.freeFiles = 0;
	rx.availFiles = 0;
	rx.invarSec = 0;
	return sunmsgreply(m, &rx.call);
}

static int
rfsinfo(SunMsg *m)
{
	Nfs3RFsInfo rx;

	/* just make something up */
	memset(&rx, 0, sizeof rx);
	rx.status = Nfs3Ok;
	rx.haveAttr = 0;
	rx.readMax = MaxDataSize;
	rx.readPref = MaxDataSize;
	rx.readMult = MaxDataSize;
	rx.writeMax = MaxDataSize;
	rx.writePref = MaxDataSize;
	rx.writeMult = MaxDataSize;
	rx.readDirPref = MaxDataSize;
	rx.maxFileSize = 1LL<<60;
	rx.timePrec.sec = 1;
	rx.timePrec.nsec = 0;
	rx.flags = Nfs3FsHomogeneous|Nfs3FsCanSetTime;
	return sunmsgreply(m, &rx.call);
}

static int
rpathconf(SunMsg *m)
{
	Nfs3RPathconf rx;

	memset(&rx, 0, sizeof rx);
	rx.status = Nfs3Ok;
	rx.haveAttr = 0;
	rx.maxLink = 1;
	rx.maxName = 1024;
	rx.noTrunc = 1;
	rx.chownRestricted = 0;
	rx.caseInsensitive = 0;
	rx.casePreserving = 1;
	return sunmsgreply(m, &rx.call);
}

static int
rrofs(SunMsg *m)
{
	uchar buf[512];	/* clumsy hack*/

	memset(buf, 0, sizeof buf);
	return senderror(m, (SunCall*)buf, Nfs3ErrRoFs);
}


static void
rnfs3(void *v)
{
	SunMsg *m;

	m = v;
	switch(m->call->type){
	default:
		abort();
	case Nfs3CallTNull:
		rnull0(m);
		break;
	case Nfs3CallTGetattr:
		rgetattr(m);
		break;
	case Nfs3CallTLookup:
		rlookup(m);
		break;
	case Nfs3CallTAccess:
		raccess(m);
		break;
	case Nfs3CallTReadlink:
		rreadlink(m);
		break;
	case Nfs3CallTRead:
		rread(m);
		break;
	case Nfs3CallTReadDir:
		rreaddir(m);
		break;
	case Nfs3CallTReadDirPlus:
		rreaddirplus(m);
		break;
	case Nfs3CallTFsStat:
		rfsstat(m);
		break;
	case Nfs3CallTFsInfo:
		rfsinfo(m);
		break;
	case Nfs3CallTPathconf:
		rpathconf(m);
		break;
	case Nfs3CallTSetattr:
	case Nfs3CallTWrite:
	case Nfs3CallTCreate:
	case Nfs3CallTMkdir:
	case Nfs3CallTSymlink:
	case Nfs3CallTMknod:
	case Nfs3CallTRemove:
	case Nfs3CallTRmdir:
	case Nfs3CallTLink:
	case Nfs3CallTCommit:
		rrofs(m);
		break;
	}
}

void
nfs3proc(void *v)
{
	Channel *c;
	SunMsg *m;

	c = v;
	threadsetname("nfs3");
	while((m = recvp(c)) != nil)
		threadcreate(rnfs3, m, SunStackSize);
}
