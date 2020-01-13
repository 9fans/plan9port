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
#include "edit.h"
#include "fns.h"

int	Glooping;
int	nest;
char	Enoname[] = "no file name given";

Address	addr;
File	*menu;
Rangeset	sel;
extern	Text*	curtext;
Rune	*collection;
int	ncollection;

int	append(File*, Cmd*, long);
int	pdisplay(File*);
void	pfilename(File*);
void	looper(File*, Cmd*, int);
void	filelooper(Text*, Cmd*, int);
void	linelooper(File*, Cmd*);
Address	lineaddr(long, Address, int);
int	filematch(File*, String*);
File	*tofile(String*);
Rune*	cmdname(File *f, String *s, int);
void	runpipe(Text*, int, Rune*, int, int);

void
clearcollection(void)
{
	free(collection);
	collection = nil;
	ncollection = 0;
}

void
resetxec(void)
{
	Glooping = nest = 0;
	clearcollection();
}

void
mkaddr(Address *a, File *f)
{
	a->r.q0 = f->curtext->q0;
	a->r.q1 = f->curtext->q1;
	a->f = f;
}

int
cmdexec(Text *t, Cmd *cp)
{
	int i;
	Addr *ap;
	File *f;
	Window *w;
	Address dot;

	if(t == nil)
		w = nil;
	else
		w = t->w;
	if(w==nil && (cp->addr==0 || cp->addr->type!='"') &&
	    !utfrune("bBnqUXY!", cp->cmdc) &&
	    !(cp->cmdc=='D' && cp->u.text))
		editerror("no current window");
	i = cmdlookup(cp->cmdc);	/* will be -1 for '{' */
	f = nil;
	if(t && t->w){
		t = &t->w->body;
		f = t->file;
		f->curtext = t;
	}
	if(i>=0 && cmdtab[i].defaddr != aNo){
		if((ap=cp->addr)==0 && cp->cmdc!='\n'){
			cp->addr = ap = newaddr();
			ap->type = '.';
			if(cmdtab[i].defaddr == aAll)
				ap->type = '*';
		}else if(ap && ap->type=='"' && ap->next==0 && cp->cmdc!='\n'){
			ap->next = newaddr();
			ap->next->type = '.';
			if(cmdtab[i].defaddr == aAll)
				ap->next->type = '*';
		}
		if(cp->addr){	/* may be false for '\n' (only) */
			static Address none = {0,0,nil};
			if(f){
				mkaddr(&dot, f);
				addr = cmdaddress(ap, dot, 0);
			}else	/* a " */
				addr = cmdaddress(ap, none, 0);
			f = addr.f;
			t = f->curtext;
		}
	}
	switch(cp->cmdc){
	case '{':
		mkaddr(&dot, f);
		if(cp->addr != nil)
			dot = cmdaddress(cp->addr, dot, 0);
		for(cp = cp->u.cmd; cp; cp = cp->next){
			if(dot.r.q1 > t->file->b.nc)
				editerror("dot extends past end of buffer during { command");
			t->q0 = dot.r.q0;
			t->q1 = dot.r.q1;
			cmdexec(t, cp);
		}
		break;
	default:
		if(i < 0)
			editerror("unknown command %c in cmdexec", cp->cmdc);
		i = (*cmdtab[i].fn)(t, cp);
		return i;
	}
	return 1;
}

char*
edittext(Window *w, int q, Rune *r, int nr)
{
	File *f;

	f = w->body.file;
	switch(editing){
	case Inactive:
		return "permission denied";
	case Inserting:
		eloginsert(f, q, r, nr);
		return nil;
	case Collecting:
		collection = runerealloc(collection, ncollection+nr+1);
		runemove(collection+ncollection, r, nr);
		ncollection += nr;
		collection[ncollection] = '\0';
		return nil;
	default:
		return "unknown state in edittext";
	}
}

/* string is known to be NUL-terminated */
Rune*
filelist(Text *t, Rune *r, int nr)
{
	if(nr == 0)
		return nil;
	r = skipbl(r, nr, &nr);
	if(r[0] != '<')
		return runestrdup(r);
	/* use < command to collect text */
	clearcollection();
	runpipe(t, '<', r+1, nr-1, Collecting);
	return collection;
}

int
a_cmd(Text *t, Cmd *cp)
{
	return append(t->file, cp, addr.r.q1);
}

int
b_cmd(Text *t, Cmd *cp)
{
	File *f;

	USED(t);
	f = tofile(cp->u.text);
	if(nest == 0)
		pfilename(f);
	curtext = f->curtext;
	return TRUE;
}

int
B_cmd(Text *t, Cmd *cp)
{
	Rune *list, *r, *s;
	int nr;

	list = filelist(t, cp->u.text->r, cp->u.text->n);
	if(list == nil)
		editerror(Enoname);
	r = list;
	nr = runestrlen(r);
	r = skipbl(r, nr, &nr);
	if(nr == 0)
		new(t, t, nil, 0, 0, r, 0);
	else while(nr > 0){
		s = findbl(r, nr, &nr);
		*s = '\0';
		new(t, t, nil, 0, 0, r, runestrlen(r));
		if(nr > 0)
			r = skipbl(s+1, nr-1, &nr);
	}
	clearcollection();
	return TRUE;
}

int
c_cmd(Text *t, Cmd *cp)
{
	elogreplace(t->file, addr.r.q0, addr.r.q1, cp->u.text->r, cp->u.text->n);
	t->q0 = addr.r.q0;
	t->q1 = addr.r.q1;
	return TRUE;
}

int
d_cmd(Text *t, Cmd *cp)
{
	USED(cp);
	if(addr.r.q1 > addr.r.q0)
		elogdelete(t->file, addr.r.q0, addr.r.q1);
	t->q0 = addr.r.q0;
	t->q1 = addr.r.q0;
	return TRUE;
}

void
D1(Text *t)
{
	if(t->w->body.file->ntext>1 || winclean(t->w, FALSE))
		colclose(t->col, t->w, TRUE);
}

int
D_cmd(Text *t, Cmd *cp)
{
	Rune *list, *r, *s, *n;
	int nr, nn;
	Window *w;
	Runestr dir, rs;
	char buf[128];

	list = filelist(t, cp->u.text->r, cp->u.text->n);
	if(list == nil){
		D1(t);
		return TRUE;
	}
	dir = dirname(t, nil, 0);
	r = list;
	nr = runestrlen(r);
	r = skipbl(r, nr, &nr);
	do{
		s = findbl(r, nr, &nr);
		*s = '\0';
		/* first time through, could be empty string, meaning delete file empty name */
		nn = runestrlen(r);
		if(r[0]=='/' || nn==0 || dir.nr==0){
			rs.r = runestrdup(r);
			rs.nr = nn;
		}else{
			n = runemalloc(dir.nr+1+nn);
			runemove(n, dir.r, dir.nr);
			n[dir.nr] = '/';
			runemove(n+dir.nr+1, r, nn);
			rs = cleanrname(runestr(n, dir.nr+1+nn));
		}
		w = lookfile(rs.r, rs.nr);
		if(w == nil){
			snprint(buf, sizeof buf, "no such file %.*S", rs.nr, rs.r);
			free(rs.r);
			editerror(buf);
		}
		free(rs.r);
		D1(&w->body);
		if(nr > 0)
			r = skipbl(s+1, nr-1, &nr);
	}while(nr > 0);
	clearcollection();
	free(dir.r);
	return TRUE;
}

static int
readloader(void *v, uint q0, Rune *r, int nr)
{
	if(nr > 0)
		eloginsert(v, q0, r, nr);
	return 0;
}

int
e_cmd(Text *t, Cmd *cp)
{
	Rune *name;
	File *f;
	int i, isdir, q0, q1, fd, nulls, samename, allreplaced;
	char *s, tmp[128];
	Dir *d;

	f = t->file;
	q0 = addr.r.q0;
	q1 = addr.r.q1;
	if(cp->cmdc == 'e'){
		if(winclean(t->w, TRUE)==FALSE)
			editerror("");	/* winclean generated message already */
		q0 = 0;
		q1 = f->b.nc;
	}
	allreplaced = (q0==0 && q1==f->b.nc);
	name = cmdname(f, cp->u.text, cp->cmdc=='e');
	if(name == nil)
		editerror(Enoname);
	i = runestrlen(name);
	samename = runeeq(name, i, t->file->name, t->file->nname);
	s = runetobyte(name, i);
	free(name);
	fd = open(s, OREAD);
	if(fd < 0){
		snprint(tmp, sizeof tmp, "can't open %s: %r", s);
		free(s);
		editerror(tmp);
	}
	d = dirfstat(fd);
	isdir = (d!=nil && (d->qid.type&QTDIR));
	free(d);
	if(isdir){
		close(fd);
		snprint(tmp, sizeof tmp, "%s is a directory", s);
		free(s);
		editerror(tmp);
	}
	elogdelete(f, q0, q1);
	nulls = 0;
	loadfile(fd, q1, &nulls, readloader, f, nil);
	free(s);
	close(fd);
	if(nulls)
		warning(nil, "%s: NUL bytes elided\n", s);
	else if(allreplaced && samename)
		f->editclean = TRUE;
	return TRUE;
}

static Rune Lempty[] = { 0 };
int
f_cmd(Text *t, Cmd *cp)
{
	Rune *name;
	String *str;
	String empty;

	if(cp->u.text == nil){
		empty.n = 0;
		empty.r = Lempty;
		str = &empty;
	}else
		str = cp->u.text;
	name = cmdname(t->file, str, TRUE);
	free(name);
	pfilename(t->file);
	return TRUE;
}

int
g_cmd(Text *t, Cmd *cp)
{
	if(t->file != addr.f){
		warning(nil, "internal error: g_cmd f!=addr.f\n");
		return FALSE;
	}
	if(rxcompile(cp->re->r) == FALSE)
		editerror("bad regexp in g command");
	if(rxexecute(t, nil, addr.r.q0, addr.r.q1, &sel) ^ cp->cmdc=='v'){
		t->q0 = addr.r.q0;
		t->q1 = addr.r.q1;
		return cmdexec(t, cp->u.cmd);
	}
	return TRUE;
}

int
i_cmd(Text *t, Cmd *cp)
{
	return append(t->file, cp, addr.r.q0);
}

void
copy(File *f, Address addr2)
{
	long p;
	int ni;
	Rune *buf;

	buf = fbufalloc();
	for(p=addr.r.q0; p<addr.r.q1; p+=ni){
		ni = addr.r.q1-p;
		if(ni > RBUFSIZE)
			ni = RBUFSIZE;
		bufread(&f->b, p, buf, ni);
		eloginsert(addr2.f, addr2.r.q1, buf, ni);
	}
	fbuffree(buf);
}

void
move(File *f, Address addr2)
{
	if(addr.f!=addr2.f || addr.r.q1<=addr2.r.q0){
		elogdelete(f, addr.r.q0, addr.r.q1);
		copy(f, addr2);
	}else if(addr.r.q0 >= addr2.r.q1){
		copy(f, addr2);
		elogdelete(f, addr.r.q0, addr.r.q1);
	}else if(addr.r.q0==addr2.r.q0 && addr.r.q1==addr2.r.q1){
		; /* move to self; no-op */
	}else
		editerror("move overlaps itself");
}

int
m_cmd(Text *t, Cmd *cp)
{
	Address dot, addr2;

	mkaddr(&dot, t->file);
	addr2 = cmdaddress(cp->u.mtaddr, dot, 0);
	if(cp->cmdc == 'm')
		move(t->file, addr2);
	else
		copy(t->file, addr2);
	return TRUE;
}

int
p_cmd(Text *t, Cmd *cp)
{
	USED(cp);
	return pdisplay(t->file);
}

int
s_cmd(Text *t, Cmd *cp)
{
	int i, j, k, c, m, n, nrp, didsub;
	long p1, op, delta;
	String *buf;
	Rangeset *rp;
	char *err;
	Rune *rbuf;

	n = cp->num;
	op= -1;
	if(rxcompile(cp->re->r) == FALSE)
		editerror("bad regexp in s command");
	nrp = 0;
	rp = nil;
	delta = 0;
	didsub = FALSE;
	for(p1 = addr.r.q0; p1<=addr.r.q1 && rxexecute(t, nil, p1, addr.r.q1, &sel); ){
		if(sel.r[0].q0 == sel.r[0].q1){	/* empty match? */
			if(sel.r[0].q0 == op){
				p1++;
				continue;
			}
			p1 = sel.r[0].q1+1;
		}else
			p1 = sel.r[0].q1;
		op = sel.r[0].q1;
		if(--n>0)
			continue;
		nrp++;
		rp = erealloc(rp, nrp*sizeof(Rangeset));
		rp[nrp-1] = sel;
	}
	rbuf = fbufalloc();
	buf = allocstring(0);
	for(m=0; m<nrp; m++){
		buf->n = 0;
		buf->r[0] = '\0';
		sel = rp[m];
		for(i = 0; i<cp->u.text->n; i++)
			if((c = cp->u.text->r[i])=='\\' && i<cp->u.text->n-1){
				c = cp->u.text->r[++i];
				if('1'<=c && c<='9') {
					j = c-'0';
					if(sel.r[j].q1-sel.r[j].q0>RBUFSIZE){
						err = "replacement string too long";
						goto Err;
					}
					bufread(&t->file->b, sel.r[j].q0, rbuf, sel.r[j].q1-sel.r[j].q0);
					for(k=0; k<sel.r[j].q1-sel.r[j].q0; k++)
						Straddc(buf, rbuf[k]);
				}else
				 	Straddc(buf, c);
			}else if(c!='&')
				Straddc(buf, c);
			else{
				if(sel.r[0].q1-sel.r[0].q0>RBUFSIZE){
					err = "right hand side too long in substitution";
					goto Err;
				}
				bufread(&t->file->b, sel.r[0].q0, rbuf, sel.r[0].q1-sel.r[0].q0);
				for(k=0; k<sel.r[0].q1-sel.r[0].q0; k++)
					Straddc(buf, rbuf[k]);
			}
		elogreplace(t->file, sel.r[0].q0, sel.r[0].q1,  buf->r, buf->n);
		delta -= sel.r[0].q1-sel.r[0].q0;
		delta += buf->n;
		didsub = 1;
		if(!cp->flag)
			break;
	}
	free(rp);
	freestring(buf);
	fbuffree(rbuf);
	if(!didsub && nest==0)
		editerror("no substitution");
	t->q0 = addr.r.q0;
	t->q1 = addr.r.q1;
	return TRUE;

Err:
	free(rp);
	freestring(buf);
	fbuffree(rbuf);
	editerror(err);
	return FALSE;
}

int
u_cmd(Text *t, Cmd *cp)
{
	int n, oseq, flag;

	n = cp->num;
	flag = TRUE;
	if(n < 0){
		n = -n;
		flag = FALSE;
	}
	oseq = -1;
	while(n-->0 && t->file->seq!=oseq){
		oseq = t->file->seq;
		undo(t, nil, nil, flag, 0, nil, 0);
	}
	return TRUE;
}

int
w_cmd(Text *t, Cmd *cp)
{
	Rune *r;
	File *f;

	f = t->file;
	if(f->seq == seq)
		editerror("can't write file with pending modifications");
	r = cmdname(f, cp->u.text, FALSE);
	if(r == nil)
		editerror("no name specified for 'w' command");
	putfile(f, addr.r.q0, addr.r.q1, r, runestrlen(r));
	/* r is freed by putfile */
	return TRUE;
}

int
x_cmd(Text *t, Cmd *cp)
{
	if(cp->re)
		looper(t->file, cp, cp->cmdc=='x');
	else
		linelooper(t->file, cp);
	return TRUE;
}

int
X_cmd(Text *t, Cmd *cp)
{
	USED(t);

	filelooper(t, cp, cp->cmdc=='X');
	return TRUE;
}

void
runpipe(Text *t, int cmd, Rune *cr, int ncr, int state)
{
	Rune *r, *s;
	int n;
	Runestr dir;
	Window *w;
	QLock *q;

	r = skipbl(cr, ncr, &n);
	if(n == 0)
		editerror("no command specified for %c", cmd);
	w = nil;
	if(state == Inserting){
		w = t->w;
		t->q0 = addr.r.q0;
		t->q1 = addr.r.q1;
		if(cmd == '<' || cmd=='|')
			elogdelete(t->file, t->q0, t->q1);
	}
	s = runemalloc(n+2);
	s[0] = cmd;
	runemove(s+1, r, n);
	n++;
	dir.r = nil;
	dir.nr = 0;
	if(t != nil)
		dir = dirname(t, nil, 0);
	if(dir.nr==1 && dir.r[0]=='.'){	/* sigh */
		free(dir.r);
		dir.r = nil;
		dir.nr = 0;
	}
	editing = state;
	if(t!=nil && t->w!=nil)
		incref(&t->w->ref);	/* run will decref */
	run(w, runetobyte(s, n), dir.r, dir.nr, TRUE, nil, nil, TRUE);
	free(s);
	if(t!=nil && t->w!=nil)
		winunlock(t->w);
	qunlock(&row.lk);
	recvul(cedit);
	/*
	 * The editoutlk exists only so that we can tell when
	 * the editout file has been closed.  It can get closed *after*
	 * the process exits because, since the process cannot be
	 * connected directly to editout (no 9P kernel support),
	 * the process is actually connected to a pipe to another
	 * process (arranged via 9pserve) that reads from the pipe
	 * and then writes the data in the pipe to editout using
	 * 9P transactions.  This process might still have a couple
	 * writes left to copy after the original process has exited.
	 */
	if(w)
		q = &w->editoutlk;
	else
		q = &editoutlk;
	qlock(q);	/* wait for file to close */
	qunlock(q);
	qlock(&row.lk);
	editing = Inactive;
	if(t!=nil && t->w!=nil)
		winlock(t->w, 'M');
}

int
pipe_cmd(Text *t, Cmd *cp)
{
	runpipe(t, cp->cmdc, cp->u.text->r, cp->u.text->n, Inserting);
	return TRUE;
}

long
nlcount(Text *t, long q0, long q1, long *pnr)
{
	long nl, start;
	Rune *buf;
	int i, nbuf;

	buf = fbufalloc();
	nbuf = 0;
	i = nl = 0;
	start = q0;
	while(q0 < q1){
		if(i == nbuf){
			nbuf = q1-q0;
			if(nbuf > RBUFSIZE)
				nbuf = RBUFSIZE;
			bufread(&t->file->b, q0, buf, nbuf);
			i = 0;
		}
		if(buf[i++] == '\n') {
			start = q0+1;
			nl++;
		}
		q0++;
	}
	fbuffree(buf);
	if(pnr != nil)
		*pnr = q0 - start;
	return nl;
}

enum {
	PosnLine = 0,
	PosnChars = 1,
	PosnLineChars = 2,
};

void
printposn(Text *t, int mode)
{
	long l1, l2, r1, r2;

	if (t != nil && t->file != nil && t->file->name != nil)
		warning(nil, "%.*S:", t->file->nname, t->file->name);

	switch(mode) {
	case PosnChars:
		warning(nil, "#%d", addr.r.q0);
		if(addr.r.q1 != addr.r.q0)
			warning(nil, ",#%d", addr.r.q1);
		warning(nil, "\n");
		return;

	default:
	case PosnLine:
		l1 = 1+nlcount(t, 0, addr.r.q0, nil);
		l2 = l1+nlcount(t, addr.r.q0, addr.r.q1, nil);
		/* check if addr ends with '\n' */
		if(addr.r.q1>0 && addr.r.q1>addr.r.q0 && textreadc(t, addr.r.q1-1)=='\n')
			--l2;
		warning(nil, "%lud", l1);
		if(l2 != l1)
			warning(nil, ",%lud", l2);
		warning(nil, "\n");
		return;

	case PosnLineChars:
		l1 = 1+nlcount(t, 0, addr.r.q0, &r1);
		l2 = l1+nlcount(t, addr.r.q0, addr.r.q1, &r2);
		if(l2 == l1)
			r2 += r1;
		warning(nil, "%lud+#%d", l1, r1);
		if(l2 != l1)
			warning(nil, ",%lud+#%d", l2, r2);
		warning(nil, "\n");
		return;
	}
}

int
eq_cmd(Text *t, Cmd *cp)
{
	int mode;

	switch(cp->u.text->n){
	case 0:
		mode = PosnLine;
		break;
	case 1:
		if(cp->u.text->r[0] == '#'){
			mode = PosnChars;
			break;
		}
		if(cp->u.text->r[0] == '+'){
			mode = PosnLineChars;
			break;
		}
	default:
		SET(mode);
		editerror("newline expected");
	}
	printposn(t, mode);
	return TRUE;
}

int
nl_cmd(Text *t, Cmd *cp)
{
	Address a;
	File *f;

	f = t->file;
	if(cp->addr == 0){
		/* First put it on newline boundaries */
		mkaddr(&a, f);
		addr = lineaddr(0, a, -1);
		a = lineaddr(0, a, 1);
		addr.r.q1 = a.r.q1;
		if(addr.r.q0==t->q0 && addr.r.q1==t->q1){
			mkaddr(&a, f);
			addr = lineaddr(1, a, 1);
		}
	}
	textshow(t, addr.r.q0, addr.r.q1, 1);
	return TRUE;
}

int
append(File *f, Cmd *cp, long p)
{
	if(cp->u.text->n > 0)
		eloginsert(f, p, cp->u.text->r, cp->u.text->n);
	f->curtext->q0 = p;
	f->curtext->q1 = p;
	return TRUE;
}

int
pdisplay(File *f)
{
	long p1, p2;
	int np;
	Rune *buf;

	p1 = addr.r.q0;
	p2 = addr.r.q1;
	if(p2 > f->b.nc)
		p2 = f->b.nc;
	buf = fbufalloc();
	while(p1 < p2){
		np = p2-p1;
		if(np>RBUFSIZE-1)
			np = RBUFSIZE-1;
		bufread(&f->b, p1, buf, np);
		buf[np] = '\0';
		warning(nil, "%S", buf);
		p1 += np;
	}
	fbuffree(buf);
	f->curtext->q0 = addr.r.q0;
	f->curtext->q1 = addr.r.q1;
	return TRUE;
}

void
pfilename(File *f)
{
	int dirty;
	Window *w;

	w = f->curtext->w;
	/* same check for dirty as in settag, but we know ncache==0 */
	dirty = !w->isdir && !w->isscratch && f->mod;
	warning(nil, "%c%c%c %.*S\n", " '"[dirty],
		'+', " ."[curtext!=nil && curtext->file==f], f->nname, f->name);
}

void
loopcmd(File *f, Cmd *cp, Range *rp, long nrp)
{
	long i;

	for(i=0; i<nrp; i++){
		f->curtext->q0 = rp[i].q0;
		f->curtext->q1 = rp[i].q1;
		cmdexec(f->curtext, cp);
	}
}

void
looper(File *f, Cmd *cp, int xy)
{
	long p, op, nrp;
	Range r, tr;
	Range *rp;

	r = addr.r;
	op= xy? -1 : r.q0;
	nest++;
	if(rxcompile(cp->re->r) == FALSE)
		editerror("bad regexp in %c command", cp->cmdc);
	nrp = 0;
	rp = nil;
	for(p = r.q0; p<=r.q1; ){
		if(!rxexecute(f->curtext, nil, p, r.q1, &sel)){ /* no match, but y should still run */
			if(xy || op>r.q1)
				break;
			tr.q0 = op, tr.q1 = r.q1;
			p = r.q1+1;	/* exit next loop */
		}else{
			if(sel.r[0].q0==sel.r[0].q1){	/* empty match? */
				if(sel.r[0].q0==op){
					p++;
					continue;
				}
				p = sel.r[0].q1+1;
			}else
				p = sel.r[0].q1;
			if(xy)
				tr = sel.r[0];
			else
				tr.q0 = op, tr.q1 = sel.r[0].q0;
		}
		op = sel.r[0].q1;
		nrp++;
		rp = erealloc(rp, nrp*sizeof(Range));
		rp[nrp-1] = tr;
	}
	loopcmd(f, cp->u.cmd, rp, nrp);
	free(rp);
	--nest;
}

void
linelooper(File *f, Cmd *cp)
{
	long nrp, p;
	Range r, linesel;
	Address a, a3;
	Range *rp;

	nest++;
	nrp = 0;
	rp = nil;
	r = addr.r;
	a3.f = f;
	a3.r.q0 = a3.r.q1 = r.q0;
	a = lineaddr(0, a3, 1);
	linesel = a.r;
	for(p = r.q0; p<r.q1; p = a3.r.q1){
		a3.r.q0 = a3.r.q1;
		if(p!=r.q0 || linesel.q1==p){
			a = lineaddr(1, a3, 1);
			linesel = a.r;
		}
		if(linesel.q0 >= r.q1)
			break;
		if(linesel.q1 >= r.q1)
			linesel.q1 = r.q1;
		if(linesel.q1 > linesel.q0)
			if(linesel.q0>=a3.r.q1 && linesel.q1>a3.r.q1){
				a3.r = linesel;
				nrp++;
				rp = erealloc(rp, nrp*sizeof(Range));
				rp[nrp-1] = linesel;
				continue;
			}
		break;
	}
	loopcmd(f, cp->u.cmd, rp, nrp);
	free(rp);
	--nest;
}

struct Looper
{
	Cmd *cp;
	int	XY;
	Window	**w;
	int	nw;
} loopstruct;	/* only one; X and Y can't nest */

void
alllooper(Window *w, void *v)
{
	Text *t;
	struct Looper *lp;
	Cmd *cp;

	lp = v;
	cp = lp->cp;
/*	if(w->isscratch || w->isdir) */
/*		return; */
	t = &w->body;
	/* only use this window if it's the current window for the file */
	if(t->file->curtext != t)
		return;
/*	if(w->nopen[QWevent] > 0) */
/*		return; */
	/* no auto-execute on files without names */
	if(cp->re==nil && t->file->nname==0)
		return;
	if(cp->re==nil || filematch(t->file, cp->re)==lp->XY){
		lp->w = erealloc(lp->w, (lp->nw+1)*sizeof(Window*));
		lp->w[lp->nw++] = w;
	}
}

void
alllocker(Window *w, void *v)
{
	if(v)
		incref(&w->ref);
	else
		winclose(w);
}

void
filelooper(Text *t, Cmd *cp, int XY)
{
	int i;
	Text *targ;

	if(Glooping++)
		editerror("can't nest %c command", "YX"[XY]);
	nest++;

	loopstruct.cp = cp;
	loopstruct.XY = XY;
	if(loopstruct.w)	/* error'ed out last time */
		free(loopstruct.w);
	loopstruct.w = nil;
	loopstruct.nw = 0;
	allwindows(alllooper, &loopstruct);
	/*
	 * add a ref to all windows to keep safe windows accessed by X
	 * that would not otherwise have a ref to hold them up during
	 * the shenanigans.  note this with globalincref so that any
	 * newly created windows start with an extra reference.
	 */
	allwindows(alllocker, (void*)1);
	globalincref = 1;
	
	/*
	 * Unlock the window running the X command.
	 * We'll need to lock and unlock each target window in turn.
	 */
	if(t && t->w)
		winunlock(t->w);
	
	for(i=0; i<loopstruct.nw; i++) {
		targ = &loopstruct.w[i]->body;
		if(targ && targ->w)
			winlock(targ->w, cp->cmdc);
		cmdexec(targ, cp->u.cmd);
		if(targ && targ->w)
			winunlock(targ->w);
	}

	if(t && t->w)
		winlock(t->w, cp->cmdc);

	allwindows(alllocker, (void*)0);
	globalincref = 0;
	free(loopstruct.w);
	loopstruct.w = nil;

	--Glooping;
	--nest;
}

void
nextmatch(File *f, String *r, long p, int sign)
{
	if(rxcompile(r->r) == FALSE)
		editerror("bad regexp in command address");
	if(sign >= 0){
		if(!rxexecute(f->curtext, nil, p, 0x7FFFFFFFL, &sel))
			editerror("no match for regexp");
		if(sel.r[0].q0==sel.r[0].q1 && sel.r[0].q0==p){
			if(++p>f->b.nc)
				p = 0;
			if(!rxexecute(f->curtext, nil, p, 0x7FFFFFFFL, &sel))
				editerror("address");
		}
	}else{
		if(!rxbexecute(f->curtext, p, &sel))
			editerror("no match for regexp");
		if(sel.r[0].q0==sel.r[0].q1 && sel.r[0].q1==p){
			if(--p<0)
				p = f->b.nc;
			if(!rxbexecute(f->curtext, p, &sel))
				editerror("address");
		}
	}
}

File	*matchfile(String*);
Address	charaddr(long, Address, int);
Address	lineaddr(long, Address, int);

Address
cmdaddress(Addr *ap, Address a, int sign)
{
	File *f = a.f;
	Address a1, a2;

	do{
		switch(ap->type){
		case 'l':
		case '#':
			a = (*(ap->type=='#'?charaddr:lineaddr))(ap->num, a, sign);
			break;

		case '.':
			mkaddr(&a, f);
			break;

		case '$':
			a.r.q0 = a.r.q1 = f->b.nc;
			break;

		case '\'':
editerror("can't handle '");
/*			a.r = f->mark; */
			break;

		case '?':
			sign = -sign;
			if(sign == 0)
				sign = -1;
			/* fall through */
		case '/':
			nextmatch(f, ap->u.re, sign>=0? a.r.q1 : a.r.q0, sign);
			a.r = sel.r[0];
			break;

		case '"':
			f = matchfile(ap->u.re);
			mkaddr(&a, f);
			break;

		case '*':
			a.r.q0 = 0, a.r.q1 = f->b.nc;
			return a;

		case ',':
		case ';':
			if(ap->u.left)
				a1 = cmdaddress(ap->u.left, a, 0);
			else
				a1.f = a.f, a1.r.q0 = a1.r.q1 = 0;
			if(ap->type == ';'){
				f = a1.f;
				a = a1;
				f->curtext->q0 = a1.r.q0;
				f->curtext->q1 = a1.r.q1;
			}
			if(ap->next)
				a2 = cmdaddress(ap->next, a, 0);
			else
				a2.f = a.f, a2.r.q0 = a2.r.q1 = f->b.nc;
			if(a1.f != a2.f)
				editerror("addresses in different files");
			a.f = a1.f, a.r.q0 = a1.r.q0, a.r.q1 = a2.r.q1;
			if(a.r.q1 < a.r.q0)
				editerror("addresses out of order");
			return a;

		case '+':
		case '-':
			sign = 1;
			if(ap->type == '-')
				sign = -1;
			if(ap->next==0 || ap->next->type=='+' || ap->next->type=='-')
				a = lineaddr(1L, a, sign);
			break;
		default:
			error("cmdaddress");
			return a;
		}
	}while(ap = ap->next);	/* assign = */
	return a;
}

struct Tofile{
	File		*f;
	String	*r;
};

void
alltofile(Window *w, void *v)
{
	Text *t;
	struct Tofile *tp;

	tp = v;
	if(tp->f != nil)
		return;
	if(w->isscratch || w->isdir)
		return;
	t = &w->body;
	/* only use this window if it's the current window for the file */
	if(t->file->curtext != t)
		return;
/*	if(w->nopen[QWevent] > 0) */
/*		return; */
	if(runeeq(tp->r->r, tp->r->n, t->file->name, t->file->nname))
		tp->f = t->file;
}

File*
tofile(String *r)
{
	struct Tofile t;
	String rr;

	rr.r = skipbl(r->r, r->n, &rr.n);
	t.f = nil;
	t.r = &rr;
	allwindows(alltofile, &t);
	if(t.f == nil)
		editerror("no such file\"%S\"", rr.r);
	return t.f;
}

void
allmatchfile(Window *w, void *v)
{
	struct Tofile *tp;
	Text *t;

	tp = v;
	if(w->isscratch || w->isdir)
		return;
	t = &w->body;
	/* only use this window if it's the current window for the file */
	if(t->file->curtext != t)
		return;
/*	if(w->nopen[QWevent] > 0) */
/*		return; */
	if(filematch(w->body.file, tp->r)){
		if(tp->f != nil)
			editerror("too many files match \"%S\"", tp->r->r);
		tp->f = w->body.file;
	}
}

File*
matchfile(String *r)
{
	struct Tofile tf;

	tf.f = nil;
	tf.r = r;
	allwindows(allmatchfile, &tf);

	if(tf.f == nil)
		editerror("no file matches \"%S\"", r->r);
	return tf.f;
}

int
filematch(File *f, String *r)
{
	char *buf;
	Rune *rbuf;
	Window *w;
	int match, i, dirty;
	Rangeset s;

	/* compile expr first so if we get an error, we haven't allocated anything */
	if(rxcompile(r->r) == FALSE)
		editerror("bad regexp in file match");
	buf = fbufalloc();
	w = f->curtext->w;
	/* same check for dirty as in settag, but we know ncache==0 */
	dirty = !w->isdir && !w->isscratch && f->mod;
	snprint(buf, BUFSIZE, "%c%c%c %.*S\n", " '"[dirty],
		'+', " ."[curtext!=nil && curtext->file==f], f->nname, f->name);
	rbuf = bytetorune(buf, &i);
	fbuffree(buf);
	match = rxexecute(nil, rbuf, 0, i, &s);
	free(rbuf);
	return match;
}

Address
charaddr(long l, Address addr, int sign)
{
	if(sign == 0)
		addr.r.q0 = addr.r.q1 = l;
	else if(sign < 0)
		addr.r.q1 = addr.r.q0 -= l;
	else if(sign > 0)
		addr.r.q0 = addr.r.q1 += l;
	if(addr.r.q0<0 || addr.r.q1>addr.f->b.nc)
		editerror("address out of range");
	return addr;
}

Address
lineaddr(long l, Address addr, int sign)
{
	int n;
	int c;
	File *f = addr.f;
	Address a;
	long p;

	a.f = f;
	if(sign >= 0){
		if(l == 0){
			if(sign==0 || addr.r.q1==0){
				a.r.q0 = a.r.q1 = 0;
				return a;
			}
			a.r.q0 = addr.r.q1;
			p = addr.r.q1-1;
		}else{
			if(sign==0 || addr.r.q1==0){
				p = 0;
				n = 1;
			}else{
				p = addr.r.q1-1;
				n = textreadc(f->curtext, p++)=='\n';
			}
			while(n < l){
				if(p >= f->b.nc)
					editerror("address out of range");
				if(textreadc(f->curtext, p++) == '\n')
					n++;
			}
			a.r.q0 = p;
		}
		while(p < f->b.nc && textreadc(f->curtext, p++)!='\n')
			;
		a.r.q1 = p;
	}else{
		p = addr.r.q0;
		if(l == 0)
			a.r.q1 = addr.r.q0;
		else{
			for(n = 0; n<l; ){	/* always runs once */
				if(p == 0){
					if(++n != l)
						editerror("address out of range");
				}else{
					c = textreadc(f->curtext, p-1);
					if(c != '\n' || ++n != l)
						p--;
				}
			}
			a.r.q1 = p;
			if(p > 0)
				p--;
		}
		while(p > 0 && textreadc(f->curtext, p-1)!='\n')	/* lines start after a newline */
			p--;
		a.r.q0 = p;
	}
	return a;
}

struct Filecheck
{
	File	*f;
	Rune	*r;
	int nr;
};

void
allfilecheck(Window *w, void *v)
{
	struct Filecheck *fp;
	File *f;

	fp = v;
	f = w->body.file;
	if(w->body.file == fp->f)
		return;
	if(runeeq(fp->r, fp->nr, f->name, f->nname))
		warning(nil, "warning: duplicate file name \"%.*S\"\n", fp->nr, fp->r);
}

Rune*
cmdname(File *f, String *str, int set)
{
	Rune *r, *s;
	int n;
	struct Filecheck fc;
	Runestr newname;

	r = nil;
	n = str->n;
	s = str->r;
	if(n == 0){
		/* no name; use existing */
		if(f->nname == 0)
			return nil;
		r = runemalloc(f->nname+1);
		runemove(r, f->name, f->nname);
		return r;
	}
	s = skipbl(s, n, &n);
	if(n == 0)
		goto Return;

	if(s[0] == '/'){
		r = runemalloc(n+1);
		runemove(r, s, n);
	}else{
		newname = dirname(f->curtext, runestrdup(s), n);
		n = newname.nr;
		r = runemalloc(n+1);	/* NUL terminate */
		runemove(r, newname.r, n);
		free(newname.r);
	}
	fc.f = f;
	fc.r = r;
	fc.nr = n;
	allwindows(allfilecheck, &fc);
	if(f->nname == 0)
		set = TRUE;

    Return:
	if(set && !runeeq(r, n, f->name, f->nname)){
		filemark(f);
		f->mod = TRUE;
		f->curtext->w->dirty = TRUE;
		winsetname(f->curtext->w, r, n);
	}
	return r;
}
