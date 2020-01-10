/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include <thread.h>
#include "fsimpl.h"

static int _fssend(Mux*, void*);
static void *_fsrecv(Mux*);
static int _fsgettag(Mux*, void*);
static int _fssettag(Mux*, void*, uint);

int chatty9pclient;
int eofkill9pclient;

enum
{
	CFidchunk = 32
};

CFsys*
fsinit(int fd)
{
	CFsys *fs;
	int n;

	fmtinstall('F', fcallfmt);
	fmtinstall('D', dirfmt);
	fmtinstall('M', dirmodefmt);

	fs = mallocz(sizeof(CFsys), 1);
	if(fs == nil){
		werrstr("mallocz: %r");
		return nil;
	}
	fs->fd = fd;
	fs->ref = 1;
	fs->mux.aux = fs;
	fs->mux.mintag = 0;
	fs->mux.maxtag = 256;
	fs->mux.send = _fssend;
	fs->mux.recv = _fsrecv;
	fs->mux.gettag = _fsgettag;
	fs->mux.settag = _fssettag;
	fs->iorecv = ioproc();
	fs->iosend = ioproc();
	muxinit(&fs->mux);

	strcpy(fs->version, "9P2000");
	if((n = fsversion(fs, 8192, fs->version, sizeof fs->version)) < 0){
		werrstr("fsversion: %r");
		_fsunmount(fs);
		return nil;
	}
	fs->msize = n;
	return fs;
}

CFid*
fsroot(CFsys *fs)
{
	/* N.B. no incref */
	return fs->root;
}

CFsys*
fsmount(int fd, char *aname)
{
	CFsys *fs;
	CFid *fid;

	fs = fsinit(fd);
	if(fs == nil)
		return nil;

	if((fid = fsattach(fs, nil, getuser(), aname)) == nil){
		_fsunmount(fs);
		return nil;
	}
	fssetroot(fs, fid);
	return fs;
}

void
_fsunmount(CFsys *fs)
{
	fs->fd = -1;
	fsunmount(fs);
}

void
fsunmount(CFsys *fs)
{
	fsclose(fs->root);
	fs->root = nil;
	_fsdecref(fs);
}

void
_fsdecref(CFsys *fs)
{
	CFid *f, **l, *next;

	qlock(&fs->lk);
	--fs->ref;
	/*fprint(2, "fsdecref %p to %d\n", fs, fs->ref); */
	if(fs->ref == 0){
		if(fs->fd >= 0)
			close(fs->fd);
		/* trim the list down to just the first in each chunk */
		for(l=&fs->freefid; *l; ){
			if((*l)->fid%CFidchunk == 0)
				l = &(*l)->next;
			else
				*l = (*l)->next;
		}
		/* now free the list */
		for(f=fs->freefid; f; f=next){
			next = f->next;
			free(f);
		}
		closeioproc(fs->iorecv);
		closeioproc(fs->iosend);
		free(fs);
		return;
	}
	qunlock(&fs->lk);
}

int
fsversion(CFsys *fs, int msize, char *version, int nversion)
{
	void *freep;
	int r, oldmintag, oldmaxtag;
	Fcall tx, rx;

	tx.tag = 0;
	tx.type = Tversion;
	tx.version = version;
	tx.msize = msize;

	/*
	 * bit of a clumsy hack -- force libmux to use NOTAG as tag.
	 * version can only be sent when there are no other messages
	 * outstanding on the wire, so this is more reasonable than it looks.
	 */
	oldmintag = fs->mux.mintag;
	oldmaxtag = fs->mux.maxtag;
	fs->mux.mintag = NOTAG;
	fs->mux.maxtag = NOTAG+1;
	r = _fsrpc(fs, &tx, &rx, &freep);
	fs->mux.mintag = oldmintag;
	fs->mux.maxtag = oldmaxtag;
	if(r < 0){
		werrstr("fsrpc: %r");
		return -1;
	}

	strecpy(version, version+nversion, rx.version);
	free(freep);
	fs->msize = rx.msize;
	return rx.msize;
}

CFid*
fsattach(CFsys *fs, CFid *afid, char *user, char *aname)
{
	Fcall tx, rx;
	CFid *fid;

	if(aname == nil)
		aname = "";

	if((fid = _fsgetfid(fs)) == nil)
		return nil;

	tx.tag = 0;
	tx.type = Tattach;
	tx.afid = afid ? afid->fid : NOFID;
	tx.fid = fid->fid;
	tx.uname = user;
	tx.aname = aname;

	if(_fsrpc(fs, &tx, &rx, 0) < 0){
		_fsputfid(fid);
		return nil;
	}
	fid->qid = rx.qid;
	return fid;
}

void
fssetroot(CFsys *fs, CFid *fid)
{
	if(fs->root)
		_fsputfid(fs->root);
	fs->root = fid;
}

int
_fsrpc(CFsys *fs, Fcall *tx, Fcall *rx, void **freep)
{
	int n, nn;
	void *tpkt, *rpkt;

	n = sizeS2M(tx);
	tpkt = malloc(n);
	if(freep)
		*freep = nil;
	if(tpkt == nil)
		return -1;
	tx->tag = 0;
	if(chatty9pclient)
		fprint(2, "<- %F\n", tx);
	nn = convS2M(tx, tpkt, n);
	if(nn != n){
		free(tpkt);
		werrstr("lib9pclient: sizeS2M convS2M mismatch");
		fprint(2, "%r\n");
		return -1;
	}
	rpkt = muxrpc(&fs->mux, tpkt);
	free(tpkt);
	if(rpkt == nil){
		werrstr("muxrpc: %r");
		return -1;
	}
	n = GBIT32((uchar*)rpkt);
	nn = convM2S(rpkt, n, rx);
	if(nn != n){
		free(rpkt);
		werrstr("lib9pclient: convM2S packet size mismatch %d %d", n, nn);
		fprint(2, "%r\n");
		return -1;
	}
	if(chatty9pclient)
		fprint(2, "-> %F\n", rx);
	if(rx->type == Rerror){
		werrstr("%s", rx->ename);
		free(rpkt);
		return -1;
	}
	if(rx->type != tx->type+1){
		werrstr("packet type mismatch -- tx %d rx %d",
			tx->type, rx->type);
		free(rpkt);
		return -1;
	}
	if(freep)
		*freep = rpkt;
	else
		free(rpkt);
	return 0;
}

CFid*
_fsgetfid(CFsys *fs)
{
	int i;
	CFid *f;

	qlock(&fs->lk);
	if(fs->freefid == nil){
		f = mallocz(sizeof(CFid)*CFidchunk, 1);
		if(f == nil){
			qunlock(&fs->lk);
			return nil;
		}
		for(i=0; i<CFidchunk; i++){
			f[i].fid = fs->nextfid++;
			f[i].next = &f[i+1];
			f[i].fs = fs;
		}
		f[i-1].next = nil;
		fs->freefid = f;
	}
	f = fs->freefid;
	fs->freefid = f->next;
	fs->ref++;
	qunlock(&fs->lk);
	f->offset = 0;
	f->mode = -1;
	f->qid.path = 0;
	f->qid.vers = 0;
	f->qid.type = 0;
	return f;
}

void
_fsputfid(CFid *f)
{
	CFsys *fs;

	fs = f->fs;
	qlock(&fs->lk);
	f->next = fs->freefid;
	fs->freefid = f;
	qunlock(&fs->lk);
	_fsdecref(fs);
}

static int
_fsgettag(Mux *mux, void *pkt)
{
	return GBIT16((uchar*)pkt+5);
}

static int
_fssettag(Mux *mux, void *pkt, uint tag)
{
	PBIT16((uchar*)pkt+5, tag);
	return 0;
}

static int
_fssend(Mux *mux, void *pkt)
{
	CFsys *fs;
	int n;

	fs = mux->aux;
	n = iowrite(fs->iosend, fs->fd, pkt, GBIT32((uchar*)pkt));
	if(n < 0 && eofkill9pclient)
		threadexitsall(nil);
	return n;
}

static void*
_fsrecv(Mux *mux)
{
	uchar *pkt;
	uchar buf[4];
	int n, nfd;
	CFsys *fs;

	fs = mux->aux;
	n = ioreadn(fs->iorecv, fs->fd, buf, 4);
	if(n != 4){
		if(eofkill9pclient)
			threadexitsall(nil);
		return nil;
	}
	n = GBIT32(buf);
	pkt = malloc(n+4);
	if(pkt == nil){
		fprint(2, "lib9pclient out of memory reading 9p packet; here comes trouble\n");
		return nil;
	}
	PBIT32(pkt, n);
	if(ioreadn(fs->iorecv, fs->fd, pkt+4, n-4) != n-4){
		free(pkt);
		return nil;
	}
	if(pkt[4] == Ropenfd){
		if((nfd=iorecvfd(fs->iorecv, fs->fd)) < 0){
			fprint(2, "recv fd error: %r\n");
			free(pkt);
			return nil;
		}
		PBIT32(pkt+n-4, nfd);
	}
	return pkt;
}

Qid
fsqid(CFid *fid)
{
	return fid->qid;
}
