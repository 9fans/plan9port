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
#include "dat.h"
#include "fns.h"
	/* for generating syms in mkfile only: */
	#include <bio.h>
	#include "edit.h"

void	mousethread(void*);
void	keyboardthread(void*);
void	waitthread(void*);
void	xfidallocthread(void*);
void	newwindowthread(void*);
void	plumbproc(void*);
int	timefmt(Fmt*);

Reffont	**fontcache;
int		nfontcache;
char		wdir[512] = ".";
Reffont	*reffonts[2];
int		snarffd = -1;
int		mainpid;
int		swapscrollbuttons = FALSE;
char		*mtpt;

enum{
	NSnarf = 1000	/* less than 1024, I/O buffer size */
};
Rune	snarfrune[NSnarf+1];

char		*fontnames[2] =
{
	"/lib/font/bit/lucsans/euro.8.font",
	"/lib/font/bit/lucm/unicode.9.font"
};

Command *command;

void	shutdownthread(void*);
void	acmeerrorinit(void);
void	readfile(Column*, char*);
static int	shutdown(void*, char*);

void
derror(Display *d, char *errorstr)
{
	USED(d);
	error(errorstr);
}

void
threadmain(int argc, char *argv[])
{
	int i;
	char *p, *loadfile;
	Column *c;
	int ncol;
	Display *d;

	rfork(RFENVG|RFNAMEG);

	ncol = -1;

	loadfile = nil;
	ARGBEGIN{
	case 'D':
		{extern int _threaddebuglevel;
		_threaddebuglevel = ~0;
		}
		break;
	case 'a':
		globalautoindent = TRUE;
		break;
	case 'b':
		bartflag = TRUE;
		break;
	case 'c':
		p = ARGF();
		if(p == nil)
			goto Usage;
		ncol = atoi(p);
		if(ncol <= 0)
			goto Usage;
		break;
	case 'f':
		fontnames[0] = ARGF();
		if(fontnames[0] == nil)
			goto Usage;
		break;
	case 'F':
		fontnames[1] = ARGF();
		if(fontnames[1] == nil)
			goto Usage;
		break;
	case 'l':
		loadfile = ARGF();
		if(loadfile == nil)
			goto Usage;
		break;
	case 'm':
		mtpt = ARGF();
		if(mtpt == nil)
			goto Usage;
		break;
	case 'r':
		swapscrollbuttons = TRUE;
		break;
	case 'W':
		winsize = ARGF();
		if(winsize == nil)
			goto Usage;
		break;
	default:
	Usage:
		fprint(2, "usage: acme -a -c ncol -f fontname -F fixedwidthfontname -l loadfile -W winsize\n");
		threadexitsall("usage");
	}ARGEND

	fontnames[0] = estrdup(fontnames[0]);
	fontnames[1] = estrdup(fontnames[1]);

	quotefmtinstall();
	fmtinstall('t', timefmt);

	cputype = getenv("cputype");
	objtype = getenv("objtype");
	home = getenv("HOME");
	acmeshell = getenv("acmeshell");
	if(acmeshell && *acmeshell == '\0')
		acmeshell = nil;
	p = getenv("tabstop");
	if(p != nil){
		maxtab = strtoul(p, nil, 0);
		free(p);
	}
	if(maxtab == 0)
		maxtab = 4;
	if(loadfile)
		rowloadfonts(loadfile);
	putenv("font", fontnames[0]);
	snarffd = open("/dev/snarf", OREAD|OCEXEC);
/*
	if(cputype){
		sprint(buf, "/acme/bin/%s", cputype);
		bind(buf, "/bin", MBEFORE);
	}
	bind("/acme/bin", "/bin", MBEFORE);
*/
	getwd(wdir, sizeof wdir);

/*
	if(geninitdraw(nil, derror, fontnames[0], "acme", nil, Refnone) < 0){
		fprint(2, "acme: can't open display: %r\n");
		threadexitsall("geninitdraw");
	}
*/
	if(initdraw(derror, fontnames[0], "acme") < 0){
		fprint(2, "acme: can't open display: %r\n");
		threadexitsall("initdraw");
	}

	d = display;
	font = d->defaultfont;
/*assert(font); */

	reffont.f = font;
	reffonts[0] = &reffont;
	incref(&reffont.ref);	/* one to hold up 'font' variable */
	incref(&reffont.ref);	/* one to hold up reffonts[0] */
	fontcache = emalloc(sizeof(Reffont*));
	nfontcache = 1;
	fontcache[0] = &reffont;

	iconinit();
	timerinit();
	rxinit();

	cwait = threadwaitchan();
	ccommand = chancreate(sizeof(Command**), 0);
	ckill = chancreate(sizeof(Rune*), 0);
	cxfidalloc = chancreate(sizeof(Xfid*), 0);
	cxfidfree = chancreate(sizeof(Xfid*), 0);
	cnewwindow = chancreate(sizeof(Channel*), 0);
	cerr = chancreate(sizeof(char*), 0);
	cedit = chancreate(sizeof(int), 0);
	cexit = chancreate(sizeof(int), 0);
	cwarn = chancreate(sizeof(void*), 1);
	if(cwait==nil || ccommand==nil || ckill==nil || cxfidalloc==nil || cxfidfree==nil || cerr==nil || cexit==nil || cwarn==nil){
		fprint(2, "acme: can't create initial channels: %r\n");
		threadexitsall("channels");
	}
	chansetname(ccommand, "ccommand");
	chansetname(ckill, "ckill");
	chansetname(cxfidalloc, "cxfidalloc");
	chansetname(cxfidfree, "cxfidfree");
	chansetname(cnewwindow, "cnewwindow");
	chansetname(cerr, "cerr");
	chansetname(cedit, "cedit");
	chansetname(cexit, "cexit");
	chansetname(cwarn, "cwarn");

	mousectl = initmouse(nil, screen);
	if(mousectl == nil){
		fprint(2, "acme: can't initialize mouse: %r\n");
		threadexitsall("mouse");
	}
	mouse = &mousectl->m;
	keyboardctl = initkeyboard(nil);
	if(keyboardctl == nil){
		fprint(2, "acme: can't initialize keyboard: %r\n");
		threadexitsall("keyboard");
	}
	mainpid = getpid();
	startplumbing();
/*
	plumbeditfd = plumbopen("edit", OREAD|OCEXEC);
	if(plumbeditfd < 0)
		fprint(2, "acme: can't initialize plumber: %r\n");
	else{
		cplumb = chancreate(sizeof(Plumbmsg*), 0);
		threadcreate(plumbproc, nil, STACK);
	}
	plumbsendfd = plumbopen("send", OWRITE|OCEXEC);
*/

	fsysinit();

	#define	WPERCOL	8
	disk = diskinit();
	if(!loadfile || !rowload(&row, loadfile, TRUE)){
		rowinit(&row, screen->clipr);
		if(ncol < 0){
			if(argc == 0)
				ncol = 2;
			else{
				ncol = (argc+(WPERCOL-1))/WPERCOL;
				if(ncol < 2)
					ncol = 2;
			}
		}
		if(ncol == 0)
			ncol = 2;
		for(i=0; i<ncol; i++){
			c = rowadd(&row, nil, -1);
			if(c==nil && i==0)
				error("initializing columns");
		}
		c = row.col[row.ncol-1];
		if(argc == 0)
			readfile(c, wdir);
		else
			for(i=0; i<argc; i++){
				p = utfrrune(argv[i], '/');
				if((p!=nil && strcmp(p, "/guide")==0) || i/WPERCOL>=row.ncol)
					readfile(c, argv[i]);
				else
					readfile(row.col[i/WPERCOL], argv[i]);
			}
	}
	flushimage(display, 1);

	acmeerrorinit();
	threadcreate(keyboardthread, nil, STACK);
	threadcreate(mousethread, nil, STACK);
	threadcreate(waitthread, nil, STACK);
	threadcreate(xfidallocthread, nil, STACK);
	threadcreate(newwindowthread, nil, STACK);
/*	threadcreate(shutdownthread, nil, STACK); */
	threadnotify(shutdown, 1);
	recvul(cexit);
	killprocs();
	threadexitsall(nil);
}

void
readfile(Column *c, char *s)
{
	Window *w;
	Rune rb[256];
	int nr;
	Runestr rs;

	w = coladd(c, nil, nil, -1);
	if(s[0] != '/')
		runesnprint(rb, sizeof rb, "%s/%s", wdir, s);
	else
		runesnprint(rb, sizeof rb, "%s", s);
	nr = runestrlen(rb);
	rs = cleanrname(runestr(rb, nr));
	winsetname(w, rs.r, rs.nr);
	textload(&w->body, 0, s, 1);
	w->body.file->mod = FALSE;
	w->dirty = FALSE;
	winsettag(w);
	winresize(w, w->r, FALSE, TRUE);
	textscrdraw(&w->body);
	textsetselect(&w->tag, w->tag.file->b.nc, w->tag.file->b.nc);
	xfidlog(w, "new");
}

char *ignotes[] = {
	"sys: write on closed pipe",
	"sys: ttin",
	"sys: ttou",
	"sys: tstp",
	nil
};

char *oknotes[] ={
	"delete",
	"hangup",
	"kill",
	"exit",
	nil
};

int	dumping;

static int
shutdown(void *v, char *msg)
{
	int i;

	USED(v);

	for(i=0; ignotes[i]; i++)
		if(strncmp(ignotes[i], msg, strlen(ignotes[i])) == 0)
			return 1;

	killprocs();
	if(!dumping && strcmp(msg, "kill")!=0 && strcmp(msg, "exit")!=0 && getpid()==mainpid){
		dumping = TRUE;
		rowdump(&row, nil);
	}
	for(i=0; oknotes[i]; i++)
		if(strncmp(oknotes[i], msg, strlen(oknotes[i])) == 0)
			threadexitsall(msg);
	print("acme: %s\n", msg);
	return 0;
}

/*
void
shutdownthread(void *v)
{
	char *msg;
	Channel *c;

	USED(v);

	threadsetname("shutdown");
	c = threadnotechan();
	while((msg = recvp(c)) != nil)
		shutdown(nil, msg);
}
*/

void
killprocs(void)
{
	Command *c;

	fsysclose();
/*	if(display) */
/*		flushimage(display, 1); */

	for(c=command; c; c=c->next)
		postnote(PNGROUP, c->pid, "hangup");
}

static int errorfd;
int erroutfd;

void
acmeerrorproc(void *v)
{
	char *buf;
	int n;

	USED(v);
	threadsetname("acmeerrorproc");
	buf = emalloc(8192+1);
	while((n=read(errorfd, buf, 8192)) >= 0){
		buf[n] = '\0';
		sendp(cerr, estrdup(buf));
	}
}

void
acmeerrorinit(void)
{
	int pfd[2];

	if(pipe(pfd) < 0)
		error("can't create pipe");
#if 0
	sprint(acmeerrorfile, "/srv/acme.%s.%d", getuser(), mainpid);
	fd = create(acmeerrorfile, OWRITE, 0666);
	if(fd < 0){
		remove(acmeerrorfile);
  		fd = create(acmeerrorfile, OWRITE, 0666);
		if(fd < 0)
			error("can't create acmeerror file");
	}
	sprint(buf, "%d", pfd[0]);
	write(fd, buf, strlen(buf));
	close(fd);
	/* reopen pfd[1] close on exec */
	sprint(buf, "/fd/%d", pfd[1]);
	errorfd = open(buf, OREAD|OCEXEC);
#endif
	fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
	fcntl(pfd[1], F_SETFD, FD_CLOEXEC);
	erroutfd = pfd[0];
	errorfd = pfd[1];
	if(errorfd < 0)
		error("can't re-open acmeerror file");
	proccreate(acmeerrorproc, nil, STACK);
}

/*
void
plumbproc(void *v)
{
	Plumbmsg *m;

	USED(v);
	threadsetname("plumbproc");
	for(;;){
		m = threadplumbrecv(plumbeditfd);
		if(m == nil)
			threadexits(nil);
		sendp(cplumb, m);
	}
}
*/

void
keyboardthread(void *v)
{
	Rune r;
	Timer *timer;
	Text *t;
	enum { KTimer, KKey, NKALT };
	static Alt alts[NKALT+1];

	USED(v);
	alts[KTimer].c = nil;
	alts[KTimer].v = nil;
	alts[KTimer].op = CHANNOP;
	alts[KKey].c = keyboardctl->c;
	alts[KKey].v = &r;
	alts[KKey].op = CHANRCV;
	alts[NKALT].op = CHANEND;

	timer = nil;
	typetext = nil;
	threadsetname("keyboardthread");
	for(;;){
		switch(alt(alts)){
		case KTimer:
			timerstop(timer);
			t = typetext;
			if(t!=nil && t->what==Tag){
				winlock(t->w, 'K');
				wincommit(t->w, t);
				winunlock(t->w);
				flushimage(display, 1);
			}
			alts[KTimer].c = nil;
			alts[KTimer].op = CHANNOP;
			break;
		case KKey:
		casekeyboard:
			typetext = rowtype(&row, r, mouse->xy);
			t = typetext;
			if(t!=nil && t->col!=nil && !(r==Kdown || r==Kleft || r==Kright))	/* scrolling doesn't change activecol */
				activecol = t->col;
			if(t!=nil && t->w!=nil)
				t->w->body.file->curtext = &t->w->body;
			if(timer != nil)
				timercancel(timer);
			if(t!=nil && t->what==Tag) {
				timer = timerstart(500);
				alts[KTimer].c = timer->c;
				alts[KTimer].op = CHANRCV;
			}else{
				timer = nil;
				alts[KTimer].c = nil;
				alts[KTimer].op = CHANNOP;
			}
			if(nbrecv(keyboardctl->c, &r) > 0)
				goto casekeyboard;
			flushimage(display, 1);
			break;
		}
	}
}

void
mousethread(void *v)
{
	Text *t, *argt;
	int but;
	uint q0, q1;
	Window *w;
	Plumbmsg *pm;
	Mouse m;
	char *act;
	enum { MResize, MMouse, MPlumb, MWarnings, NMALT };
	static Alt alts[NMALT+1];

	USED(v);
	threadsetname("mousethread");
	alts[MResize].c = mousectl->resizec;
	alts[MResize].v = nil;
	alts[MResize].op = CHANRCV;
	alts[MMouse].c = mousectl->c;
	alts[MMouse].v = &mousectl->m;
	alts[MMouse].op = CHANRCV;
	alts[MPlumb].c = cplumb;
	alts[MPlumb].v = &pm;
	alts[MPlumb].op = CHANRCV;
	alts[MWarnings].c = cwarn;
	alts[MWarnings].v = nil;
	alts[MWarnings].op = CHANRCV;
	if(cplumb == nil)
		alts[MPlumb].op = CHANNOP;
	alts[NMALT].op = CHANEND;

	for(;;){
		qlock(&row.lk);
		flushwarnings();
		qunlock(&row.lk);
		flushimage(display, 1);
		switch(alt(alts)){
		case MResize:
			if(getwindow(display, Refnone) < 0)
				error("attach to window");
			draw(screen, screen->r, display->white, nil, ZP);
			iconinit();
			scrlresize();
			rowresize(&row, screen->clipr);
			break;
		case MPlumb:
			if(strcmp(pm->type, "text") == 0){
				act = plumblookup(pm->attr, "action");
				if(act==nil || strcmp(act, "showfile")==0)
					plumblook(pm);
				else if(strcmp(act, "showdata")==0)
					plumbshow(pm);
			}
			plumbfree(pm);
			break;
		case MWarnings:
			break;
		case MMouse:
			/*
			 * Make a copy so decisions are consistent; mousectl changes
			 * underfoot.  Can't just receive into m because this introduces
			 * another race; see /sys/src/libdraw/mouse.c.
			 */
			m = mousectl->m;
			qlock(&row.lk);
			t = rowwhich(&row, m.xy);

			if((t!=mousetext && t!=nil && t->w!=nil) &&
				(mousetext==nil || mousetext->w==nil || t->w->id!=mousetext->w->id)) {
				xfidlog(t->w, "focus");
			}

			if(t!=mousetext && mousetext!=nil && mousetext->w!=nil){
				winlock(mousetext->w, 'M');
				mousetext->eq0 = ~0;
				wincommit(mousetext->w, mousetext);
				winunlock(mousetext->w);
			}
			mousetext = t;
			if(t == nil)
				goto Continue;
			w = t->w;
			if(t==nil || m.buttons==0)
				goto Continue;
			but = 0;
			if(m.buttons == 1)
				but = 1;
			else if(m.buttons == 2)
				but = 2;
			else if(m.buttons == 4)
				but = 3;
			barttext = t;
			if(t->what==Body && ptinrect(m.xy, t->scrollr)){
				if(but){
					if(swapscrollbuttons){
						if(but == 1)
							but = 3;
						else if(but == 3)
							but = 1;
					}
					winlock(w, 'M');
					t->eq0 = ~0;
					textscroll(t, but);
					winunlock(w);
				}
				goto Continue;
			}
			/* scroll buttons, wheels, etc. */
			if(w != nil && (m.buttons & (8|16))){
				if(m.buttons & 8)
					but = Kscrolloneup;
				else
					but = Kscrollonedown;
				winlock(w, 'M');
				t->eq0 = ~0;
				texttype(t, but);
				winunlock(w);
				goto Continue;
			}
			if(ptinrect(m.xy, t->scrollr)){
				if(but){
					if(t->what == Columntag)
						rowdragcol(&row, t->col, but);
					else if(t->what == Tag){
						coldragwin(t->col, t->w, but);
						if(t->w)
							barttext = &t->w->body;
					}
					if(t->col)
						activecol = t->col;
				}
				goto Continue;
			}
			if(m.buttons){
				if(w)
					winlock(w, 'M');
				t->eq0 = ~0;
				if(w)
					wincommit(w, t);
				else
					textcommit(t, TRUE);
				if(m.buttons & 1){
					textselect(t);
					if(w)
						winsettag(w);
					argtext = t;
					seltext = t;
					if(t->col)
						activecol = t->col;	/* button 1 only */
					if(t->w!=nil && t==&t->w->body)
						activewin = t->w;
				}else if(m.buttons & 2){
					if(textselect2(t, &q0, &q1, &argt))
						execute(t, q0, q1, FALSE, argt);
				}else if(m.buttons & 4){
					if(textselect3(t, &q0, &q1))
						look3(t, q0, q1, FALSE);
				}
				if(w)
					winunlock(w);
				goto Continue;
			}
    Continue:
			qunlock(&row.lk);
			break;
		}
	}
}

/*
 * There is a race between process exiting and our finding out it was ever created.
 * This structure keeps a list of processes that have exited we haven't heard of.
 */
typedef struct Pid Pid;
struct Pid
{
	int	pid;
	char	msg[ERRMAX];
	Pid	*next;
};

void
waitthread(void *v)
{
	Waitmsg *w;
	Command *c, *lc;
	uint pid;
	int found, ncmd;
	Rune *cmd;
	char *err;
	Text *t;
	Pid *pids, *p, *lastp;
	enum { WErr, WKill, WWait, WCmd, NWALT };
	Alt alts[NWALT+1];

	USED(v);
	threadsetname("waitthread");
	pids = nil;
	alts[WErr].c = cerr;
	alts[WErr].v = &err;
	alts[WErr].op = CHANRCV;
	alts[WKill].c = ckill;
	alts[WKill].v = &cmd;
	alts[WKill].op = CHANRCV;
	alts[WWait].c = cwait;
	alts[WWait].v = &w;
	alts[WWait].op = CHANRCV;
	alts[WCmd].c = ccommand;
	alts[WCmd].v = &c;
	alts[WCmd].op = CHANRCV;
	alts[NWALT].op = CHANEND;

	command = nil;
	for(;;){
		switch(alt(alts)){
		case WErr:
			qlock(&row.lk);
			warning(nil, "%s", err);
			free(err);
			flushimage(display, 1);
			qunlock(&row.lk);
			break;
		case WKill:
			found = FALSE;
			ncmd = runestrlen(cmd);
			for(c=command; c; c=c->next){
				/* -1 for blank */
				if(runeeq(c->name, c->nname-1, cmd, ncmd) == TRUE){
					if(postnote(PNGROUP, c->pid, "kill") < 0)
						warning(nil, "kill %S: %r\n", cmd);
					found = TRUE;
				}
			}
			if(!found)
				warning(nil, "Kill: no process %S\n", cmd);
			free(cmd);
			break;
		case WWait:
			pid = w->pid;
			lc = nil;
			for(c=command; c; c=c->next){
				if(c->pid == pid){
					if(lc)
						lc->next = c->next;
					else
						command = c->next;
					break;
				}
				lc = c;
			}
			qlock(&row.lk);
			t = &row.tag;
			textcommit(t, TRUE);
			if(c == nil){
				/* helper processes use this exit status */
				if(strncmp(w->msg, "libthread", 9) != 0){
					p = emalloc(sizeof(Pid));
					p->pid = pid;
					strncpy(p->msg, w->msg, sizeof(p->msg));
					p->next = pids;
					pids = p;
				}
			}else{
				if(search(t, c->name, c->nname)){
					textdelete(t, t->q0, t->q1, TRUE);
					textsetselect(t, 0, 0);
				}
				if(w->msg[0])
					warning(c->md, "%.*S: exit %s\n", c->nname-1, c->name, w->msg);
				flushimage(display, 1);
			}
			qunlock(&row.lk);
			free(w);
    Freecmd:
			if(c){
				if(c->iseditcmd)
					sendul(cedit, 0);
				free(c->text);
				free(c->name);
				fsysdelid(c->md);
				free(c);
			}
			break;
		case WCmd:
			/* has this command already exited? */
			lastp = nil;
			for(p=pids; p!=nil; p=p->next){
				if(p->pid == c->pid){
					if(p->msg[0])
						warning(c->md, "%s\n", p->msg);
					if(lastp == nil)
						pids = p->next;
					else
						lastp->next = p->next;
					free(p);
					goto Freecmd;
				}
				lastp = p;
			}
			c->next = command;
			command = c;
			qlock(&row.lk);
			t = &row.tag;
			textcommit(t, TRUE);
			textinsert(t, 0, c->name, c->nname, TRUE);
			textsetselect(t, 0, 0);
			flushimage(display, 1);
			qunlock(&row.lk);
			break;
		}
	}
}

void
xfidallocthread(void *v)
{
	Xfid *xfree, *x;
	enum { Alloc, Free, N };
	static Alt alts[N+1];

	USED(v);
	threadsetname("xfidallocthread");
	alts[Alloc].c = cxfidalloc;
	alts[Alloc].v = nil;
	alts[Alloc].op = CHANRCV;
	alts[Free].c = cxfidfree;
	alts[Free].v = &x;
	alts[Free].op = CHANRCV;
	alts[N].op = CHANEND;

	xfree = nil;
	for(;;){
		switch(alt(alts)){
		case Alloc:
			x = xfree;
			if(x)
				xfree = x->next;
			else{
				x = emalloc(sizeof(Xfid));
				x->c = chancreate(sizeof(void(*)(Xfid*)), 0);
				chansetname(x->c, "xc%p", x->c);
				x->arg = x;
				threadcreate(xfidctl, x->arg, STACK);
			}
			sendp(cxfidalloc, x);
			break;
		case Free:
			x->next = xfree;
			xfree = x;
			break;
		}
	}
}

/* this thread, in the main proc, allows fsysproc to get a window made without doing graphics */
void
newwindowthread(void *v)
{
	Window *w;

	USED(v);
	threadsetname("newwindowthread");

	for(;;){
		/* only fsysproc is talking to us, so synchronization is trivial */
		recvp(cnewwindow);
		w = makenewwindow(nil);
		winsettag(w);
		xfidlog(w, "new");
		sendp(cnewwindow, w);
	}
}

Reffont*
rfget(int fix, int save, int setfont, char *name)
{
	Reffont *r;
	Font *f;
	int i;

	r = nil;
	if(name == nil){
		name = fontnames[fix];
		r = reffonts[fix];
	}
	if(r == nil){
		for(i=0; i<nfontcache; i++)
			if(strcmp(name, fontcache[i]->f->name) == 0){
				r = fontcache[i];
				goto Found;
			}
		f = openfont(display, name);
		if(f == nil){
			warning(nil, "can't open font file %s: %r\n", name);
			return nil;
		}
		r = emalloc(sizeof(Reffont));
		r->f = f;
		fontcache = erealloc(fontcache, (nfontcache+1)*sizeof(Reffont*));
		fontcache[nfontcache++] = r;
	}
    Found:
	if(save){
		incref(&r->ref);
		if(reffonts[fix])
			rfclose(reffonts[fix]);
		reffonts[fix] = r;
		if(name != fontnames[fix]){
			free(fontnames[fix]);
			fontnames[fix] = estrdup(name);
		}
	}
	if(setfont){
		reffont.f = r->f;
		incref(&r->ref);
		rfclose(reffonts[0]);
		font = r->f;
		reffonts[0] = r;
		incref(&r->ref);
		iconinit();
	}
	incref(&r->ref);
	return r;
}

void
rfclose(Reffont *r)
{
	int i;

	if(decref(&r->ref) == 0){
		for(i=0; i<nfontcache; i++)
			if(r == fontcache[i])
				break;
		if(i >= nfontcache)
			warning(nil, "internal error: can't find font in cache\n");
		else{
			nfontcache--;
			memmove(fontcache+i, fontcache+i+1, (nfontcache-i)*sizeof(Reffont*));
		}
		freefont(r->f);
		free(r);
	}
}

Cursor boxcursor = {
	{-7, -7},
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	 0xFF, 0xFF, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F,
	 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xFF, 0xFF,
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE,
	 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
	 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
	 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x00}
};

void
iconinit(void)
{
	Rectangle r;
	Image *tmp;

	if(tagcols[BACK] == nil) {
		/* Blue */
		tagcols[BACK] = allocimagemix(display, DPalebluegreen, DWhite);
		tagcols[HIGH] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPalegreygreen);
		tagcols[BORD] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPurpleblue);
		tagcols[TEXT] = display->black;
		tagcols[HTEXT] = display->black;
	
		/* Yellow */
		textcols[BACK] = allocimagemix(display, DPaleyellow, DWhite);
		textcols[HIGH] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DDarkyellow);
		textcols[BORD] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DYellowgreen);
		textcols[TEXT] = display->black;
		textcols[HTEXT] = display->black;
	}
	
	r = Rect(0, 0, Scrollwid+ButtonBorder, font->height+1);
	if(button && eqrect(r, button->r))
		return;

	if(button){
		freeimage(button);
		freeimage(modbutton);
		freeimage(colbutton);
	}

	button = allocimage(display, r, screen->chan, 0, DNofill);
	draw(button, r, tagcols[BACK], nil, r.min);
	r.max.x -= ButtonBorder;
	border(button, r, ButtonBorder, tagcols[BORD], ZP);

	r = button->r;
	modbutton = allocimage(display, r, screen->chan, 0, DNofill);
	draw(modbutton, r, tagcols[BACK], nil, r.min);
	r.max.x -= ButtonBorder;
	border(modbutton, r, ButtonBorder, tagcols[BORD], ZP);
	r = insetrect(r, ButtonBorder);
	tmp = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DMedblue);
	draw(modbutton, r, tmp, nil, ZP);
	freeimage(tmp);

	r = button->r;
	colbutton = allocimage(display, r, screen->chan, 0, DPurpleblue);

	but2col = allocimage(display, r, screen->chan, 1, 0xAA0000FF);
	but3col = allocimage(display, r, screen->chan, 1, 0x006600FF);
}

/*
 * /dev/snarf updates when the file is closed, so we must open our own
 * fd here rather than use snarffd
 */

/* rio truncates larges snarf buffers, so this avoids using the
 * service if the string is huge */

#define MAXSNARF 100*1024

void
acmeputsnarf(void)
{
	int i, n;
	Fmt f;
	char *s;

	if(snarfbuf.nc==0)
		return;
	if(snarfbuf.nc > MAXSNARF)
		return;

	fmtstrinit(&f);
	for(i=0; i<snarfbuf.nc; i+=n){
		n = snarfbuf.nc-i;
		if(n >= NSnarf)
			n = NSnarf;
		bufread(&snarfbuf, i, snarfrune, n);
		if(fmtprint(&f, "%.*S", n, snarfrune) < 0)
			break;
	}
	s = fmtstrflush(&f);
	if(s && s[0])
		putsnarf(s);
	free(s);
}

void
acmegetsnarf(void)
{
	char *s;
	int nb, nr, nulls, len;
	Rune *r;

	s = getsnarf();
	if(s == nil || s[0]==0){
		free(s);
		return;
	}

	len = strlen(s);
	r = runemalloc(len+1);
	cvttorunes(s, len, r, &nb, &nr, &nulls);
	bufreset(&snarfbuf);
	bufinsert(&snarfbuf, 0, r, nr);
	free(r);
	free(s);
}

int
ismtpt(char *file)
{
	int n;

	if(mtpt == nil)
		return 0;

	/* This is not foolproof, but it will stop a lot of them. */
	n = strlen(mtpt);
	return strncmp(file, mtpt, n) == 0 && ((n > 0 && mtpt[n-1] == '/') || file[n] == '/' || file[n] == 0);
}

int
timefmt(Fmt *f)
{
	Tm *tm;

	tm = localtime(va_arg(f->args, ulong));
	return fmtprint(f, "%04d/%02d/%02d %02d:%02d:%02d",
		tm->year+1900, tm->mon+1, tm->mday, tm->hour, tm->min, tm->sec);
}

