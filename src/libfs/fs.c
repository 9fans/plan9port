/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "fsimpl.h"

static int _fssend(Mux*, void*);
static void *_fsrecv(Mux*);
static int _fsgettag(Mux*, void*);
static int _fssettag(Mux*, void*, uint);

enum
{
	Fidchunk = 32
};

Fsys*
fsinit(int fd)
{
	Fsys *fs;

	fmtinstall('F', fcallfmt);
	fmtinstall('D', dirfmt);
	fmtinstall('M', dirmodefmt);

	fs = mallocz(sizeof(Fsys), 1);
	if(fs == nil)
		return nil;
	fs->fd = fd;
	fs->ref = 1;
	fs->mux.aux = fs;
	fs->mux.mintag = 0;
	fs->mux.maxtag = 256;
	fs->mux.send = _fssend;
	fs->mux.recv = _fsrecv;
	fs->mux.gettag = _fsgettag;
	fs->mux.settag = _fssettag;
	muxinit(&fs->mux);
	return fs;
}

Fid*
fsroot(Fsys *fs)
{
	/* N.B. no incref */
	return fs->root;
}

Fsys*
fsmount(int fd)
{
	int n;
	char *user;
	Fsys *fs;

	fs = fsinit(fd);
	if(fs == nil)
		return nil;
	strcpy(fs->version, "9P2000");
	if((n = fsversion(fs, 8192, fs->version, sizeof fs->version)) < 0){
	Error:
		fsunmount(fs);
		return nil;
	}
	fs->msize = n;

	user = getuser();
	if((fs->root = fsattach(fs, nil, getuser(), "")) == nil)
		goto Error;
	return fs;
}

void
fsunmount(Fsys *fs)
{
	_fsdecref(fs);
}

void
_fsdecref(Fsys *fs)
{
	Fid *f, *next;

	qlock(&fs->lk);
	if(--fs->ref == 0){
		close(fs->fd);
		for(f=fs->freefid; f; f=next){
			next = f->next;
			if(f->fid%Fidchunk == 0)
				free(f);
		}
		free(fs);
	}
	qunlock(&fs->lk);
}

int
fsversion(Fsys *fs, int msize, char *version, int nversion)
{
	void *freep;
	Fcall tx, rx;

	tx.type = Tversion;
	tx.version = version;
	tx.msize = msize;

	if(fsrpc(fs, &tx, &rx, &freep) < 0)
		return -1;
	strecpy(version, version+nversion, rx.version);
	free(freep);
	return rx.msize;
}

Fid*
fsattach(Fsys *fs, Fid *afid, char *user, char *aname)
{
	Fcall tx, rx;
	Fid *fid;

	if((fid = _fsgetfid(fs)) == nil)
		return nil;

	tx.type = Tattach;
	tx.afid = afid ? afid->fid : NOFID;
	tx.fid = fid->fid;
	tx.uname = user;
	tx.aname = aname;

	if(fsrpc(fs, &tx, &rx, 0) < 0){
		_fsputfid(fid);
		return nil;
	}
	fid->qid = rx.qid;
	return fid;
}

int
fsrpc(Fsys *fs, Fcall *tx, Fcall *rx, void **freep)
{
	int n, nn;
	void *tpkt, *rpkt;

	n = sizeS2M(tx);
	tpkt = malloc(n);
fprint(2, "tpkt %p\n", tpkt);
	if(freep)
		*freep = nil;
	if(tpkt == nil)
		return -1;
	fprint(2, "<- %F\n", tx);
	nn = convS2M(tx, tpkt, n);
	if(nn != n){
		free(tpkt);
		werrstr("libfs: sizeS2M convS2M mismatch");
		fprint(2, "%r\n");
		return -1;
	}
	rpkt = muxrpc(&fs->mux, tpkt);
fprint(2, "tpkt %p\n", tpkt);
	free(tpkt);
fprint(2, "tpkt freed\n");
	if(rpkt == nil)
		return -1;
	n = GBIT32((uchar*)rpkt);
	nn = convM2S(rpkt, n, rx);
	if(nn != n){
		free(rpkt);
		werrstr("libfs: convM2S packet size mismatch");
		fprint(2, "%r\n");
		return -1;
	}
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

Fid*
_fsgetfid(Fsys *fs)
{
	int i;
	Fid *f;

	qlock(&fs->lk);
	if(fs->freefid == nil){
		f = malloc(sizeof(Fid)*Fidchunk);
		if(f == nil){
			qunlock(&fs->lk);
			return nil;
		}
		for(i=0; i<Fidchunk; i++){
			f[i].fid = fs->nextfid++;
			f[i].next = &f[i+1];
			f[i].fs = fs;
			fs->ref++;
		}
		f[i-1].next = nil;
		fs->freefid = f;
	}
	f = fs->freefid;
	fs->freefid = f->next;
	qunlock(&fs->lk);
	return f;
}

void
_fsputfid(Fid *f)
{
	Fsys *fs;

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
	Fsys *fs;

	fs = mux->aux;
	return write(fs->fd, pkt, GBIT32((uchar*)pkt));
}

static void*
_fsrecv(Mux *mux)
{
	uchar *pkt;
	uchar buf[4];
	int n;
	Fsys *fs;

	fs = mux->aux;
	n = readn(fs->fd, buf, 4);
	if(n != 4)
		return nil;
	n = GBIT32(buf);
	pkt = malloc(n+4);
	if(pkt == nil){
		fprint(2, "libfs out of memory reading 9p packet; here comes trouble\n");
		return nil;
	}
	PBIT32(pkt, n);
	if(readn(fs->fd, pkt+4, n-4) != n-4){
		free(pkt);
		return nil;
	}
#if 0
	if(pkt[4] == Ropenfd){
		/* do unix socket crap */
		sysfatal("no socket crap implemented");
	}
#endif
	return pkt;
}
