#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <plumb.h>
#include <9pclient.h>
#include "dat.h"

Window*
newwindow(void)
{
	char buf[12];
	Window *w;

	w = emalloc(sizeof(Window));
	w->ctl = fsopen(acmefs, "new/ctl", ORDWR|OCEXEC);
	if(w->ctl == nil || fsread(w->ctl, buf, 12)!=12)
		error("can't open window ctl file: %r");

	w->id = atoi(buf);
	w->event = winopenfile(w, "event");
	w->addr = nil;	/* will be opened when needed */
	w->body = nil;
	w->data = nil;
	w->cevent = chancreate(sizeof(Event*), 0);
	w->ref = 1;
	return w;
}

void
winincref(Window *w)
{
	qlock(&w->lk);
	++w->ref;
	qunlock(&w->lk);
}

void
windecref(Window *w)
{
	qlock(&w->lk);
	if(--w->ref > 0){
		qunlock(&w->lk);
		return;
	}
	fsclose(w->event);
	chanfree(w->cevent);
	free(w);
}

void
winsetdump(Window *w, char *dir, char *cmd)
{
	if(dir != nil)
		ctlprint(w->ctl, "dumpdir %s\n", dir);
	if(cmd != nil)
		ctlprint(w->ctl, "dump %s\n", cmd);
}

void
wineventproc(void *v)
{
	Window *w;
	int i;

	w = v;
	for(i=0; ; i++){
		if(i >= NEVENT)
			i = 0;
		wingetevent(w, &w->e[i]);
		sendp(w->cevent, &w->e[i]);
	}
}

static CFid*
winopenfile1(Window *w, char *f, int m)
{
	char buf[64];
	CFid* fd;

	sprint(buf, "%d/%s", w->id, f);
	fd = fsopen(acmefs, buf, m|OCEXEC);
	if(fd == nil)
		error("can't open window file %s: %r", f);
	return fd;
}

CFid*
winopenfile(Window *w, char *f)
{
	return winopenfile1(w, f, ORDWR);
}

void
wintagwrite(Window *w, char *s, int n)
{
	CFid* fid;

	fid = winopenfile(w, "tag");
	if(fswrite(fid, s, n) != n)
		error("tag write: %r");
	fsclose(fid);
}

void
winname(Window *w, char *s)
{
	int len;
	char *ns, *sp;
	Rune r = L'␣';	/* visible space */

	len = 0;
	ns = emalloc(strlen(s)*runelen(r) + 1);
	for(sp = s; *sp != '\0'; sp++, len++){
		if(isspace(*sp)){
			len += runetochar(ns+len, &r)-1;
			continue;
		}
		*(ns+len) = *sp;
	}
	ctlprint(w->ctl, "name %s\n", ns);
	free(ns);
	return;
}

void
winopenbody(Window *w, int mode)
{
	char buf[256];
	CFid* fid;

	sprint(buf, "%d/body", w->id);
	fid = fsopen(acmefs, buf, mode|OCEXEC);
	w->body = fid;
	if(w->body == nil)
		error("can't open window body file: %r");
}

void
winclosebody(Window *w)
{
	if(w->body != nil){
		fsclose(w->body);
		w->body = nil;
	}
}

void
winwritebody(Window *w, char *s, int n)
{
	if(w->body == nil)
		winopenbody(w, OWRITE);
	if(fswrite(w->body, s, n) != n)
		error("write error to window: %r");
}

int
wingetec(Window *w)
{
	if(w->nbuf == 0){
		w->nbuf = fsread(w->event, w->buf, sizeof w->buf);
		if(w->nbuf <= 0){
			/* probably because window has exited, and only called by wineventproc, so just shut down */
			windecref(w);
			threadexits(nil);
		}
		w->bufp = w->buf;
	}
	w->nbuf--;
	return *w->bufp++;
}

int
wingeten(Window *w)
{
	int n, c;

	n = 0;
	while('0'<=(c=wingetec(w)) && c<='9')
		n = n*10+(c-'0');
	if(c != ' ')
		error("event number syntax");
	return n;
}

int
wingeter(Window *w, char *buf, int *nb)
{
	Rune r;
	int n;

	r = wingetec(w);
	buf[0] = r;
	n = 1;
	if(r >= Runeself) {
		while(!fullrune(buf, n))
			buf[n++] = wingetec(w);
		chartorune(&r, buf);
	}
	*nb = n;
	return r;
}

void
wingetevent(Window *w, Event *e)
{
	int i, nb;

	e->c1 = wingetec(w);
	e->c2 = wingetec(w);
	e->q0 = wingeten(w);
	e->q1 = wingeten(w);
	e->flag = wingeten(w);
	e->nr = wingeten(w);
	if(e->nr > EVENTSIZE)
		error("event string too long");
	e->nb = 0;
	for(i=0; i<e->nr; i++){
		e->r[i] = wingeter(w, e->b+e->nb, &nb);
		e->nb += nb;
	}
	e->r[e->nr] = 0;
	e->b[e->nb] = 0;
	if(wingetec(w) != '\n')
		error("event syntax error");
}

void
winwriteevent(Window *w, Event *e)
{
	fsprint(w->event, "%c%c%d %d\n", e->c1, e->c2, e->q0, e->q1);
}

void
winread(Window *w, uint q0, uint q1, char *data)
{
	int m, n, nr;
	char buf[256];

	if(w->addr == nil)
		w->addr = winopenfile(w, "addr");
	if(w->data == nil)
		w->data = winopenfile(w, "data");
	m = q0;
	while(m < q1){
		n = sprint(buf, "#%d", m);
		if(fswrite(w->addr, buf, n) != n)
			error("error writing addr: %r");
		n = fsread(w->data, buf, sizeof buf);
		if(n <= 0)
			error("reading data: %r");
		nr = utfnlen(buf, n);
		while(m+nr >q1){
			do; while(n>0 && (buf[--n]&0xC0)==0x80);
			--nr;
		}
		if(n == 0)
			break;
		memmove(data, buf, n);
		data += n;
		*data = 0;
		m += nr;
	}
}

void
windormant(Window *w)
{
	if(w->addr != nil){
		fsclose(w->addr);
		w->addr = nil;
	}
	if(w->body != nil){
		fsclose(w->body);
		w->body = nil;
	}
	if(w->data != nil){
		fsclose(w->data);
		w->data = nil;
	}
}


int
windel(Window *w, int sure)
{
	if(sure)
		fswrite(w->ctl, "delete\n", 7);
	else if(fswrite(w->ctl, "del\n", 4) != 4)
		return 0;
	/* event proc will die due to read error from event file */
	windormant(w);
	fsclose(w->ctl);
	w->ctl = nil;
	return 1;
}

void
winclean(Window *w)
{
	ctlprint(w->ctl, "clean\n");
}

int
winsetaddr(Window *w, char *addr, int errok)
{
	if(w->addr == nil)
		w->addr = winopenfile(w, "addr");
	if(fswrite(w->addr, addr, strlen(addr)) < 0){
		if(!errok)
			error("error writing addr(%s): %r", addr);
		return 0;
	}
	return 1;
}

int
winselect(Window *w, char *addr, int errok)
{
	if(winsetaddr(w, addr, errok)){
		ctlprint(w->ctl, "dot=addr\n");
		return 1;
	}
	return 0;
}

char*
winreadbody(Window *w, int *np)	/* can't use readfile because acme doesn't report the length */
{
	char *s;
	int m, na, n;

	if(w->body != nil)
		winclosebody(w);
	winopenbody(w, OREAD);
	s = nil;
	na = 0;
	n = 0;
	for(;;){
		if(na < n+512){
			na += 1024;
			s = realloc(s, na+1);
		}
		m = fsread(w->body, s+n, na-n);
		if(m <= 0)
			break;
		n += m;
	}
	s[n] = 0;
	winclosebody(w);
	*np = n;
	return s;
}

char*
winselection(Window *w)
{
	int m, n;
	char *buf;
	char tmp[256];
	CFid* fid;

	fid = winopenfile1(w, "rdsel", OREAD);
	if(fid == nil)
		error("can't open rdsel: %r");
	n = 0;
	buf = nil;
	for(;;){
		m = fsread(fid, tmp, sizeof tmp);
		if(m <= 0)
			break;
		buf = erealloc(buf, n+m+1);
		memmove(buf+n, tmp, m);
		n += m;
		buf[n] = '\0';
	}
	fsclose(fid);
	return buf;
}
