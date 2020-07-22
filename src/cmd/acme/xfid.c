#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

enum
{
	Ctlsize	= 5*12
};

char	Edel[]		= "deleted window";
char	Ebadctl[]		= "ill-formed control message";
char	Ebadaddr[]	= "bad address syntax";
char	Eaddr[]		= "address out of range";
char	Einuse[]		= "already in use";
char	Ebadevent[]	= "bad event syntax";
extern char Eperm[];

static
void
clampaddr(Window *w)
{
	if(w->addr.q0 < 0)
		w->addr.q0 = 0;
	if(w->addr.q1 < 0)
		w->addr.q1 = 0;
	if(w->addr.q0 > w->body.file->b.nc)
		w->addr.q0 = w->body.file->b.nc;
	if(w->addr.q1 > w->body.file->b.nc)
		w->addr.q1 = w->body.file->b.nc;
}

void
xfidctl(void *arg)
{
	Xfid *x;
	void (*f)(Xfid*);

	threadsetname("xfidctlthread");
	x = arg;
	for(;;){
		f = (void(*)(Xfid*))recvp(x->c);
		(*f)(x);
		flushimage(display, 1);
		sendp(cxfidfree, x);
	}
}

void
xfidflush(Xfid *x)
{
	Fcall fc;
	int i, j;
	Window *w;
	Column *c;
	Xfid *wx;

	xfidlogflush(x);

	/* search windows for matching tag */
	qlock(&row.lk);
	for(j=0; j<row.ncol; j++){
		c = row.col[j];
		for(i=0; i<c->nw; i++){
			w = c->w[i];
			winlock(w, 'E');
			wx = w->eventx;
			if(wx!=nil && wx->fcall.tag==x->fcall.oldtag){
				w->eventx = nil;
				wx->flushed = TRUE;
				sendp(wx->c, nil);
				winunlock(w);
				goto out;
			}
			winunlock(w);
		}
	}
out:
	qunlock(&row.lk);
	respond(x, &fc, nil);
}

void
xfidopen(Xfid *x)
{
	Fcall fc;
	Window *w;
	Text *t;
	char *s;
	Rune *r;
	int m, n, q, q0, q1;

	w = x->f->w;
	t = &w->body;
	q = FILE(x->f->qid);
	if(w){
		winlock(w, 'E');
		switch(q){
		case QWaddr:
			if(w->nopen[q]++ == 0){
				w->addr = range(0, 0);
				w->limit = range(-1,-1);
			}
			break;
		case QWdata:
		case QWxdata:
			w->nopen[q]++;
			break;
		case QWevent:
			if(w->nopen[q]++ == 0){
				if(!w->isdir && w->col!=nil){
					w->filemenu = FALSE;
					winsettag(w);
				}
			}
			break;
		case QWrdsel:
			/*
			 * Use a temporary file.
			 * A pipe would be the obvious, but we can't afford the
			 * broken pipe notification.  Using the code to read QWbody
			 * is n², which should probably also be fixed.  Even then,
			 * though, we'd need to squirrel away the data in case it's
			 * modified during the operation, e.g. by |sort
			 */
			if(w->rdselfd > 0){
				winunlock(w);
				respond(x, &fc, Einuse);
				return;
			}
			w->rdselfd = tempfile();
			if(w->rdselfd < 0){
				winunlock(w);
				respond(x, &fc, "can't create temp file");
				return;
			}
			w->nopen[q]++;
			q0 = t->q0;
			q1 = t->q1;
			r = fbufalloc();
			s = fbufalloc();
			while(q0 < q1){
				n = q1 - q0;
				if(n > BUFSIZE/UTFmax)
					n = BUFSIZE/UTFmax;
				bufread(&t->file->b, q0, r, n);
				m = snprint(s, BUFSIZE+1, "%.*S", n, r);
				if(write(w->rdselfd, s, m) != m){
					warning(nil, "can't write temp file for pipe command %r\n");
					break;
				}
				q0 += n;
			}
			fbuffree(s);
			fbuffree(r);
			break;
		case QWwrsel:
			w->nopen[q]++;
			seq++;
			filemark(t->file);
			cut(t, t, nil, FALSE, TRUE, nil, 0);
			w->wrselrange = range(t->q1, t->q1);
			w->nomark = TRUE;
			break;
		case QWeditout:
			if(editing == FALSE){
				winunlock(w);
				respond(x, &fc, Eperm);
				return;
			}
			if(!canqlock(&w->editoutlk)){
				winunlock(w);
				respond(x, &fc, Einuse);
				return;
			}
			w->wrselrange = range(t->q1, t->q1);
			break;
		}
		winunlock(w);
	}
	else{
		switch(q){
		case Qlog:
			xfidlogopen(x);
			break;
		case Qeditout:
			if(!canqlock(&editoutlk)){
				respond(x, &fc, Einuse);
				return;
			}
			break;
		}
	}
	fc.qid = x->f->qid;
	fc.iounit = messagesize-IOHDRSZ;
	x->f->open = TRUE;
	respond(x, &fc, nil);
}

void
xfidclose(Xfid *x)
{
	Fcall fc;
	Window *w;
	int q;
	Text *t;

	w = x->f->w;
	x->f->busy = FALSE;
	x->f->w = nil;
	if(x->f->open == FALSE){
		if(w != nil)
			winclose(w);
		respond(x, &fc, nil);
		return;
	}

	q = FILE(x->f->qid);
	x->f->open = FALSE;
	if(w){
		winlock(w, 'E');
		switch(q){
		case QWctl:
			if(w->ctlfid!=~0 && w->ctlfid==x->f->fid){
				w->ctlfid = ~0;
				qunlock(&w->ctllock);
			}
			break;
		case QWdata:
		case QWxdata:
			w->nomark = FALSE;
			/* fall through */
		case QWaddr:
		case QWevent:	/* BUG: do we need to shut down Xfid? */
			if(--w->nopen[q] == 0){
				if(q == QWdata || q == QWxdata)
					w->nomark = FALSE;
				if(q==QWevent && !w->isdir && w->col!=nil){
					w->filemenu = TRUE;
					winsettag(w);
				}
				if(q == QWevent){
					free(w->dumpstr);
					free(w->dumpdir);
					w->dumpstr = nil;
					w->dumpdir = nil;
				}
			}
			break;
		case QWrdsel:
			close(w->rdselfd);
			w->rdselfd = 0;
			break;
		case QWwrsel:
			w->nomark = FALSE;
			t = &w->body;
			/* before: only did this if !w->noscroll, but that didn't seem right in practice */
			textshow(t, min(w->wrselrange.q0, t->file->b.nc),
				min(w->wrselrange.q1, t->file->b.nc), 1);
			textscrdraw(t);
			break;
		case QWeditout:
			qunlock(&w->editoutlk);
			break;
		}
		winunlock(w);
		winclose(w);
	}
	else{
		switch(q){
		case Qeditout:
			qunlock(&editoutlk);
			break;
		}
	}
	respond(x, &fc, nil);
}

void
xfidread(Xfid *x)
{
	Fcall fc;
	int n, q;
	uint off;
	char *b;
	char buf[256];
	Window *w;

	q = FILE(x->f->qid);
	w = x->f->w;
	if(w == nil){
		fc.count = 0;
		switch(q){
		case Qcons:
		case Qlabel:
			break;
		case Qindex:
			xfidindexread(x);
			return;
		case Qlog:
			xfidlogread(x);
			return;
		default:
			warning(nil, "unknown qid %d\n", q);
			break;
		}
		respond(x, &fc, nil);
		return;
	}
	winlock(w, 'F');
	if(w->col == nil){
		winunlock(w);
		respond(x, &fc, Edel);
		return;
	}
	off = x->fcall.offset;
	switch(q){
	case QWaddr:
		textcommit(&w->body, TRUE);
		clampaddr(w);
		sprint(buf, "%11d %11d ", w->addr.q0, w->addr.q1);
		goto Readbuf;

	case QWbody:
		xfidutfread(x, &w->body, w->body.file->b.nc, QWbody);
		break;

	case QWctl:
		b = winctlprint(w, buf, 1);
		goto Readb;

	Readbuf:
		b = buf;
	Readb:
		n = strlen(b);
		if(off > n)
			off = n;
		if(off+x->fcall.count > n)
			x->fcall.count = n-off;
		fc.count = x->fcall.count;
		fc.data = b+off;
		respond(x, &fc, nil);
		if(b != buf)
			free(b);
		break;

	case QWevent:
		xfideventread(x, w);
		break;

	case QWdata:
		/* BUG: what should happen if q1 > q0? */
		if(w->addr.q0 > w->body.file->b.nc){
			respond(x, &fc, Eaddr);
			break;
		}
		w->addr.q0 += xfidruneread(x, &w->body, w->addr.q0, w->body.file->b.nc);
		w->addr.q1 = w->addr.q0;
		break;

	case QWxdata:
		/* BUG: what should happen if q1 > q0? */
		if(w->addr.q0 > w->body.file->b.nc){
			respond(x, &fc, Eaddr);
			break;
		}
		w->addr.q0 += xfidruneread(x, &w->body, w->addr.q0, w->addr.q1);
		break;

	case QWtag:
		xfidutfread(x, &w->tag, w->tag.file->b.nc, QWtag);
		break;

	case QWrdsel:
		seek(w->rdselfd, off, 0);
		n = x->fcall.count;
		if(n > BUFSIZE)
			n = BUFSIZE;
		b = fbufalloc();
		n = read(w->rdselfd, b, n);
		if(n < 0){
			respond(x, &fc, "I/O error in temp file");
			break;
		}
		fc.count = n;
		fc.data = b;
		respond(x, &fc, nil);
		fbuffree(b);
		break;

	default:
		sprint(buf, "unknown qid %d in read", q);
		respond(x, &fc, nil);
	}
	winunlock(w);
}

static int
shouldscroll(Text *t, uint q0, int qid)
{
	if(qid == Qcons)
		return TRUE;
	return t->org <= q0 && q0 <= t->org+t->fr.nchars;
}

static Rune*
fullrunewrite(Xfid *x, int *inr)
{
	int q, cnt, c, nb, nr;
	Rune *r;

	q = x->f->nrpart;
	cnt = x->fcall.count;
	if(q > 0){
		memmove(x->fcall.data+q, x->fcall.data, cnt);	/* there's room; see fsysproc */
		memmove(x->fcall.data, x->f->rpart, q);
		cnt += q;
		x->f->nrpart = 0;
	}
	r = runemalloc(cnt);
	cvttorunes(x->fcall.data, cnt-UTFmax, r, &nb, &nr, nil);
	/* approach end of buffer */
	while(fullrune(x->fcall.data+nb, cnt-nb)){
		c = nb;
		nb += chartorune(&r[nr], x->fcall.data+c);
		if(r[nr])
			nr++;
	}
	if(nb < cnt){
		memmove(x->f->rpart, x->fcall.data+nb, cnt-nb);
		x->f->nrpart = cnt-nb;
	}
	*inr = nr;
	return r;
}

void
xfidwrite(Xfid *x)
{
	Fcall fc;
	int c, qid, nb, nr, eval;
	char buf[64], *err;
	Window *w;
	Rune *r;
	Range a;
	Text *t;
	uint q0, tq0, tq1;

	qid = FILE(x->f->qid);
	w = x->f->w;
	if(w){
		c = 'F';
		if(qid==QWtag || qid==QWbody)
			c = 'E';
		winlock(w, c);
		if(w->col == nil){
			winunlock(w);
			respond(x, &fc, Edel);
			return;
		}
	}
	x->fcall.data[x->fcall.count] = 0;
	switch(qid){
	case Qcons:
		w = errorwin(x->f->mntdir, 'X');
		t=&w->body;
		goto BodyTag;

	case Qlabel:
		fc.count = x->fcall.count;
		respond(x, &fc, nil);
		break;

	case QWaddr:
		x->fcall.data[x->fcall.count] = 0;
		r = bytetorune(x->fcall.data, &nr);
		t = &w->body;
		wincommit(w, t);
		eval = TRUE;
		a = address(FALSE, t, w->limit, w->addr, r, 0, nr, rgetc, &eval, (uint*)&nb);
		free(r);
		if(nb < nr){
			respond(x, &fc, Ebadaddr);
			break;
		}
		if(!eval){
			respond(x, &fc, Eaddr);
			break;
		}
		w->addr = a;
		fc.count = x->fcall.count;
		respond(x, &fc, nil);
		break;

	case Qeditout:
	case QWeditout:
		r = fullrunewrite(x, &nr);
		if(w)
			err = edittext(w, w->wrselrange.q1, r, nr);
		else
			err = edittext(nil, 0, r, nr);
		free(r);
		if(err != nil){
			respond(x, &fc, err);
			break;
		}
		fc.count = x->fcall.count;
		respond(x, &fc, nil);
		break;

	case QWerrors:
		w = errorwinforwin(w);
		t = &w->body;
		goto BodyTag;

	case QWbody:
	case QWwrsel:
		t = &w->body;
		goto BodyTag;

	case QWctl:
		xfidctlwrite(x, w);
		break;

	case QWdata:
		a = w->addr;
		t = &w->body;
		wincommit(w, t);
		if(a.q0>t->file->b.nc || a.q1>t->file->b.nc){
			respond(x, &fc, Eaddr);
			break;
		}
		r = runemalloc(x->fcall.count);
		cvttorunes(x->fcall.data, x->fcall.count, r, &nb, &nr, nil);
		if(w->nomark == FALSE){
			seq++;
			filemark(t->file);
		}
		q0 = a.q0;
		if(a.q1 > q0){
			textdelete(t, q0, a.q1, TRUE);
			w->addr.q1 = q0;
		}
		tq0 = t->q0;
		tq1 = t->q1;
		textinsert(t, q0, r, nr, TRUE);
		if(tq0 >= q0)
			tq0 += nr;
		if(tq1 >= q0)
			tq1 += nr;
		textsetselect(t, tq0, tq1);
		if(shouldscroll(t, q0, qid))
			textshow(t, q0+nr, q0+nr, 0);
		textscrdraw(t);
		winsettag(w);
		free(r);
		w->addr.q0 += nr;
		w->addr.q1 = w->addr.q0;
		fc.count = x->fcall.count;
		respond(x, &fc, nil);
		break;

	case QWevent:
		xfideventwrite(x, w);
		break;

	case QWtag:
		t = &w->tag;
		goto BodyTag;

	BodyTag:
		r = fullrunewrite(x, &nr);
		if(nr > 0){
			wincommit(w, t);
			if(qid == QWwrsel){
				q0 = w->wrselrange.q1;
				if(q0 > t->file->b.nc)
					q0 = t->file->b.nc;
			}else
				q0 = t->file->b.nc;
			if(qid == QWtag)
				textinsert(t, q0, r, nr, TRUE);
			else{
				if(w->nomark == FALSE){
					seq++;
					filemark(t->file);
				}
				q0 = textbsinsert(t, q0, r, nr, TRUE, &nr);
				textsetselect(t, t->q0, t->q1);	/* insert could leave it somewhere else */
				if(qid!=QWwrsel && shouldscroll(t, q0, qid))
					textshow(t, q0+nr, q0+nr, 1);
				textscrdraw(t);
			}
			winsettag(w);
			if(qid == QWwrsel)
				w->wrselrange.q1 += nr;
			free(r);
		}
		fc.count = x->fcall.count;
		respond(x, &fc, nil);
		break;

	default:
		sprint(buf, "unknown qid %d in write", qid);
		respond(x, &fc, buf);
		break;
	}
	if(w)
		winunlock(w);
}

void
xfidctlwrite(Xfid *x, Window *w)
{
	Fcall fc;
	int i, m, n, nb, nr, nulls;
	Rune *r;
	char *err, *p, *pp, *q, *e;
	int isfbuf, scrdraw, settag;
	Text *t;

	err = nil;
	e = x->fcall.data+x->fcall.count;
	scrdraw = FALSE;
	settag = FALSE;
	isfbuf = TRUE;
	if(x->fcall.count < RBUFSIZE)
		r = fbufalloc();
	else{
		isfbuf = FALSE;
		r = emalloc(x->fcall.count*UTFmax+1);
	}
	x->fcall.data[x->fcall.count] = 0;
	textcommit(&w->tag, TRUE);
	for(n=0; n<x->fcall.count; n+=m){
		p = x->fcall.data+n;
		if(strncmp(p, "lock", 4) == 0){	/* make window exclusive use */
			qlock(&w->ctllock);
			w->ctlfid = x->f->fid;
			m = 4;
		}else
		if(strncmp(p, "unlock", 6) == 0){	/* release exclusive use */
			w->ctlfid = ~0;
			qunlock(&w->ctllock);
			m = 6;
		}else
		if(strncmp(p, "clean", 5) == 0){	/* mark window 'clean', seq=0 */
			t = &w->body;
			t->eq0 = ~0;
			filereset(t->file);
			t->file->mod = FALSE;
			w->dirty = FALSE;
			settag = TRUE;
			m = 5;
		}else
		if(strncmp(p, "dirty", 5) == 0){	/* mark window 'dirty' */
			t = &w->body;
			/* doesn't change sequence number, so "Put" won't appear.  it shouldn't. */
			t->file->mod = TRUE;
			w->dirty = TRUE;
			settag = TRUE;
			m = 5;
		}else
		if(strncmp(p, "show", 4) == 0){	/* show dot */
			t = &w->body;
			textshow(t, t->q0, t->q1, 1);
			m = 4;
		}else
		if(strncmp(p, "name ", 5) == 0){	/* set file name */
			pp = p+5;
			m = 5;
			q = memchr(pp, '\n', e-pp);
			if(q==nil || q==pp){
				err = Ebadctl;
				break;
			}
			*q = 0;
			nulls = FALSE;
			cvttorunes(pp, q-pp, r, &nb, &nr, &nulls);
			if(nulls){
				err = "nulls in file name";
				break;
			}
			for(i=0; i<nr; i++)
				if(r[i] <= ' '){
					err = "bad character in file name";
					goto out;
				}
out:
			seq++;
			filemark(w->body.file);
			winsetname(w, r, nr);
			m += (q+1) - pp;
		}else
		if(strncmp(p, "font ", 5) == 0){		/* execute font command */
			pp = p+5;
			m = 5;
			q = memchr(pp, '\n', e-pp);
			if(q==nil || q==pp){
				err = Ebadctl;
				break;
			}
			*q = 0;
			nulls = FALSE;
			cvttorunes(pp, q-pp, r, &nb, &nr, &nulls);
			if(nulls){
				err = "nulls in font string";
				break;
			}
			fontx(&w->body, nil, nil, FALSE, XXX, r, nr);
			m += (q+1) - pp;
		}else
		if(strncmp(p, "dump ", 5) == 0){	/* set dump string */
			pp = p+5;
			m = 5;
			q = memchr(pp, '\n', e-pp);
			if(q==nil || q==pp){
				err = Ebadctl;
				break;
			}
			*q = 0;
			nulls = FALSE;
			cvttorunes(pp, q-pp, r, &nb, &nr, &nulls);
			if(nulls){
				err = "nulls in dump string";
				break;
			}
			w->dumpstr = runetobyte(r, nr);
			m += (q+1) - pp;
		}else
		if(strncmp(p, "dumpdir ", 8) == 0){	/* set dump directory */
			pp = p+8;
			m = 8;
			q = memchr(pp, '\n', e-pp);
			if(q==nil || q==pp){
				err = Ebadctl;
				break;
			}
			*q = 0;
			nulls = FALSE;
			cvttorunes(pp, q-pp, r, &nb, &nr, &nulls);
			if(nulls){
				err = "nulls in dump directory string";
				break;
			}
			w->dumpdir = runetobyte(r, nr);
			m += (q+1) - pp;
		}else
		if(strncmp(p, "delete", 6) == 0){	/* delete for sure */
			colclose(w->col, w, TRUE);
			m = 6;
		}else
		if(strncmp(p, "del", 3) == 0){	/* delete, but check dirty */
			if(!winclean(w, TRUE)){
				err = "file dirty";
				break;
			}
			colclose(w->col, w, TRUE);
			m = 3;
		}else
		if(strncmp(p, "get", 3) == 0){	/* get file */
			get(&w->body, nil, nil, FALSE, XXX, nil, 0);
			m = 3;
		}else
		if(strncmp(p, "put", 3) == 0){	/* put file */
			put(&w->body, nil, nil, XXX, XXX, nil, 0);
			m = 3;
		}else
		if(strncmp(p, "dot=addr", 8) == 0){	/* set dot */
			textcommit(&w->body, TRUE);
			clampaddr(w);
			w->body.q0 = w->addr.q0;
			w->body.q1 = w->addr.q1;
			textsetselect(&w->body, w->body.q0, w->body.q1);
			settag = TRUE;
			m = 8;
		}else
		if(strncmp(p, "addr=dot", 8) == 0){	/* set addr */
			w->addr.q0 = w->body.q0;
			w->addr.q1 = w->body.q1;
			m = 8;
		}else
		if(strncmp(p, "limit=addr", 10) == 0){	/* set limit */
			textcommit(&w->body, TRUE);
			clampaddr(w);
			w->limit.q0 = w->addr.q0;
			w->limit.q1 = w->addr.q1;
			m = 10;
		}else
		if(strncmp(p, "nomark", 6) == 0){	/* turn off automatic marking */
			w->nomark = TRUE;
			m = 6;
		}else
		if(strncmp(p, "mark", 4) == 0){	/* mark file */
			seq++;
			filemark(w->body.file);
			settag = TRUE;
			m = 4;
		}else
		if(strncmp(p, "nomenu", 6) == 0){	/* turn off automatic menu */
			w->filemenu = FALSE;
			settag = TRUE;
			m = 6;
		}else
		if(strncmp(p, "menu", 4) == 0){	/* enable automatic menu */
			w->filemenu = TRUE;
			settag = TRUE;
			m = 4;
		}else
		if(strncmp(p, "cleartag", 8) == 0){	/* wipe tag right of bar */
			wincleartag(w);
			settag = TRUE;
			m = 8;
		}else{
			err = Ebadctl;
			break;
		}
		while(p[m] == '\n')
			m++;
	}

	if(isfbuf)
		fbuffree(r);
	else
		free(r);
	if(err)
		n = 0;
	fc.count = n;
	respond(x, &fc, err);
	if(settag)
		winsettag(w);
	if(scrdraw)
		textscrdraw(&w->body);
}

void
xfideventwrite(Xfid *x, Window *w)
{
	Fcall fc;
	int m, n;
	Rune *r;
	char *err, *p, *q;
	int isfbuf;
	Text *t;
	int c;
	uint q0, q1;

	err = nil;
	isfbuf = TRUE;
	if(x->fcall.count < RBUFSIZE)
		r = fbufalloc();
	else{
		isfbuf = FALSE;
		r = emalloc(x->fcall.count*UTFmax+1);
	}
	for(n=0; n<x->fcall.count; n+=m){
		p = x->fcall.data+n;
		w->owner = *p++;	/* disgusting */
		c = *p++;
		while(*p == ' ')
			p++;
		q0 = strtoul(p, &q, 10);
		if(q == p)
			goto Rescue;
		p = q;
		while(*p == ' ')
			p++;
		q1 = strtoul(p, &q, 10);
		if(q == p)
			goto Rescue;
		p = q;
		while(*p == ' ')
			p++;
		if(*p++ != '\n')
			goto Rescue;
		m = p-(x->fcall.data+n);
		if('a'<=c && c<='z')
			t = &w->tag;
		else if('A'<=c && c<='Z')
			t = &w->body;
		else
			goto Rescue;
		if(q0>t->file->b.nc || q1>t->file->b.nc || q0>q1)
			goto Rescue;

		qlock(&row.lk);	/* just like mousethread */
		switch(c){
		case 'x':
		case 'X':
			execute(t, q0, q1, TRUE, nil);
			break;
		case 'l':
		case 'L':
			look3(t, q0, q1, TRUE);
			break;
		default:
			qunlock(&row.lk);
			goto Rescue;
		}
		qunlock(&row.lk);

	}

    Out:
	if(isfbuf)
		fbuffree(r);
	else
		free(r);
	if(err)
		n = 0;
	fc.count = n;
	respond(x, &fc, err);
	return;

    Rescue:
	err = Ebadevent;
	goto Out;
}

void
xfidutfread(Xfid *x, Text *t, uint q1, int qid)
{
	Fcall fc;
	Window *w;
	Rune *r;
	char *b, *b1;
	uint q, off, boff;
	int m, n, nr, nb;

	w = t->w;
	wincommit(w, t);
	off = x->fcall.offset;
	r = fbufalloc();
	b = fbufalloc();
	b1 = fbufalloc();
	n = 0;
	if(qid==w->utflastqid && off>=w->utflastboff && w->utflastq<=q1){
		boff = w->utflastboff;
		q = w->utflastq;
	}else{
		/* BUG: stupid code: scan from beginning */
		boff = 0;
		q = 0;
	}
	w->utflastqid = qid;
	while(q<q1 && n<x->fcall.count){
		/*
		 * Updating here avoids partial rune problem: we're always on a
		 * char boundary. The cost is we will usually do one more read
		 * than we really need, but that's better than being n^2.
		 */
		w->utflastboff = boff;
		w->utflastq = q;
		nr = q1-q;
		if(nr > BUFSIZE/UTFmax)
			nr = BUFSIZE/UTFmax;
		bufread(&t->file->b, q, r, nr);
		nb = snprint(b, BUFSIZE+1, "%.*S", nr, r);
		if(boff >= off){
			m = nb;
			if(boff+m > off+x->fcall.count)
				m = off+x->fcall.count - boff;
			memmove(b1+n, b, m);
			n += m;
		}else if(boff+nb > off){
			if(n != 0)
				error("bad count in utfrune");
			m = nb - (off-boff);
			if(m > x->fcall.count)
				m = x->fcall.count;
			memmove(b1, b+(off-boff), m);
			n += m;
		}
		boff += nb;
		q += nr;
	}
	fbuffree(r);
	fbuffree(b);
	fc.count = n;
	fc.data = b1;
	respond(x, &fc, nil);
	fbuffree(b1);
}

int
xfidruneread(Xfid *x, Text *t, uint q0, uint q1)
{
	Fcall fc;
	Window *w;
	Rune *r, junk;
	char *b, *b1;
	uint q, boff;
	int i, rw, m, n, nr, nb;

	w = t->w;
	wincommit(w, t);
	r = fbufalloc();
	b = fbufalloc();
	b1 = fbufalloc();
	n = 0;
	q = q0;
	boff = 0;
	while(q<q1 && n<x->fcall.count){
		nr = q1-q;
		if(nr > BUFSIZE/UTFmax)
			nr = BUFSIZE/UTFmax;
		bufread(&t->file->b, q, r, nr);
		nb = snprint(b, BUFSIZE+1, "%.*S", nr, r);
		m = nb;
		if(boff+m > x->fcall.count){
			i = x->fcall.count - boff;
			/* copy whole runes only */
			m = 0;
			nr = 0;
			while(m < i){
				rw = chartorune(&junk, b+m);
				if(m+rw > i)
					break;
				m += rw;
				nr++;
			}
			if(m == 0)
				break;
		}
		memmove(b1+n, b, m);
		n += m;
		boff += nb;
		q += nr;
	}
	fbuffree(r);
	fbuffree(b);
	fc.count = n;
	fc.data = b1;
	respond(x, &fc, nil);
	fbuffree(b1);
	return q-q0;
}

void
xfideventread(Xfid *x, Window *w)
{
	Fcall fc;
	int i, n;

	i = 0;
	x->flushed = FALSE;
	while(w->nevents == 0){
		if(i){
			if(!x->flushed)
				respond(x, &fc, "window shut down");
			return;
		}
		w->eventx = x;
		winunlock(w);
		recvp(x->c);
		winlock(w, 'F');
		i++;
	}

	n = w->nevents;
	if(n > x->fcall.count)
		n = x->fcall.count;
	fc.count = n;
	fc.data = w->events;
	respond(x, &fc, nil);
	w->nevents -= n;
	if(w->nevents){
		memmove(w->events, w->events+n, w->nevents);
		w->events = erealloc(w->events, w->nevents);
	}else{
		free(w->events);
		w->events = nil;
	}
}

void
xfidindexread(Xfid *x)
{
	Fcall fc;
	int i, j, m, n, nmax, isbuf, cnt, off;
	Window *w;
	char *b;
	Rune *r;
	Column *c;

	qlock(&row.lk);
	nmax = 0;
	for(j=0; j<row.ncol; j++){
		c = row.col[j];
		for(i=0; i<c->nw; i++){
			w = c->w[i];
			nmax += Ctlsize + w->tag.file->b.nc*UTFmax + 1;
		}
	}
	nmax++;
	isbuf = (nmax<=RBUFSIZE);
	if(isbuf)
		b = (char*)x->buf;
	else
		b = emalloc(nmax);
	r = fbufalloc();
	n = 0;
	for(j=0; j<row.ncol; j++){
		c = row.col[j];
		for(i=0; i<c->nw; i++){
			w = c->w[i];
			/* only show the currently active window of a set */
			if(w->body.file->curtext != &w->body)
				continue;
			winctlprint(w, b+n, 0);
			n += Ctlsize;
			m = min(RBUFSIZE, w->tag.file->b.nc);
			bufread(&w->tag.file->b, 0, r, m);
			m = n + snprint(b+n, nmax-n-1, "%.*S", m, r);
			while(n<m && b[n]!='\n')
				n++;
			b[n++] = '\n';
		}
	}
	qunlock(&row.lk);
	off = x->fcall.offset;
	cnt = x->fcall.count;
	if(off > n)
		off = n;
	if(off+cnt > n)
		cnt = n-off;
	fc.count = cnt;
	memmove(r, b+off, cnt);
	fc.data = (char*)r;
	if(!isbuf)
		free(b);
	respond(x, &fc, nil);
	fbuffree(r);
}
