#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <regexp.h>
#include <9pclient.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

CFid *plumbsendfid;
CFid *plumbeditfid;

Window*	openfile(Text*, Expand*);

int	nuntitled;

void
plumbthread(void *v)
{
	CFid *fid;
	Plumbmsg *m;
	Timer *t;

	USED(v);
	threadsetname("plumbproc");
	
	/*
	 * Loop so that if plumber is restarted, acme need not be.
	 */
	for(;;){
		/*
		 * Connect to plumber.
		 */
		plumbunmount();
		while((fid = plumbopenfid("edit", OREAD|OCEXEC)) == nil){
			t = timerstart(2000);
			recv(t->c, nil);
			timerstop(t);
		}
		plumbeditfid = fid;
		plumbsendfid = plumbopenfid("send", OWRITE|OCEXEC);
	
		/*
		 * Relay messages.
		 */
		for(;;){
			m = plumbrecvfid(plumbeditfid);
			if(m == nil)
				break;
			sendp(cplumb, m);
		}

		/*
		 * Lost connection.
		 */
		fid = plumbsendfid;
		plumbsendfid = nil;
		fsclose(fid);

		fid = plumbeditfid;
		plumbeditfid = nil;
		fsclose(fid);
	}
}

void
startplumbing(void)
{
	cplumb = chancreate(sizeof(Plumbmsg*), 0);
	chansetname(cplumb, "cplumb");
	threadcreate(plumbthread, nil, STACK);
}


void
look3(Text *t, uint q0, uint q1, int external)
{
	int n, c, f, expanded;
	Text *ct;
	Expand e;
	Rune *r;
	uint p;
	Plumbmsg *m;
	Runestr dir;
	char buf[32];

	ct = seltext;
	if(ct == nil)
		seltext = t;
	expanded = expand(t, q0, q1, &e);
	if(!external && t->w!=nil && t->w->nopen[QWevent]>0){
		/* send alphanumeric expansion to external client */
		if(expanded == FALSE)
			return;
		f = 0;
		if((e.u.at!=nil && t->w!=nil) || (e.nname>0 && lookfile(e.name, e.nname)!=nil))
			f = 1;		/* acme can do it without loading a file */
		if(q0!=e.q0 || q1!=e.q1)
			f |= 2;	/* second (post-expand) message follows */
		if(e.nname)
			f |= 4;	/* it's a file name */
		c = 'l';
		if(t->what == Body)
			c = 'L';
		n = q1-q0;
		if(n <= EVENTSIZE){
			r = runemalloc(n);
			bufread(&t->file->b, q0, r, n);
			winevent(t->w, "%c%d %d %d %d %.*S\n", c, q0, q1, f, n, n, r);
			free(r);
		}else
			winevent(t->w, "%c%d %d %d 0 \n", c, q0, q1, f, n);
		if(q0==e.q0 && q1==e.q1)
			return;
		if(e.nname){
			n = e.nname;
			if(e.a1 > e.a0)
				n += 1+(e.a1-e.a0);
			r = runemalloc(n);
			runemove(r, e.name, e.nname);
			if(e.a1 > e.a0){
				r[e.nname] = ':';
				bufread(&e.u.at->file->b, e.a0, r+e.nname+1, e.a1-e.a0);
			}
		}else{
			n = e.q1 - e.q0;
			r = runemalloc(n);
			bufread(&t->file->b, e.q0, r, n);
		}
		f &= ~2;
		if(n <= EVENTSIZE)
			winevent(t->w, "%c%d %d %d %d %.*S\n", c, e.q0, e.q1, f, n, n, r);
		else
			winevent(t->w, "%c%d %d %d 0 \n", c, e.q0, e.q1, f, n);
		free(r);
		goto Return;
	}
	if(plumbsendfid != nil){
		/* send whitespace-delimited word to plumber */
		m = emalloc(sizeof(Plumbmsg));
		m->src = estrdup("acme");
		m->dst = nil;
		dir = dirname(t, nil, 0);
		if(dir.nr==1 && dir.r[0]=='.'){	/* sigh */
			free(dir.r);
			dir.r = nil;
			dir.nr = 0;
		}
		if(dir.nr == 0)
			m->wdir = estrdup(wdir);
		else
			m->wdir = runetobyte(dir.r, dir.nr);
		free(dir.r);
		m->type = estrdup("text");
		m->attr = nil;
		buf[0] = '\0';
		if(q1 == q0){
			if(t->q1>t->q0 && t->q0<=q0 && q0<=t->q1){
				q0 = t->q0;
				q1 = t->q1;
			}else{
				p = q0;
				while(q0>0 && (c=tgetc(t, q0-1))!=' ' && c!='\t' && c!='\n')
					q0--;
				while(q1<t->file->b.nc && (c=tgetc(t, q1))!=' ' && c!='\t' && c!='\n')
					q1++;
				if(q1 == q0){
					plumbfree(m);
					goto Return;
				}
				sprint(buf, "click=%d", p-q0);
				m->attr = plumbunpackattr(buf);
			}
		}
		r = runemalloc(q1-q0);
		bufread(&t->file->b, q0, r, q1-q0);
		m->data = runetobyte(r, q1-q0);
		m->ndata = strlen(m->data);
		free(r);
		if(m->ndata<messagesize-1024 && plumbsendtofid(plumbsendfid, m) >= 0){
			plumbfree(m);
			goto Return;
		}
		plumbfree(m);
		/* plumber failed to match; fall through */
	}

	/* interpret alphanumeric string ourselves */
	if(expanded == FALSE)
		return;
	if(e.name || e.u.at)
		openfile(t, &e);
	else{
		if(t->w == nil)
			return;
		ct = &t->w->body;
		if(t->w != ct->w)
			winlock(ct->w, 'M');
		if(t == ct)
			textsetselect(ct, e.q1, e.q1);
		n = e.q1 - e.q0;
		r = runemalloc(n);
		bufread(&t->file->b, e.q0, r, n);
		if(search(ct, r, n) && e.jump)
			moveto(mousectl, addpt(frptofchar(&ct->fr, ct->fr.p0), Pt(4, ct->fr.font->height-4)));
		if(t->w != ct->w)
			winunlock(ct->w);
		free(r);
	}

   Return:
	free(e.name);
	free(e.bname);
}

int
plumbgetc(void *a, uint n)
{
	Rune *r;

	r = a;
	if(n>runestrlen(r))
		return 0;
	return r[n];
}

void
plumblook(Plumbmsg *m)
{
	Expand e;
	char *addr;

	if(m->ndata >= BUFSIZE){
		warning(nil, "insanely long file name (%d bytes) in plumb message (%.32s...)\n", m->ndata, m->data);
		return;
	}
	e.q0 = 0;
	e.q1 = 0;
	if(m->data[0] == '\0')
		return;
	e.u.ar = nil;
	e.bname = m->data;
	e.name = bytetorune(e.bname, &e.nname);
	e.jump = TRUE;
	e.a0 = 0;
	e.a1 = 0;
	addr = plumblookup(m->attr, "addr");
	if(addr != nil){
		e.u.ar = bytetorune(addr, &e.a1);
		e.agetc = plumbgetc;
	}
	drawtopwindow();
	openfile(nil, &e);
	free(e.name);
	free(e.u.at);
}

void
plumbshow(Plumbmsg *m)
{
	Window *w;
	Rune rb[256], *r;
	int nb, nr;
	Runestr rs;
	char *name, *p, namebuf[16];

	drawtopwindow();
	w = makenewwindow(nil);
	name = plumblookup(m->attr, "filename");
	if(name == nil){
		name = namebuf;
		nuntitled++;
		snprint(namebuf, sizeof namebuf, "Untitled-%d", nuntitled);
	}
	p = nil;
	if(name[0]!='/' && m->wdir!=nil && m->wdir[0]!='\0'){
		nb = strlen(m->wdir) + 1 + strlen(name) + 1;
		p = emalloc(nb);
		snprint(p, nb, "%s/%s", m->wdir, name);
		name = p;
	}
	cvttorunes(name, strlen(name), rb, &nb, &nr, nil);
	free(p);
	rs = cleanrname(runestr(rb, nr));
	winsetname(w, rs.r, rs.nr);
	r = runemalloc(m->ndata);
	cvttorunes(m->data, m->ndata, r, &nb, &nr, nil);
	textinsert(&w->body, 0, r, nr, TRUE);
	free(r);
	w->body.file->mod = FALSE;
	w->dirty = FALSE;
	winsettag(w);
	textscrdraw(&w->body);
	textsetselect(&w->tag, w->tag.file->b.nc, w->tag.file->b.nc);
	xfidlog(w, "new");
}

int
search(Text *ct, Rune *r, uint n)
{
	uint q, nb, maxn;
	int around;
	Rune *s, *b, *c;

	if(n==0 || n>ct->file->b.nc)
		return FALSE;
	if(2*n > RBUFSIZE){
		warning(nil, "string too long\n");
		return FALSE;
	}
	maxn = max(2*n, RBUFSIZE);
	s = fbufalloc();
	b = s;
	nb = 0;
	b[nb] = 0;
	around = 0;
	q = ct->q1;
	for(;;){
		if(q >= ct->file->b.nc){
			q = 0;
			around = 1;
			nb = 0;
			b[nb] = 0;
		}
		if(nb > 0){
			c = runestrchr(b, r[0]);
			if(c == nil){
				q += nb;
				nb = 0;
				b[nb] = 0;
				if(around && q>=ct->q1)
					break;
				continue;
			}
			q += (c-b);
			nb -= (c-b);
			b = c;
		}
		/* reload if buffer covers neither string nor rest of file */
		if(nb<n && nb!=ct->file->b.nc-q){
			nb = ct->file->b.nc-q;
			if(nb >= maxn)
				nb = maxn-1;
			bufread(&ct->file->b, q, s, nb);
			b = s;
			b[nb] = '\0';
		}
		/* this runeeq is fishy but the null at b[nb] makes it safe */
		if(runeeq(b, n, r, n)==TRUE){
			if(ct->w){
				textshow(ct, q, q+n, 1);
				winsettag(ct->w);
			}else{
				ct->q0 = q;
				ct->q1 = q+n;
			}
			seltext = ct;
			fbuffree(s);
			return TRUE;
		}
		--nb;
		b++;
		q++;
		if(around && q>=ct->q1)
			break;
	}
	fbuffree(s);
	return FALSE;
}

int
isfilec(Rune r)
{
	static Rune Lx[] = { '.', '-', '+', '/', ':', 0 };
	if(isalnum(r))
		return TRUE;
	if(runestrchr(Lx, r))
		return TRUE;
	return FALSE;
}

/* Runestr wrapper for cleanname */
Runestr
cleanrname(Runestr rs)
{
	char *s;
	int nb, nulls;

	s = runetobyte(rs.r, rs.nr);
	cleanname(s);
	cvttorunes(s, strlen(s), rs.r, &nb, &rs.nr, &nulls);
	free(s);
	return rs;
}

Runestr
includefile(Rune *dir, Rune *file, int nfile)
{
	int m, n;
	char *a;
	Rune *r;
	static Rune Lslash[] = { '/', 0 };

	m = runestrlen(dir);
	a = emalloc((m+1+nfile)*UTFmax+1);
	sprint(a, "%S/%.*S", dir, nfile, file);
	n = access(a, 0);
	free(a);
	if(n < 0)
		return runestr(nil, 0);
	r = runemalloc(m+1+nfile);
	runemove(r, dir, m);
	runemove(r+m, Lslash, 1);
	runemove(r+m+1, file, nfile);
	free(file);
	return cleanrname(runestr(r, m+1+nfile));
}

static	Rune	*objdir;

Runestr
includename(Text *t, Rune *r, int n)
{
	Window *w;
	char buf[128];
	Rune Lsysinclude[] = { '/', 's', 'y', 's', '/', 'i', 'n', 'c', 'l', 'u', 'd', 'e', 0 };
	Rune Lusrinclude[] = { '/', 'u', 's', 'r', '/', 'i', 'n', 'c', 'l', 'u', 'd', 'e', 0 };
	Rune Lusrlocalinclude[] = { '/', 'u', 's', 'r', '/', 'l', 'o', 'c', 'a', 'l', 
			'/', 'i', 'n', 'c', 'l', 'u', 'd', 'e', 0 };
	Rune Lusrlocalplan9include[] = { '/', 'u', 's', 'r', '/', 'l', 'o', 'c', 'a', 'l', 
			'/', 'p', 'l', 'a', 'n', '9', '/', 'i', 'n', 'c', 'l', 'u', 'd', 'e', 0 };
	Runestr file;
	int i;

	if(objdir==nil && objtype!=nil){
		sprint(buf, "/%s/include", objtype);
		objdir = bytetorune(buf, &i);
		objdir = runerealloc(objdir, i+1);
		objdir[i] = '\0';	
	}

	w = t->w;
	if(n==0 || r[0]=='/' || w==nil)
		goto Rescue;
	if(n>2 && r[0]=='.' && r[1]=='/')
		goto Rescue;
	file.r = nil;
	file.nr = 0;
	for(i=0; i<w->nincl && file.r==nil; i++)
		file = includefile(w->incl[i], r, n);

	if(file.r == nil)
		file = includefile(Lsysinclude, r, n);
	if(file.r == nil)
		file = includefile(Lusrlocalplan9include, r, n);
	if(file.r == nil)
		file = includefile(Lusrlocalinclude, r, n);
	if(file.r == nil)
		file = includefile(Lusrinclude, r, n);
	if(file.r==nil && objdir!=nil)
		file = includefile(objdir, r, n);
	if(file.r == nil)
		goto Rescue;
	return file;

    Rescue:
	return runestr(r, n);
}

Runestr
dirname(Text *t, Rune *r, int n)
{
	Rune *b, c;
	uint m, nt;
	int slash;
	Runestr tmp;

	b = nil;
	if(t==nil || t->w==nil)
		goto Rescue;
	nt = t->w->tag.file->b.nc;
	if(nt == 0)
		goto Rescue;
	if(n>=1 && r[0]=='/')
		goto Rescue;
	b = runemalloc(nt+n+1);
	bufread(&t->w->tag.file->b, 0, b, nt);
	slash = -1;
	for(m=0; m<nt; m++){
		c = b[m];
		if(c == '/')
			slash = m;
		if(c==' ' || c=='\t')
			break;
	}
	if(slash < 0)
		goto Rescue;
	runemove(b+slash+1, r, n);
	free(r);
	return cleanrname(runestr(b, slash+1+n));

    Rescue:
	free(b);
	tmp = runestr(r, n);
	if(r)
		return cleanrname(tmp);
	return tmp;
}

int
expandfile(Text *t, uint q0, uint q1, Expand *e)
{
	int i, n, nname, colon, eval;
	uint amin, amax;
	Rune *r, c;
	Window *w;
	Runestr rs;

	amax = q1;
	if(q1 == q0){
		colon = -1;
		while(q1<t->file->b.nc && isfilec(c=textreadc(t, q1))){
			if(c == ':'){
				colon = q1;
				break;
			}
			q1++;
		}
		while(q0>0 && (isfilec(c=textreadc(t, q0-1)) || isaddrc(c) || isregexc(c))){
			q0--;
			if(colon<0 && c==':')
				colon = q0;
		}
		/*
		 * if it looks like it might begin file: , consume address chars after :
		 * otherwise terminate expansion at :
		 */
		if(colon >= 0){
			q1 = colon;
			if(colon<t->file->b.nc-1 && isaddrc(textreadc(t, colon+1))){
				q1 = colon+1;
				while(q1<t->file->b.nc && isaddrc(textreadc(t, q1)))
					q1++;
			}
		}
		if(q1 > q0)
			if(colon >= 0){	/* stop at white space */
				for(amax=colon+1; amax<t->file->b.nc; amax++)
					if((c=textreadc(t, amax))==' ' || c=='\t' || c=='\n')
						break;
			}else
				amax = t->file->b.nc;
	}
	amin = amax;
	e->q0 = q0;
	e->q1 = q1;
	n = q1-q0;
	if(n == 0)
		return FALSE;
	/* see if it's a file name */
	r = runemalloc(n);
	bufread(&t->file->b, q0, r, n);
	/* first, does it have bad chars? */
	nname = -1;
	for(i=0; i<n; i++){
		c = r[i];
		if(c==':' && nname<0){
			if(q0+i+1<t->file->b.nc && (i==n-1 || isaddrc(textreadc(t, q0+i+1))))
				amin = q0+i;
			else
				goto Isntfile;
			nname = i;
		}
	}
	if(nname == -1)
		nname = n;
	for(i=0; i<nname; i++)
		if(!isfilec(r[i]))
			goto Isntfile;
	/*
	 * See if it's a file name in <>, and turn that into an include
	 * file name if so.  Should probably do it for "" too, but that's not
	 * restrictive enough syntax and checking for a #include earlier on the
	 * line would be silly.
	 */
	if(q0>0 && textreadc(t, q0-1)=='<' && q1<t->file->b.nc && textreadc(t, q1)=='>'){
		rs = includename(t, r, nname);
		r = rs.r;
		nname = rs.nr;
	}
	else if(amin == q0)
		goto Isfile;
	else{
		rs = dirname(t, r, nname);
		r = rs.r;
		nname = rs.nr;
	}
	e->bname = runetobyte(r, nname);
	/* if it's already a window name, it's a file */
	w = lookfile(r, nname);
	if(w != nil)
		goto Isfile;
	/* if it's the name of a file, it's a file */
	if(ismtpt(e->bname) || access(e->bname, 0) < 0){
		free(e->bname);
		e->bname = nil;
		goto Isntfile;
	}

  Isfile:
	e->name = r;
	e->nname = nname;
	e->u.at = t;
	e->a0 = amin+1;
	eval = FALSE;
	address(TRUE, nil, range(-1,-1), range(0,0), t, e->a0, amax, tgetc, &eval, (uint*)&e->a1);
	return TRUE;

   Isntfile:
	free(r);
	return FALSE;
}

int
expand(Text *t, uint q0, uint q1, Expand *e)
{
	memset(e, 0, sizeof *e);
	e->agetc = tgetc;
	/* if in selection, choose selection */
	e->jump = TRUE;
	if(q1==q0 && t->q1>t->q0 && t->q0<=q0 && q0<=t->q1){
		q0 = t->q0;
		q1 = t->q1;
		if(t->what == Tag)
			e->jump = FALSE;
	}

	if(expandfile(t, q0, q1, e))
		return TRUE;

	if(q0 == q1){
		while(q1<t->file->b.nc && isalnum(textreadc(t, q1)))
			q1++;
		while(q0>0 && isalnum(textreadc(t, q0-1)))
			q0--;
	}
	e->q0 = q0;
	e->q1 = q1;
	return q1 > q0;
}

Window*
lookfile(Rune *s, int n)
{
	int i, j, k;
	Window *w;
	Column *c;
	Text *t;

	/* avoid terminal slash on directories */
	if(n>1 && s[n-1] == '/')
		--n;
	for(j=0; j<row.ncol; j++){
		c = row.col[j];
		for(i=0; i<c->nw; i++){
			w = c->w[i];
			t = &w->body;
			k = t->file->nname;
			if(k>1 && t->file->name[k-1] == '/')
				k--;
			if(runeeq(t->file->name, k, s, n)){
				w = w->body.file->curtext->w;
				if(w->col != nil)	/* protect against race deleting w */
					return w;
			}
		}
	}
	return nil;
}

Window*
lookid(int id, int dump)
{
	int i, j;
	Window *w;
	Column *c;

	for(j=0; j<row.ncol; j++){
		c = row.col[j];
		for(i=0; i<c->nw; i++){
			w = c->w[i];
			if(dump && w->dumpid == id)
				return w;
			if(!dump && w->id == id)
				return w;
		}
	}
	return nil;
}


Window*
openfile(Text *t, Expand *e)
{
	Range r;
	Window *w, *ow;
	int eval, i, n;
	Rune *rp;
	Runestr rs;
	uint dummy;

	r.q0 = 0;
	r.q1 = 0;
	if(e->nname == 0){
		w = t->w;
		if(w == nil)
			return nil;
	}else{
		w = lookfile(e->name, e->nname);
		if(w == nil && e->name[0] != '/'){
			/*
			 * Unrooted path in new window.
			 * This can happen if we type a pwd-relative path
			 * in the topmost tag or the column tags.  
			 * Most of the time plumber takes care of these,
			 * but plumber might not be running or might not
			 * be configured to accept plumbed directories.
			 * Make the name a full path, just like we would if
			 * opening via the plumber.
			 */
			n = utflen(wdir)+1+e->nname+1;
			rp = runemalloc(n);
			runesnprint(rp, n, "%s/%.*S", wdir, e->nname, e->name);
			rs = cleanrname(runestr(rp, n-1));
			free(e->name);
			e->name = rs.r;
			e->nname = rs.nr;
			w = lookfile(e->name, e->nname);
		}
	}
	if(w){
		t = &w->body;
		if(!t->col->safe && t->fr.maxlines==0) /* window is obscured by full-column window */
			colgrow(t->col, t->col->w[0], 1);
	}else{
		ow = nil;
		if(t)
			ow = t->w;
		w = makenewwindow(t);
		t = &w->body;
		winsetname(w, e->name, e->nname);
		if(textload(t, 0, e->bname, 1) >= 0)
			t->file->unread = FALSE;
		t->file->mod = FALSE;
		t->w->dirty = FALSE;
		winsettag(t->w);
		textsetselect(&t->w->tag, t->w->tag.file->b.nc, t->w->tag.file->b.nc);
		if(ow != nil){
			for(i=ow->nincl; --i>=0; ){
				n = runestrlen(ow->incl[i]);
				rp = runemalloc(n);
				runemove(rp, ow->incl[i], n);
				winaddincl(w, rp, n);
			}
			for(i=0; i < NINDENT; i++)
				w->indent[i] = ow->indent[i];
		}else
			for(i=0; i < NINDENT; i++)
				w->indent[i] = globalindent[i];
		xfidlog(w, "new");
	}
	if(e->a1 == e->a0)
		eval = FALSE;
	else{
		eval = TRUE;
		r = address(TRUE, t, range(-1,-1), range(t->q0, t->q1), e->u.at, e->a0, e->a1, e->agetc, &eval, &dummy);
		if(r.q0 > r.q1) {
			eval = FALSE;
			warning(nil, "addresses out of order\n");
		}
		if(eval == FALSE)
			e->jump = FALSE;	/* don't jump if invalid address */
	}
	if(eval == FALSE){
		r.q0 = t->q0;
		r.q1 = t->q1;
	}
	textshow(t, r.q0, r.q1, 1);
	winsettag(t->w);
	seltext = t;
	if(e->jump)
		moveto(mousectl, addpt(frptofchar(&t->fr, t->fr.p0), Pt(4, font->height-4)));
	return w;
}

void
new(Text *et, Text *t, Text *argt, int flag1, int flag2, Rune *arg, int narg)
{
	int ndone;
	Rune *a, *f;
	int na, nf;
	Expand e;
	Runestr rs;
	Window *w;

	getarg(argt, FALSE, TRUE, &a, &na);
	if(a){
		new(et, t, nil, flag1, flag2, a, na);
		if(narg == 0)
			return;
	}
	/* loop condition: *arg is not a blank */
	for(ndone=0; ; ndone++){
		a = findbl(arg, narg, &na);
		if(a == arg){
			if(ndone==0 && et->col!=nil) {
				w = coladd(et->col, nil, nil, -1);
				winsettag(w);
				xfidlog(w, "new");
			}
			break;
		}
		nf = narg-na;
		f = runemalloc(nf);
		runemove(f, arg, nf);
		rs = dirname(et, f, nf);
		memset(&e, 0, sizeof e);
		e.name = rs.r;
		e.nname = rs.nr;
		e.bname = runetobyte(rs.r, rs.nr);
		e.jump = TRUE;
		openfile(et, &e);
		free(e.name);
		free(e.bname);
		arg = skipbl(a, na, &narg);
	}
}
