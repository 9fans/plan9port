#include <u.h>
#include <signal.h>
#include <libc.h>
#include <ctype.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include <plumb.h>
#include <complete.h>
#define Extern
#include "dat.h"
#include "fns.h"
#include "term.h"

const char *termprog = "9term";
int use9wm;
int mainpid;
int mousepid;
int plumbfd;
int rcpid;
int rcfd;
int sfd;
Window *w;
char *fontname;

void derror(Display*, char*);
void	mousethread(void*);
void	keyboardthread(void*);
void winclosethread(void*);
void deletethread(void*);
void rcoutputproc(void*);
void	rcinputproc(void*);
void hangupnote(void*, char*);
void resizethread(void*);
void	servedevtext(void);

int errorshouldabort = 0;
int cooked;

void
usage(void)
{
	fprint(2, "usage: 9term [-s] [-f font] [-W winsize] [cmd ...]\n");
	threadexitsall("usage");
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char *argv[])
{
	char *p;

	rfork(RFNOTEG);
	font = nil;
	_wantfocuschanges = 1;
	mainpid = getpid();
	messagesize = 8192;

	ARGBEGIN{
	default:
		usage();
	case 'l':
		loginshell = TRUE;
		break;
	case 'f':
		fontname = EARGF(usage());
		break;
	case 's':
		scrolling = TRUE;
		break;
	case 'c':
		cooked = TRUE;
		break;
	case 'w':	/* started from rio or 9wm */
		use9wm = TRUE;
		break;
	case 'W':
		winsize = EARGF(usage());
		break;
	}ARGEND

	if(fontname)
		putenv("font", fontname);

	p = getenv("tabstop");
	if(p == 0)
		p = getenv("TABSTOP");
	if(p && maxtab <= 0)
		maxtab = strtoul(p, 0, 0);
	if(maxtab <= 0)
		maxtab = 4;
	free(p);

	startdir = ".";

	if(initdraw(derror, fontname, "9term") < 0)
		sysfatal("initdraw: %r");

	notify(hangupnote);
	noteenable("sys: child");

	mousectl = initmouse(nil, screen);
	if(mousectl == nil)
		error("cannot find mouse");
	keyboardctl = initkeyboard(nil);
	if(keyboardctl == nil)
		error("cannot find keyboard");
	mouse = &mousectl->m;

	winclosechan = chancreate(sizeof(Window*), 0);
	deletechan = chancreate(sizeof(char*), 0);

	timerinit();
	servedevtext();
	rcpid = rcstart(argc, argv, &rcfd, &sfd);
	w = new(screen, FALSE, scrolling, rcpid, ".", nil, nil);

	threadcreate(keyboardthread, nil, STACK);
	threadcreate(mousethread, nil, STACK);
	threadcreate(resizethread, nil, STACK);

	proccreate(rcoutputproc, nil, STACK);
	proccreate(rcinputproc, nil, STACK);
}

void
derror(Display *d, char *errorstr)
{
	USED(d);
	error(errorstr);
}

void
hangupnote(void *a, char *msg)
{
	if(getpid() != mainpid)
		noted(NDFLT);
	if(strcmp(msg, "hangup") == 0){
		postnote(PNPROC, rcpid, "hangup");
		noted(NDFLT);
	}
	if(strstr(msg, "child")){
		char buf[128];
		int n;

		n = awaitnohang(buf, sizeof buf-1);
		if(n > 0){
			buf[n] = 0;
			if(atoi(buf) == rcpid)
				threadexitsall(0);
		}
		noted(NCONT);
	}
	noted(NDFLT);
}

void
keyboardthread(void *v)
{
	Rune buf[2][20], *rp;
	int i, n;

	USED(v);
	threadsetname("keyboardthread");
	n = 0;
	for(;;){
		rp = buf[n];
		n = 1-n;
		recv(keyboardctl->c, rp);
		for(i=1; i<nelem(buf[0])-1; i++)
			if(nbrecv(keyboardctl->c, rp+i) <= 0)
				break;
		rp[i] = L'\0';
		sendp(w->ck, rp);
	}
}

void
resizethread(void *v)
{
	Point p;

	USED(v);

	for(;;){
		p = stringsize(display->defaultfont, "0");
		if(p.x && p.y)
			updatewinsize(Dy(screen->r)/p.y, (Dx(screen->r)-Scrollwid-2)/p.x,
				Dx(screen->r), Dy(screen->r));
		wresize(w, screen, 0);
		flushimage(display, 1);
		if(recv(mousectl->resizec, nil) != 1)
			break;
		if(getwindow(display, Refnone) < 0)
			sysfatal("can't reattach to window");
	}
}

void
mousethread(void *v)
{
	int sending;
	Mouse tmp;

	USED(v);

	sending = FALSE;
	threadsetname("mousethread");
	while(readmouse(mousectl) >= 0){
		if(sending){
		Send:
			/* send to window */
			if(mouse->buttons == 0)
				sending = FALSE;
			else
				wsetcursor(w, 0);
			tmp = mousectl->m;
			send(w->mc.c, &tmp);
			continue;
		}
		if((mouse->buttons&(1|8|16)) || ptinrect(mouse->xy, w->scrollr)){
			sending = TRUE;
			goto Send;
		}else if(mouse->buttons&2)
			button2menu(w);
		else
			bouncemouse(mouse);
	}
}

void
wborder(Window *w, int type)
{
}

Window*
wpointto(Point pt)
{
	return w;
}

Window*
new(Image *i, int hideit, int scrollit, int pid, char *dir, char *cmd, char **argv)
{
	Window *w;
	Mousectl *mc;
	Channel *cm, *ck, *cctl;

	if(i == nil)
		return nil;
	cm = chancreate(sizeof(Mouse), 0);
	ck = chancreate(sizeof(Rune*), 0);
	cctl = chancreate(sizeof(Wctlmesg), 4);
	if(cm==nil || ck==nil || cctl==nil)
		error("new: channel alloc failed");
	mc = emalloc(sizeof(Mousectl));
	*mc = *mousectl;
/*	mc->image = i; */
	mc->c = cm;
	w = wmk(i, mc, ck, cctl, scrollit);
	free(mc);	/* wmk copies *mc */
	window = erealloc(window, ++nwindow*sizeof(Window*));
	window[nwindow-1] = w;
	if(hideit){
		hidden[nhidden++] = w;
		w->screenr = ZR;
	}
	threadcreate(winctl, w, STACK);
	if(!hideit)
		wcurrent(w);
	flushimage(display, 1);
	wsetpid(w, pid, 1);
	wsetname(w);
	if(dir)
		w->dir = estrdup(dir);
	return w;
}

/*
 * Button 2 menu.  Extra entry for always cook
 */

enum
{
	Cut,
	Paste,
	Snarf,
	Plumb,
	Look,
	Send,
	Scroll,
	Cook
};

char		*menu2str[] = {
	"cut",
	"paste",
	"snarf",
	"plumb",
	"look",
	"send",
	"cook",
	"scroll",
	nil
};


Menu menu2 =
{
	menu2str
};

Rune newline[] = { '\n' };

void
button2menu(Window *w)
{
	if(w->deleted)
		return;
	incref(&w->ref);
	if(w->scrolling)
		menu2str[Scroll] = "noscroll";
	else
		menu2str[Scroll] = "scroll";
	if(cooked)
		menu2str[Cook] = "nocook";
	else
		menu2str[Cook] = "cook";

	switch(menuhit(2, mousectl, &menu2, wscreen)){
	case Cut:
		wsnarf(w);
		wcut(w);
		wscrdraw(w);
		break;

	case Snarf:
		wsnarf(w);
		break;

	case Paste:
		riogetsnarf();
		wpaste(w);
		wscrdraw(w);
		break;

	case Plumb:
		wplumb(w);
		break;

	case Look:
		wlook(w);
		break;

	case Send:
		riogetsnarf();
		wsnarf(w);
		if(nsnarf == 0)
			break;
		if(w->rawing){
			waddraw(w, snarf, nsnarf);
			if(snarf[nsnarf-1]!='\n' && snarf[nsnarf-1]!='\004')
				waddraw(w, newline, 1);
		}else{
			winsert(w, snarf, nsnarf, w->nr);
			if(snarf[nsnarf-1]!='\n' && snarf[nsnarf-1]!='\004')
				winsert(w, newline, 1, w->nr);
		}
		wsetselect(w, w->nr, w->nr);
		wshow(w, w->nr);
		break;

	case Scroll:
		if(w->scrolling ^= 1)
			wshow(w, w->nr);
		break;
	case Cook:
		cooked ^= 1;
		break;
	}
	wclose(w);
	wsendctlmesg(w, Wakeup, ZR, nil);
	flushimage(display, 1);
}

int
rawon(void)
{
	return !cooked && !isecho(sfd);
}

/*
 * I/O with child rc.
 */

int label(Rune*, int);

void
rcoutputproc(void *arg)
{
	int i, cnt, n, nb, nr;
	static char data[9000];
	Conswritemesg cwm;
	Rune *r;
	Stringpair pair;

	i = 0;
	cnt = 0;
	for(;;){
		/* XXX Let typing have a go -- maybe there's a rubout waiting. */
		i = 1-i;
		n = read(rcfd, data+cnt, sizeof data-cnt);
		if(n <= 0){
			if(n < 0)
				fprint(2, "9term: rc read error: %r\n");
			threadexitsall("eof on rc output");
		}
		n = echocancel(data+cnt, n);
		if(n == 0)
			continue;
		cnt += n;
		r = runemalloc(cnt);
		cvttorunes(data, cnt-UTFmax, r, &nb, &nr, nil);
		/* approach end of buffer */
		while(fullrune(data+nb, cnt-nb)){
			nb += chartorune(&r[nr], data+nb);
			if(r[nr])
				nr++;
		}
		if(nb < cnt)
			memmove(data, data+nb, cnt-nb);
		cnt -= nb;

		nr = label(r, nr);
		if(nr == 0)
			continue;

		recv(w->conswrite, &cwm);
		pair.s = r;
		pair.ns = nr;
		send(cwm.cw, &pair);
	}
}

void
winterrupt(Window *w)
{
	char rubout[1];

	USED(w);
	rubout[0] = getintr(sfd);
	write(rcfd, rubout, 1);
}

int
intrc(void)
{
	return getintr(sfd);
}

/*
 * Process in-band messages about window title changes.
 * The messages are of the form:
 *
 *	\033];xxx\007
 *
 * where xxx is the new directory.  This format was chosen
 * because it changes the label on xterm windows.
 */
int
label(Rune *sr, int n)
{
	Rune *sl, *el, *er, *r;
	char *p, *dir;

	er = sr+n;
	for(r=er-1; r>=sr; r--)
		if(*r == '\007')
			break;
	if(r < sr)
		return n;

	el = r+1;
	for(sl=el-3; sl>=sr; sl--)
		if(sl[0]=='\033' && sl[1]==']' && sl[2]==';')
			break;
	if(sl < sr)
		return n;

	dir = smprint("%.*S", (el-1)-(sl+3), sl+3);
	if(dir){
		if(strcmp(dir, "*9term-hold+") == 0) {
			w->holding = 1;
			wrepaint(w);
			flushimage(display, 1);
		} else {
			drawsetlabel(dir);
			free(w->dir);
			w->dir = dir;
		}
	}

	/* remove trailing /-sysname if present */
	p = strrchr(dir, '/');
	if(p && *(p+1) == '-'){
		if(p == dir)
			p++;
		*p = 0;
	}

	runemove(sl, el, er-el);
	n -= (el-sl);
	return n;
}

void
rcinputproc(void *arg)
{
	static char data[9000];
	Consreadmesg crm;
	Channel *c1, *c2;
	Stringpair pair;

	for(;;){
		recv(w->consread, &crm);
		c1 = crm.c1;
		c2 = crm.c2;

		pair.s = data;
		pair.ns = sizeof data;
		send(c1, &pair);
		recv(c2, &pair);

		if(isecho(sfd))
			echoed(pair.s, pair.ns);
		if(write(rcfd, pair.s, pair.ns) < 0)
			threadexitsall(nil);
	}
}

/*
 * Snarf buffer - rio uses runes internally
 */
void
rioputsnarf(void)
{
	char *s;

	s = smprint("%.*S", nsnarf, snarf);
	if(s){
		putsnarf(s);
		free(s);
	}
}

void
riogetsnarf(void)
{
	char *s;
	int n, nb, nulls;

	s = getsnarf();
	if(s == nil)
		return;
	n = strlen(s)+1;
	free(snarf);
	snarf = runemalloc(n);
	cvttorunes(s, n, snarf, &nb, &nsnarf, &nulls);
	free(s);
}

/*
 * Clumsy hack to make " and "" work.
 * Then again, what's not a clumsy hack here in Unix land?
 */

char adir[100];
char thesocket[100];
int afd;

void listenproc(void*);
void textproc(void*);

void
removethesocket(void)
{
	if(thesocket[0])
		if(remove(thesocket) < 0)
			fprint(2, "remove %s: %r\n", thesocket);
}

void
servedevtext(void)
{
	char buf[100];

	snprint(buf, sizeof buf, "unix!/tmp/9term-text.%d", getpid());

	if((afd = announce(buf, adir)) < 0){
		putenv("text9term", "");
		return;
	}

	putenv("text9term", buf);
	proccreate(listenproc, nil, STACK);
	strcpy(thesocket, buf+5);
	atexit(removethesocket);
}

void
listenproc(void *arg)
{
	int fd;
	char dir[100];

	threadsetname("listen %s", thesocket);
	USED(arg);
	for(;;){
		fd = listen(adir, dir);
		if(fd < 0){
			close(afd);
			return;
		}
		proccreate(textproc, (void*)(uintptr)fd, STACK);
	}
}

void
textproc(void *arg)
{
	int fd, i, x, n, end;
	Rune r;
	char buf[4096], *p, *ep;

	threadsetname("textproc");
	fd = (uintptr)arg;
	p = buf;
	ep = buf+sizeof buf;
	if(w == nil){
		close(fd);
		return;
	}
	end = w->org+w->nr;	/* avoid possible output loop */
	for(i=w->org;; i++){
		if(i >= end || ep-p < UTFmax){
			for(x=0; x<p-buf; x+=n)
				if((n = write(fd, buf+x, (p-x)-buf)) <= 0)
					goto break2;

			if(i >= end)
				break;
			p = buf;
		}
		if(i < w->org)
			i = w->org;
		r = w->r[i-w->org];
		if(r < Runeself)
			*p++ = r;
		else
			p += runetochar(p, &r);
	}
break2:
	close(fd);
}
