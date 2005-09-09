#include <u.h>
#include <libc.h>
#include <thread.h>
#include <9pclient.h>
#include "acme.h"

extern int *xxx;
static CFsys *acmefs;
Win *windows;
static Win *last;

void
mountacme(void)
{
	if(acmefs == nil){
		acmefs = nsmount("acme", nil);
		if(acmefs == nil)
			sysfatal("cannot mount acme: %r");
	}
}

Win*
newwin(void)
{
	Win *w;
	CFid *fid;
	char buf[100];
	int id, n;

	mountacme();
	fid = fsopen(acmefs, "new/ctl", ORDWR);
	if(fid == nil)
		sysfatal("open new/ctl: %r");
	n = fsread(fid, buf, sizeof buf-1);
	if(n <= 0)
		sysfatal("read new/ctl: %r");
	buf[n] = 0;
	id = atoi(buf);
	if(id == 0)
		sysfatal("read new/ctl: malformed message: %s", buf);

	w = emalloc(sizeof *w);
	w->id = id;
	w->ctl = fid;
	w->next = nil;
	w->prev = last;
	if(last)
		last->next = w;
	else
		windows = w;
	last = w;
	return w;
}

void
winclosefiles(Win *w)
{
	if(w->ctl){
		fsclose(w->ctl);
		w->ctl = nil;
	}
	if(w->body){
		fsclose(w->body);
		w->body = nil;
	}
	if(w->addr){
		fsclose(w->addr);
		w->addr = nil;
	}
	if(w->tag){
		fsclose(w->tag);
		w->tag = nil;
	}
	if(w->event){
		fsclose(w->event);
		w->event = nil;
	}
	if(w->data){
		fsclose(w->data);
		w->data = nil;
	}
	if(w->xdata){
		fsclose(w->xdata);
		w->xdata = nil;
	}
}

void
winfree(Win *w)
{
	winclosefiles(w);
	if(w->c){
		chanfree(w->c);
		w->c = nil;
	}
	if(w->next)
		w->next->prev = w->prev;
	else
		last = w->prev;
	if(w->prev)
		w->prev->next = w->next;
	else
		windows = w->next;
	free(w);
}

void
windeleteall(void)
{
	Win *w, *next;

	for(w=windows; w; w=next){
		next = w->next;
		winctl(w, "delete");
	}
}

static CFid*
wfid(Win *w, char *name)
{
	char buf[100];
	CFid **fid;

	if(strcmp(name, "ctl") == 0)
		fid = &w->ctl;
	else if(strcmp(name, "body") == 0)
		fid = &w->body;
	else if(strcmp(name, "addr") == 0)
		fid = &w->addr;
	else if(strcmp(name, "tag") == 0)
		fid = &w->tag;
	else if(strcmp(name, "event") == 0)
		fid = &w->event;
	else if(strcmp(name, "data") == 0)
		fid = &w->data;
	else if(strcmp(name, "xdata") == 0)
		fid = &w->xdata;
	else{
		fid = 0;
		sysfatal("bad window file name %s", name);
	}

	if(*fid == nil){
		snprint(buf, sizeof buf, "acme/%d/%s", w->id, name);
		*fid = fsopen(acmefs, buf, ORDWR);
		if(*fid == nil)
			sysfatal("open %s: %r", buf);
	}
	return *fid;
}

int
winopenfd(Win *w, char *name, int mode)
{
	char buf[100];
	
	snprint(buf, sizeof buf, "%d/%s", w->id, name);
	return fsopenfd(acmefs, buf, mode);
}

int
winctl(Win *w, char *fmt, ...)
{
	char *s;
	va_list arg;
	CFid *fid;
	int n;

	va_start(arg, fmt);
	s = evsmprint(fmt, arg);
	va_end(arg);

	fid = wfid(w, "ctl");
	n = fspwrite(fid, s, strlen(s), 0);
	free(s);
	return n;
}

int
winname(Win *w, char *fmt, ...)
{
	char *s;
	va_list arg;
	int n;

	va_start(arg, fmt);
	s = evsmprint(fmt, arg);
	va_end(arg);

	n = winctl(w, "name %s\n", s);
	free(s);
	return n;
}

int
winprint(Win *w, char *name, char *fmt, ...)
{
	char *s;
	va_list arg;
	int n;

	va_start(arg, fmt);
	s = evsmprint(fmt, arg);
	va_end(arg);

	n = fswrite(wfid(w, name), s, strlen(s));
	free(s);
	return n;
}

int
winaddr(Win *w, char *fmt, ...)
{
	char *s;
	va_list arg;
	int n;

	va_start(arg, fmt);
	s = evsmprint(fmt, arg);
	va_end(arg);

	n = fswrite(wfid(w, "addr"), s, strlen(s));
	free(s);
	return n;
}

int
winreadaddr(Win *w, uint *q1)
{
	char buf[40], *p;
	uint q0;
	int n;
	
	n = fspread(wfid(w, "addr"), buf, sizeof buf-1, 0);
	if(n <= 0)
		return -1;
	buf[n] = 0;
	q0 = strtoul(buf, &p, 10);
	if(q1)
		*q1 = strtoul(p, nil, 10);
	return q0;
}

int
winread(Win *w, char *file, void *a, int n)
{
	return fspread(wfid(w, file), a, n, 0);
}

int
winwrite(Win *w, char *file, void *a, int n)
{
	return fswrite(wfid(w, file), a, n);
}

char*
fsreadm(CFid *fid)
{
	char *buf;
	int n, tot, m;
	
	m = 128;
	buf = emalloc(m+1);
	tot = 0;
	while((n = fspread(fid, buf+tot, m-tot, tot)) > 0){
		tot += n;
		if(tot >= m){
			m += 128;
			buf = erealloc(buf, m+1);
		}
	}
	if(n < 0){
		free(buf);
		return nil;
	}
	buf[tot] = 0;
	return buf;
}

char*
winmread(Win *w, char *file)
{
	return fsreadm(wfid(w, file));
}

char*
winindex(void)
{
	CFid *fid;
	char *s;
	
	mountacme();
	if((fid = fsopen(acmefs, "index", OREAD)) == nil)
		return nil;
	s = fsreadm(fid);
	fsclose(fid);
	return s;
}

int
winseek(Win *w, char *file, int n, int off)
{
	return fsseek(wfid(w, file), n, off);
}

int
winwriteevent(Win *w, Event *e)
{
	char buf[100];

	snprint(buf, sizeof buf, "%c%c%d %d \n", e->c1, e->c2, e->q0, e->q1);
	return fswrite(wfid(w, "event"), buf, strlen(buf));
}

int
windel(Win *w, int sure)
{
	return winctl(w, sure ? "delete" : "del");
}

int
winfd(Win *w, char *name, int mode)
{
	char buf[100];

	snprint(buf, sizeof buf, "acme/%d/%s", w->id, name);
	return fsopenfd(acmefs, buf, mode);
}

static void
error(Win *w, char *msg)
{
	if(msg == nil)
		longjmp(w->jmp, 1);
	fprint(2, "%s: win%d: %s\n", argv0, w->id, msg);
	longjmp(w->jmp, 2);
}

static int
getec(Win *w, CFid *efd)
{
	if(w->nbuf <= 0){
		w->nbuf = fsread(efd, w->buf, sizeof w->buf);
		if(w->nbuf <= 0)
			error(w, nil);
		w->bufp = w->buf;
	}
	--w->nbuf;
	return *w->bufp++;
}

static int
geten(Win *w, CFid *efd)
{
	int n, c;

	n = 0;
	while('0'<=(c=getec(w,efd)) && c<='9')
		n = n*10+(c-'0');
	if(c != ' ')
		error(w, "event number syntax");
	return n;
}

static int
geter(Win *w, CFid *efd, char *buf, int *nb)
{
	Rune r;
	int n;

	r = getec(w, efd);
	buf[0] = r;
	n = 1;
	if(r < Runeself)
		goto Return;
	while(!fullrune(buf, n))
		buf[n++] = getec(w, efd);
	chartorune(&r, buf);
    Return:
	*nb = n;
	return r;
}

static void
gete(Win *w, CFid *efd, Event *e)
{
	int i, nb;

	e->c1 = getec(w, efd);
	e->c2 = getec(w, efd);
	e->q0 = geten(w, efd);
	e->q1 = geten(w, efd);
	e->flag = geten(w, efd);
	e->nr = geten(w, efd);
	if(e->nr > EVENTSIZE)
		error(w, "event string too long");
	e->nb = 0;
	for(i=0; i<e->nr; i++){
		/* e->r[i] = */ geter(w, efd, e->text+e->nb, &nb);
		e->nb += nb;
	}
/* 	e->r[e->nr] = 0; */
	e->text[e->nb] = 0;
	if(getec(w, efd) != '\n')
		error(w, "event syntax 2");
}

int
winreadevent(Win *w, Event *e)
{
	CFid *efd;
	int r;

	if((r = setjmp(w->jmp)) != 0){
		if(r == 1)
			return 0;
		return -1;
	}
	efd = wfid(w, "event");
	gete(w, efd, e);
	e->oq0 = e->q0;
	e->oq1 = e->q1;

	/* expansion */
	if(e->flag&2){
		gete(w, efd, &w->e2);
		if(e->q0==e->q1){
			w->e2.oq0 = e->q0;
			w->e2.oq1 = e->q1;
			w->e2.flag = e->flag;
			*e = w->e2;
		}
	}

	/* chorded argument */
	if(e->flag&8){
		gete(w, efd, &w->e3);	/* arg */
		gete(w, efd, &w->e4);	/* location */
		strcpy(e->arg, w->e3.text);
		strcpy(e->loc, w->e4.text);
	}

	return 1;
}

int
eventfmt(Fmt *fmt)
{
	Event *e;

	e = va_arg(fmt->args, Event*);
	return fmtprint(fmt, "%c%c %d %d %d %d %q", e->c1, e->c2, e->q0, e->q1, e->flag, e->nr, e->text);
}

void*
emalloc(uint n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

void*
erealloc(void *v, uint n)
{
	v = realloc(v, n);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

char*
estrdup(char *s)
{
	if(s == nil)
		return nil;
	s = strdup(s);
	if(s == nil)
		sysfatal("out of memory");
	return s;
}

char*
evsmprint(char *s, va_list v)
{
	s = vsmprint(s, v);
	if(s == nil)
		sysfatal("out of memory");
	return s;
}

int
pipewinto(Win *w, char *name, int errto, char *cmd, ...)
{
	va_list arg;
	char *p;
	int fd[3], pid;

	va_start(arg, cmd);
	p = evsmprint(cmd, arg);
	va_end(arg);
	fd[0] = winfd(w, name, OREAD);
	fd[1] = dup(errto, -1);
	fd[2] = dup(errto, -1);
	pid = threadspawnl(fd, "rc", "rc", "-c", p, 0);
	free(p);
	return pid;
}

int
pipetowin(Win *w, char *name, int errto, char *cmd, ...)
{
	va_list arg;
	char *p;
	int fd[3], pid, pfd[2];
	char buf[1024];
	int n;

	/*
	 * cannot use winfd here because of buffering caused
	 * by pipe.  program might exit before final write to acme
	 * happens.  so we might return before the final write.
	 *
	 * to avoid this, we tend the pipe ourselves.
	 */
	if(pipe(pfd) < 0)
		sysfatal("pipe: %r");
	va_start(arg, cmd);
	p = evsmprint(cmd, arg);
	va_end(arg);
	fd[0] = open("/dev/null", OREAD);
	fd[1] = pfd[1];
	if(errto == 0)
		fd[2] = dup(fd[1], -1);
	else
		fd[2] = dup(errto, -1);
	pid = threadspawnl(fd, "rc", "rc", "-c", p, 0);
	free(p);
	while((n = read(pfd[0], buf, sizeof buf)) > 0)
		winwrite(w, name, buf, n);
	close(pfd[0]);
	return pid;
}

char*
sysrun(int errto, char *fmt, ...)
{
	static char buf[1024];
	char *cmd;
	va_list arg;
	int n, fd[3], p[2], tot, pid;

#undef pipe
	if(pipe(p) < 0)
		sysfatal("pipe: %r");
	fd[0] = open("/dev/null", OREAD);
	fd[1] = p[1];
	if(errto == 0)
		fd[2] = dup(fd[1], -1);
	else
		fd[2] = dup(errto, -1);

	va_start(arg, fmt);
	cmd = evsmprint(fmt, arg);
	va_end(arg);
	pid = threadspawnl(fd, "rc", "rc", "-c", cmd, 0);

	tot = 0;
	while((n = read(p[0], buf+tot, sizeof buf-tot)) > 0)
		tot += n;
	close(p[0]);
	twait(pid);
	if(n < 0)
		return nil;
	free(cmd);
	if(tot == sizeof buf)
		tot--;
	buf[tot] = 0;
	while(tot > 0 && isspace((uchar)buf[tot-1]))
		tot--;
	buf[tot] = 0;
	if(tot == 0){
		werrstr("no output");
		return nil;
	}
	return estrdup(buf);
}

static void
eventreader(void *v)
{
	Event e[2];
	Win *w;
	int i;
	
	w = v;
	i = 0;
	for(;;){
		if(winreadevent(w, &e[i]) <= 0)
			break;
		sendp(w->c, &e[i]);
		i = 1-i;	/* toggle */
	}
	sendp(w->c, nil);
	threadexits(nil);
}

Channel*
wineventchan(Win *w)
{
	if(w->c == nil){
		w->c = chancreate(sizeof(Event*), 0);
		threadcreate(eventreader, w, 32*1024);
	}
	return w->c;
}

char*
wingetname(Win *w)
{
	int n;
	char *p;
	
	n = winread(w, "tag", w->name, sizeof w->name-1);
	if(n <= 0)
		return nil;
	w->name[n] = 0;
	p = strchr(w->name, ' ');
	if(p)
		*p = 0;
	return w->name;
}

