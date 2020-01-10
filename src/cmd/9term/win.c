#include <u.h>
#include <libc.h>
#include <thread.h>
#include <fcall.h>
#include <9pclient.h>
#include "term.h"

const char *termprog = "win";

#define	EVENTSIZE	256
#define	STACK	32768

typedef struct Event Event;
typedef struct Q Q;

struct Event
{
	int	c1;
	int	c2;
	int	q0;
	int	q1;
	int	flag;
	int	nb;
	int	nr;
	char	b[EVENTSIZE*UTFmax+1];
	Rune	r[EVENTSIZE+1];
};

Event blank = {
	'M',
	'X',
	0, 0, 0, 1, 1,
	{ ' ', 0 },
	{ ' ', 0 }
};

struct Q
{
	QLock	lk;
	int		p;
	int		k;
};

Q	q;

CFid *eventfd;
CFid *addrfd;
CFid *datafd;
CFid *ctlfd;
/* int bodyfd; */

char	*typing;
int	ntypeb;
int	ntyper;
int	ntypebreak;
int	debug;
int	rcfd;
int	cook = 1;
int	password;
int	israw(int);

char *name;

char **prog;
Channel *cwait;
int pid = -1;

int	label(char*, int);
void	error(char*, ...);
void	stdinproc(void*);
void	stdoutproc(void*);
void	type(Event*, int, CFid*, CFid*);
void	sende(Event*, int, CFid*, CFid*, CFid*, int);
char	*onestring(int, char**);
int	delete(Event*);
void	deltype(uint, uint);
void	sendbs(int, int);
void	runproc(void*);

int
fsfidprint(CFid *fid, char *fmt, ...)
{
	char buf[256];
	va_list arg;
	int n;

	va_start(arg, fmt);
	n = vsnprint(buf, sizeof buf, fmt, arg);
	va_end(arg);
	return fswrite(fid, buf, n);
}

void
usage(void)
{
	fprint(2, "usage: win cmd args...\n");
	threadexitsall("usage");
}

void
waitthread(void *v)
{
	recvp(cwait);
	threadexitsall(nil);
}

void
hangupnote(void *a, char *msg)
{
	if(strcmp(msg, "hangup") == 0 && pid != 0){
		postnote(PNGROUP, pid, "hangup");
		noted(NDFLT);
	}
	if(strstr(msg, "child")){
		char buf[128];
		int n;

		n = awaitnohang(buf, sizeof buf-1);
		if(n > 0){
			buf[n] = 0;
			if(atoi(buf) == pid)
				threadexitsall(0);
		}
		noted(NCONT);
	}
	noted(NDFLT);
}

void
threadmain(int argc, char **argv)
{
	int fd, id;
	char buf[256];
	char buf1[128];
	CFsys *fs;
	char *dump;

	dump = onestring(argc, argv);

	ARGBEGIN{
	case 'd':
		debug = 1;
		break;
	case 'n':
		name = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	prog = argv;

	if(name == nil){
		if(argc > 0)
			name = argv[0];
		else{
			name = sysname();
			if(name == nil)
				name = "gnot";
		}
	}

	/*
	 * notedisable("sys: write on closed pipe");
	 * not okay to disable the note, because that
	 * gets inherited by the subshell, so that something
	 * as simple as "yes | sed 10q" never exits.
	 * call notifyoff instead.  (is notedisable ever safe?)
	 */
	notifyoff("sys: write on closed pipe");

	noteenable("sys: child");
	notify(hangupnote);

	if((fs = nsmount("acme", "")) == 0)
		sysfatal("nsmount acme: %r");
	ctlfd = fsopen(fs, "new/ctl", ORDWR|OCEXEC);
	if(ctlfd == 0 || fsread(ctlfd, buf, 12) != 12)
		sysfatal("ctl: %r");
	id = atoi(buf);
	snprint(buf, sizeof buf, "%d", id);
	putenv("winid", buf);
	sprint(buf, "%d/tag", id);
	fd = fsopenfd(fs, buf, OWRITE|OCEXEC);
	write(fd, " Send", 1+4);
	close(fd);
	sprint(buf, "%d/event", id);
	eventfd = fsopen(fs, buf, ORDWR|OCEXEC);
	sprint(buf, "%d/addr", id);
	addrfd = fsopen(fs, buf, ORDWR|OCEXEC);
	sprint(buf, "%d/data", id);
	datafd = fsopen(fs, buf, ORDWR|OCEXEC);
	sprint(buf, "%d/body", id);
/*	bodyfd = fsopenfd(fs, buf, ORDWR|OCEXEC); */
	if(eventfd==nil || addrfd==nil || datafd==nil)
		sysfatal("data files: %r");
/*
	if(eventfd<0 || addrfd<0 || datafd<0 || bodyfd<0)
		sysfatal("data files: %r");
*/
	fsunmount(fs);

	cwait = threadwaitchan();
	threadcreate(waitthread, nil, STACK);
	pid = rcstart(argc, argv, &rcfd, nil);
	if(pid == -1)
		sysfatal("exec failed");

	getwd(buf1, sizeof buf1);
	sprint(buf, "name %s/-%s\n0\n", buf1, name);
	fswrite(ctlfd, buf, strlen(buf));
	sprint(buf, "dumpdir %s/\n", buf1);
	fswrite(ctlfd, buf, strlen(buf));
	sprint(buf, "dump %s\n", dump);
	fswrite(ctlfd, buf, strlen(buf));
	sprint(buf, "scroll");
	fswrite(ctlfd, buf, strlen(buf));

	updatewinsize(25, 80, 0, 0);
	proccreate(stdoutproc, nil, STACK);
	stdinproc(nil);
}

void
error(char *s, ...)
{
	va_list arg;

	if(s){
		va_start(arg, s);
		s = vsmprint(s, arg);
		va_end(arg);
		fprint(2, "win: %s: %r\n", s);
	}
	if(pid != -1)
		postnote(PNGROUP, pid, "hangup");
	threadexitsall(s);
}

char*
onestring(int argc, char **argv)
{
	char *p;
	int i, n;
	static char buf[1024];

	if(argc == 0)
		return "";
	p = buf;
	for(i=0; i<argc; i++){
		n = strlen(argv[i]);
		if(p+n+1 >= buf+sizeof buf)
			break;
		memmove(p, argv[i], n);
		p += n;
		*p++ = ' ';
	}
	p[-1] = 0;
	return buf;
}

int
getec(CFid *efd)
{
	static char buf[8192];
	static char *bufp;
	static int nbuf;

	if(nbuf == 0){
		nbuf = fsread(efd, buf, sizeof buf);
		if(nbuf <= 0)
			error(nil);
		bufp = buf;
	}
	--nbuf;
	return *bufp++;
}

int
geten(CFid *efd)
{
	int n, c;

	n = 0;
	while('0'<=(c=getec(efd)) && c<='9')
		n = n*10+(c-'0');
	if(c != ' ')
		error("event number syntax");
	return n;
}

int
geter(CFid *efd, char *buf, int *nb)
{
	Rune r;
	int n;

	r = getec(efd);
	buf[0] = r;
	n = 1;
	if(r < Runeself)
		goto Return;
	while(!fullrune(buf, n))
		buf[n++] = getec(efd);
	chartorune(&r, buf);
    Return:
	*nb = n;
	return r;
}

void
gete(CFid *efd, Event *e)
{
	int i, nb;

	e->c1 = getec(efd);
	e->c2 = getec(efd);
	e->q0 = geten(efd);
	e->q1 = geten(efd);
	e->flag = geten(efd);
	e->nr = geten(efd);
	if(e->nr > EVENTSIZE)
		error("event string too long");
	e->nb = 0;
	for(i=0; i<e->nr; i++){
		e->r[i] = geter(efd, e->b+e->nb, &nb);
		e->nb += nb;
	}
	e->r[e->nr] = 0;
	e->b[e->nb] = 0;
	if(getec(efd) != '\n')
		error("event syntax 2");
}

int
nrunes(char *s, int nb)
{
	int i, n;
	Rune r;

	n = 0;
	for(i=0; i<nb; n++)
		i += chartorune(&r, s+i);
	return n;
}

void
stdinproc(void *v)
{
	CFid *cfd = ctlfd;
	CFid *efd = eventfd;
	CFid *dfd = datafd;
	CFid *afd = addrfd;
	int fd0 = rcfd;
	Event e, e2, e3, e4;
	int n;

	USED(v);

	for(;;){
		if(debug)
			fprint(2, "typing[%d,%d)\n", q.p, q.p+ntyper);
		gete(efd, &e);
		if(debug)
			fprint(2, "msg %c%c q[%d,%d)... ", e.c1, e.c2, e.q0, e.q1);
		qlock(&q.lk);
		switch(e.c1){
		default:
		Unknown:
			print("unknown message %c%c\n", e.c1, e.c2);
			break;

		case 'E':	/* write to body or tag; can't affect us */
			switch(e.c2){
			case 'I':
			case 'D':		/* body */
				if(debug)
					fprint(2, "shift typing %d... ", e.q1-e.q0);
				q.p += e.q1-e.q0;
				break;

			case 'i':
			case 'd':		/* tag */
				break;

			default:
				goto Unknown;
			}
			break;

		case 'F':	/* generated by our actions; ignore */
			break;

		case 'K':
		case 'M':
			switch(e.c2){
			case 'I':
				if(e.nr == 1 && e.r[0] == 0x7F) {
					char buf[1];
					fsprint(addrfd, "#%ud,#%ud", e.q0, e.q1);
					fswrite(datafd, "", 0);
					buf[0] = 0x7F;
					write(fd0, buf, 1);
					break;
				}
				if(e.q0 < q.p){
					if(debug)
						fprint(2, "shift typing %d... ", e.q1-e.q0);
					q.p += e.q1-e.q0;
				}
				else if(e.q0 <= q.p+ntyper){
					if(debug)
						fprint(2, "type... ");
					type(&e, fd0, afd, dfd);
				}
				break;

			case 'D':
				n = delete(&e);
				q.p -= n;
				if(israw(fd0) && e.q1 >= q.p+n)
					sendbs(fd0, n);
				break;

			case 'x':
			case 'X':
				if(e.flag & 2)
					gete(efd, &e2);
				if(e.flag & 8){
					gete(efd, &e3);
					gete(efd, &e4);
				}
				if(e.flag&1 || (e.c2=='x' && e.nr==0 && e2.nr==0)){
					/* send it straight back */
					fsfidprint(efd, "%c%c%d %d\n", e.c1, e.c2, e.q0, e.q1);
					break;
				}
				if(e.q0==e.q1 && (e.flag&2)){
					e2.flag = e.flag;
					e = e2;
				}
				char buf[100];
				snprint(buf, sizeof buf, "%.*S", e.nr, e.r);
				if(cistrcmp(buf, "cook") == 0) {
					cook = 1;
					break;
				}
				if(cistrcmp(buf, "nocook") == 0) {
					cook = 0;
					break;
				}
				if(e.flag & 8){
					if(e.q1 != e.q0){
						sende(&e, fd0, cfd, afd, dfd, 0);
						sende(&blank, fd0, cfd, afd, dfd, 0);
					}
					sende(&e3, fd0, cfd, afd, dfd, 1);
				}else	 if(e.q1 != e.q0)
					sende(&e, fd0, cfd, afd, dfd, 1);
				break;

			case 'l':
			case 'L':
				/* just send it back */
				if(e.flag & 2)
					gete(efd, &e2);
				fsfidprint(efd, "%c%c%d %d\n", e.c1, e.c2, e.q0, e.q1);
				break;

			case 'd':
			case 'i':
				break;

			default:
				goto Unknown;
			}
		}
		qunlock(&q.lk);
	}
}

int
dropcr(char *p, int n)
{
	int i;
	char *w, *r, *q;

	r = p;
	w = p;
	for(i=0; i<n; i++) {
		switch(*r) {
		case '\b':
			if(w > p)
				w--;
			break;
		case '\r':
			while(i<n-1 && *(r+1) == '\r') {
				r++;
				i++;
			}
			if(i<n && *(r+1) != '\n') {
				q = r;
				while(q>p && *(q-1) != '\n')
					q--;
				if(q > p) {
					w = q;
					break;
				}
			}
			*w++ = '\n';
			break;
		default:
			*w++ = *r;
			break;
		}
		r++;
	}
	return w-p;
}

void
stdoutproc(void *v)
{
	int fd1 = rcfd;
	CFid *afd = addrfd;
	CFid *dfd = datafd;
	int n, m, w, npart;
	char *buf, *s, *t;
	Rune r;
	char x[16], hold[UTFmax];

	USED(v);
	buf = malloc(8192+UTFmax+1);
	npart = 0;
	for(;;){
		/* Let typing have a go -- maybe there's a rubout waiting. */
		yield();
		n = read(fd1, buf+npart, 8192);
		if(n <= 0)
			error(nil);

		n = echocancel(buf+npart, n);
		if(n == 0)
			continue;

		n = dropcrnl(buf+npart, n);
		if(n == 0)
			continue;

		n = dropcr(buf+npart, n);
		if(n == 0)
			continue;

		/* squash NULs */
		s = memchr(buf+npart, 0, n);
		if(s){
			for(t=s; s<buf+npart+n; s++)
				if(*t = *s)	/* assign = */
					t++;
			n = t-(buf+npart);
		}

		n += npart;

		/* hold on to final partial rune */
		npart = 0;
		while(n>0 && (buf[n-1]&0xC0)){
			--n;
			npart++;
			if((buf[n]&0xC0)!=0x80){
				if(fullrune(buf+n, npart)){
					w = chartorune(&r, buf+n);
					n += w;
					npart -= w;
				}
				break;
			}
		}
		if(n > 0){
			memmove(hold, buf+n, npart);
			buf[n] = 0;
			n = label(buf, n);
			buf[n] = 0;

			// clumsy but effective: notice password
			// prompts so we can disable echo.
			password = 0;
			if(cistrstr(buf, "password") || cistrstr(buf, "passphrase")) {
				int i;

				i = n;
				while(i > 0 && buf[i-1] == ' ')
					i--;
				password = i > 0 && buf[i-1] == ':';
			}

			qlock(&q.lk);
			m = sprint(x, "#%d", q.p);
			if(fswrite(afd, x, m) != m){
				fprint(2, "stdout writing address %s: %r; resetting\n", x);
				if(fswrite(afd, "$", 1) < 0)
					fprint(2, "reset: %r\n");
				fsseek(afd, 0, 0);
				m = fsread(afd, x, sizeof x-1);
				if(m >= 0){
					x[m] = 0;
					q.p = atoi(x);
				}
			}
			if(fswrite(dfd, buf, n) != n)
				error("stdout writing body");
			/* Make sure acme scrolls to the end of the above write. */
			if(fswrite(dfd, nil, 0) != 0)
				error("stdout flushing body");
			q.p += nrunes(buf, n);
			qunlock(&q.lk);
			memmove(buf, hold, npart);
		}
	}
}

char wdir[512];
int
label(char *sr, int n)
{
	char *sl, *el, *er, *r, *p;

	er = sr+n;
	for(r=er-1; r>=sr; r--)
		if(*r == '\007')
			break;
	if(r < sr)
		return n;

	el = r+1;
	if(el-sr > sizeof wdir - strlen(name) - 20)
		sr = el - (sizeof wdir - strlen(name) - 20);
	for(sl=el-3; sl>=sr; sl--)
		if(sl[0]=='\033' && sl[1]==']' && sl[2]==';')
			break;
	if(sl < sr)
		return n;

	*r = 0;
	if(strcmp(sl+3, "*9term-hold+") != 0) {
		/*
		 * add /-sysname if not present
		 */
		snprint(wdir, sizeof wdir, "name %s", sl+3);
		p = strrchr(wdir, '/');
		if(p==nil || *(p+1) != '-'){
			p = wdir+strlen(wdir);
			if(*(p-1) != '/')
				*p++ = '/';
			*p++ = '-';
			strcpy(p, name);
		}
		strcat(wdir, "\n0\n");
		fswrite(ctlfd, wdir, strlen(wdir));
	}

	memmove(sl, el, er-el);
	n -= (el-sl);
	return n;
}

int
delete(Event *e)
{
	uint q0, q1;
	int deltap;

	q0 = e->q0;
	q1 = e->q1;
	if(q1 <= q.p)
		return e->q1-e->q0;
	if(q0 >= q.p+ntyper)
		return 0;
	deltap = 0;
	if(q0 < q.p){
		deltap = q.p-q0;
		q0 = 0;
	}else
		q0 -= q.p;
	if(q1 > q.p+ntyper)
		q1 = ntyper;
	else
		q1 -= q.p;
	deltype(q0, q1);
	return deltap;
}

void
addtype(int c, uint p0, char *b, int nb, int nr)
{
	int i, w;
	Rune r;
	uint p;
	char *b0;

	for(i=0; i<nb; i+=w){
		w = chartorune(&r, b+i);
		if((r==0x7F||r==3) && c=='K'){
			write(rcfd, "\x7F", 1);
			/* toss all typing */
			q.p += ntyper+nr;
			ntypebreak = 0;
			ntypeb = 0;
			ntyper = 0;
			/* buglet:  more than one delete ignored */
			return;
		}
		if(r=='\n' || r==0x04)
			ntypebreak++;
	}
	typing = realloc(typing, ntypeb+nb);
	if(typing == nil)
		error("realloc");
	if(p0 == ntyper)
		memmove(typing+ntypeb, b, nb);
	else{
		b0 = typing;
		for(p=0; p<p0 && b0<typing+ntypeb; p++){
			w = chartorune(&r, b0+i);
			b0 += w;
		}
		if(p != p0)
			error("typing: findrune");
		memmove(b0+nb, b0, (typing+ntypeb)-b0);
		memmove(b0, b, nb);
	}
	ntypeb += nb;
	ntyper += nr;
}

int
israw(int fd0)
{
	return (!cook || password) && !isecho(fd0);
}

void
sendtype(int fd0)
{
	int i, n, nr, raw;

	raw = israw(fd0);
	while(ntypebreak || (raw && ntypeb > 0)){
		for(i=0; i<ntypeb; i++)
			if(typing[i]=='\n' || typing[i]==0x04 || (i==ntypeb-1 && raw)){
				if((typing[i] == '\n' || typing[i] == 0x04) && ntypebreak > 0)
					ntypebreak--;
				n = i+1;
				i++;
				if(!raw)
					echoed(typing, n);
				if(write(fd0, typing, n) != n)
					error("sending to program");
				nr = nrunes(typing, i);
				q.p += nr;
				ntyper -= nr;
				ntypeb -= i;
				memmove(typing, typing+i, ntypeb);
				goto cont2;
			}
		print("no breakchar\n");
		ntypebreak = 0;
cont2:;
	}
}

void
sendbs(int fd0, int n)
{
	char buf[128];
	int m;

	memset(buf, 0x08, sizeof buf);
	while(n > 0) {
		m = sizeof buf;
		if(m > n)
			m = n;
		n -= m;
		write(fd0, buf, m);
	}
}

void
deltype(uint p0, uint p1)
{
	int w;
	uint p, b0, b1;
	Rune r;

	/* advance to p0 */
	b0 = 0;
	for(p=0; p<p0 && b0<ntypeb; p++){
		w = chartorune(&r, typing+b0);
		b0 += w;
	}
	if(p != p0)
		error("deltype 1");
	/* advance to p1 */
	b1 = b0;
	for(; p<p1 && b1<ntypeb; p++){
		w = chartorune(&r, typing+b1);
		b1 += w;
		if(r=='\n' || r==0x04)
			ntypebreak--;
	}
	if(p != p1)
		error("deltype 2");
	memmove(typing+b0, typing+b1, ntypeb-b1);
	ntypeb -= b1-b0;
	ntyper -= p1-p0;
}

void
type(Event *e, int fd0, CFid *afd, CFid *dfd)
{
	int m, n, nr;
	char buf[128];

	if(e->nr > 0)
		addtype(e->c1, e->q0-q.p, e->b, e->nb, e->nr);
	else{
		m = e->q0;
		while(m < e->q1){
			n = sprint(buf, "#%d", m);
			fswrite(afd, buf, n);
			n = fsread(dfd, buf, sizeof buf);
			nr = nrunes(buf, n);
			while(m+nr > e->q1){
				do; while(n>0 && (buf[--n]&0xC0)==0x80);
				--nr;
			}
			if(n == 0)
				break;
			addtype(e->c1, m-q.p, buf, n, nr);
			m += nr;
		}
	}
	if(israw(fd0)) {
		n = sprint(buf, "#%d,#%d", e->q0, e->q1);
		fswrite(afd, buf, n);
		fswrite(dfd, "", 0);
		q.p -= e->q1 - e->q0;
	}
	sendtype(fd0);
	if(e->nb > 0 && e->b[e->nb-1] == '\n')
		cook = 1;
}

void
sende(Event *e, int fd0, CFid *cfd, CFid *afd, CFid *dfd, int donl)
{
	int l, m, n, nr, lastc, end;
	char abuf[16], buf[128];

	end = q.p+ntyper;
	l = sprint(abuf, "#%d", end);
	fswrite(afd, abuf, l);
	if(e->nr > 0){
		fswrite(dfd, e->b, e->nb);
		addtype(e->c1, ntyper, e->b, e->nb, e->nr);
		lastc = e->r[e->nr-1];
	}else{
		m = e->q0;
		lastc = 0;
		while(m < e->q1){
			n = sprint(buf, "#%d", m);
			fswrite(afd, buf, n);
			n = fsread(dfd, buf, sizeof buf);
			nr = nrunes(buf, n);
			while(m+nr > e->q1){
				do; while(n>0 && (buf[--n]&0xC0)==0x80);
				--nr;
			}
			if(n == 0)
				break;
			l = sprint(abuf, "#%d", end);
			fswrite(afd, abuf, l);
			fswrite(dfd, buf, n);
			addtype(e->c1, ntyper, buf, n, nr);
			lastc = buf[n-1];
			m += nr;
			end += nr;
		}
	}
	if(donl && lastc!='\n'){
		fswrite(dfd, "\n", 1);
		addtype(e->c1, ntyper, "\n", 1, 1);
	}
	fswrite(cfd, "dot=addr", 8);
	sendtype(fd0);
}
