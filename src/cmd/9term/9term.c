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
#include "term.h"

enum
{
	STACK = 32768
};

int noecho = 0;

void servedevtext(void);
void listenproc(void*);
void textthread(void*);

typedef struct Text	Text;
typedef struct Readbuf	Readbuf;

enum
{
	HiWater	= 640000,	/* max size of history */
	LoWater	= 400000,	/* min size of history after max'ed */
	MinWater	= 20000,
};

/* various geometric paramters */
enum
{
	Scrollwid 	= 12,		/* width of scroll bar */
	Scrollgap 	= 4,		/* gap right of scroll bar */
	Maxtab		= 4,
};

enum
{
	Cut,
	Paste,
	Snarf,
	Send,
	Plumb,
	Scroll,
	Cooked,
};

#define	ESC		0x1B
#define	CUT		0x18	/* ctrl-x */		
#define	COPY		0x03	/* crtl-c */
#define	PASTE		0x16	/* crtl-v */

#define	READBUFSIZE 8192
#define TRUE 1
#define FALSE 0


struct Text
{
	Frame		*f;		/* frame ofr terminal */
	Mouse		m;
	uint		nr;		/* num of runes in term */
	uint		maxr;	/* max num of runes in r */
	Rune		*r;		/* runes for term */
	uint		nraw;		/* num of runes in raw buffer */
	Rune		*raw;		/* raw buffer */
	uint		org;		/* first rune on the screen */
	uint		q0;		/* start of selection region */
	uint		q1;		/* end of selection region */
	uint		qh;		/* unix point */
	int		npart;		/* partial runes read from console */
	char		part[UTFmax];	
	int		nsnarf;		/* snarf buffer */
	Rune		*snarf;
};

struct Readbuf
{
	short	n;				/* # bytes in buf */
	uchar	data[READBUFSIZE];		/* data bytes */
};

void	mouse(void);
void	domenu2(int);
void	loop(void);
void	geom(void);
void	fill(void);
void	tcheck(void);
void	updatesel(void);
void	doreshape(void);
void	runewrite(Rune*, int);
void	consread(void);
void	conswrite(char*, int);
int	bswidth(Rune c, uint start, int eatnl);
void	cut(void);
void	paste(Rune*, int, int);
void	snarfupdate(void);
void	snarf(void);
void	show(uint);
void	key(Rune);
void	setorigin(uint org, int exact);
uint	line2q(uint);
uint	backnl(uint, uint);
int	cansee(uint);
uint	backnl(uint, uint);
void	addraw(Rune*, int);
void	mselect(void);
void	doubleclick(uint *q0, uint *q1);
int	clickmatch(int cl, int cr, int dir, uint *q);
Rune	*strrune(Rune *s, Rune c);
int	consready(void);
Rectangle scrpos(Rectangle r, ulong p0, ulong p1, ulong tot);
void	scrdraw(void);
void	scroll(int);
void	hostproc(void *arg);
void	hoststart(void);
void	plumbstart(void);
void	plumb(uint, uint);
void	plumbclick(uint*, uint*);
uint	insert(Rune*, int, uint, int);
void scrolldown(int);
void scrollup(int);

#define	runemalloc(n)		malloc((n)*sizeof(Rune))
#define	runerealloc(a, n)	realloc(a, (n)*sizeof(Rune))
#define	runemove(a, b, n)	memmove(a, b, (n)*sizeof(Rune))
Rectangle	scrollr;	/* scroll bar rectangle */
Rectangle	lastsr;		/* used for scroll bar */
int		holdon;		/* hold mode */
int		rawon(void);		/* raw mode */
int		cooked;		/* force cooked */
int		scrolling;	/* window scrolls */
int		clickmsec;	/* time of last click */
uint		clickq0;	/* point of last click */
int		rcfd;
int		sfd;	/* slave fd, to get/set terminal mode */
int		rcpid;
int		maxtab;
int		use9wm;
Mousectl*	mc;
Keyboardctl*	kc;
Channel*	hostc;
Readbuf		rcbuf[2];
int		mainpid;
int		acmecolors;
int		plumbfd;
int		button2exec;
int		label(Rune*, int);
char		wdir[1024];
char		childwdir[1024];
void		hangupnote(void*, char*);
char		thesocket[100];

char *menu2str[] = {
	"cut",
	"paste",
	"snarf",
	"send",
	"plumb",
	"scroll",
	"cooked",
	0
};

Image* cols[NCOL];
Image* hcols[NCOL];
Image* palegrey;
Image* paleblue;
Image* blue;
Image *plumbcolor;
Image *execcolor;

Menu menu2 =
{
	menu2str
};

Text	t;

Cursor whitearrow = {
	{0, 0},
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFC, 
	 0xFF, 0xF0, 0xFF, 0xF0, 0xFF, 0xF8, 0xFF, 0xFC, 
	 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFC, 
	 0xF3, 0xF8, 0xF1, 0xF0, 0xE0, 0xE0, 0xC0, 0x40, },
	{0xFF, 0xFF, 0xFF, 0xFF, 0xC0, 0x06, 0xC0, 0x1C, 
	 0xC0, 0x30, 0xC0, 0x30, 0xC0, 0x38, 0xC0, 0x1C, 
	 0xC0, 0x0E, 0xC0, 0x07, 0xCE, 0x0E, 0xDF, 0x1C, 
	 0xD3, 0xB8, 0xF1, 0xF0, 0xE0, 0xE0, 0xC0, 0x40, }
};

Cursor query = {
	{-7,-7},
	{0x0f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc, 0x7f, 0xfe,
	 0x7c, 0x7e, 0x78, 0x7e, 0x00, 0xfc, 0x01, 0xf8,
	 0x03, 0xf0, 0x07, 0xe0, 0x07, 0xc0, 0x07, 0xc0,
	 0x07, 0xc0, 0x07, 0xc0, 0x07, 0xc0, 0x07, 0xc0, },
	{0x00, 0x00, 0x0f, 0xf0, 0x1f, 0xf8, 0x3c, 0x3c,
	 0x38, 0x1c, 0x00, 0x3c, 0x00, 0x78, 0x00, 0xf0,
	 0x01, 0xe0, 0x03, 0xc0, 0x03, 0x80, 0x03, 0x80,
	 0x00, 0x00, 0x03, 0x80, 0x03, 0x80, 0x00, 0x00, }
};

void
usage(void)
{
	fprint(2, "usage: 9term [-ars] [-W winsize] [cmd ...]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	char *p, *font;
	char buf[32];

	rfork(RFNOTEG);
	font = nil;
	_wantfocuschanges = 1;
	mainpid = getpid();
	ARGBEGIN{
	default:
		usage();
	case 'a':	/* acme mode */
		button2exec++;
		break;
	case 'f':
		font = EARGF(usage());
		break;
	case 's':
		scrolling++;
		break;
	case 'w':	/* started from "rio" window manager */
		use9wm = 1;
		break;
	case 'W':
		winsize = EARGF(usage());
		break;
	}ARGEND

	if(font)
		putenv("font", font);

	p = getenv("tabstop");
	if(p == 0)
		p = getenv("TABSTOP");
	if(p != 0 && maxtab <= 0)
		maxtab = strtoul(p, 0, 0);
	if(maxtab <= 0)
		maxtab = 4;	/* be like rio */

	snprint(buf, sizeof buf, "%d", maxtab);
	putenv("tabstop", buf);

	initdraw(0, nil, "9term");
	notify(hangupnote);
	noteenable("sys: child");
	servedevtext();

	mc = initmouse(nil, screen);
	kc = initkeyboard(nil);
	rcpid = rcstart(argc, argv, &rcfd, &sfd);
	hoststart();
	plumbstart();

	t.f = mallocz(sizeof(Frame), 1);

	if(acmecolors){
		cols[BACK] = allocimagemix(display, DPaleyellow, DWhite);
		cols[HIGH] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, DDarkyellow);
		cols[BORD] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, DYellowgreen);
	}else{
		cols[BACK] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, DWhite);
		cols[HIGH] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0xCCCCCCFF);
		cols[BORD] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x999999FF);
	}
	cols[TEXT] = display->black;
	cols[HTEXT] = display->black;
	palegrey = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x666666FF);

	hcols[BACK] = cols[BACK];
	hcols[HIGH] = cols[HIGH];
	blue = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, DMedblue);
	paleblue = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, DGreyblue);

	hcols[BORD] = blue;
	hcols[TEXT] = hcols[BORD];
	hcols[HTEXT] = hcols[TEXT];

	plumbcolor = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x006600FF);
	execcolor = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xAA0000FF);

	if(!blue || !palegrey || !paleblue || !plumbcolor || !execcolor)
		sysfatal("alloc colors: %r");
	draw(screen, screen->r, cols[BACK], nil, ZP);
	geom();
	loop();
}

int
isexpand(Rune r)
{
	return r=='_' || ('0' <= r && r <= '9') || isalpharune(r);
}

void
hangupnote(void *a, char *msg)
{
	if(getpid() != mainpid)
		noted(NDFLT);
	if(strcmp(msg, "hangup") == 0 && rcpid != 0){
		postnote(PNGROUP, rcpid, "hangup");
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
hostproc(void *arg)
{
	Channel *c;
	int i, n, which;

	c = arg;

	i = 0;
	for(;;){
		/* Let typing have a go -- maybe there's a rubout waiting. */
		yield();

		i = 1-i;	/* toggle */
		n = read(rcfd, rcbuf[i].data, sizeof rcbuf[i].data);
		if(n <= 0){
			if(n < 0)
				fprint(2, "9term: host read error: %r\n");
			threadexitsall("host");
		}
		rcbuf[i].n = n;
		which = i;
		send(c, &which);
	}
}

void
hoststart(void)
{
	hostc = chancreate(sizeof(int), 0);
	proccreate(hostproc, hostc, 32*1024);
}

void
loop(void)
{
	Rune r;
	int i;
	Alt a[5];

	a[0].c = mc->c;
	a[0].v = &mc->m;
	a[0].op = CHANRCV;

	a[1].c = kc->c;
	a[1].v = &r;
	a[1].op = CHANRCV;

	a[2].c = hostc;
	a[2].v = &i;
	a[2].op = CHANRCV;

	a[3].c = mc->resizec;
	a[3].v = nil;
	a[3].op = CHANRCV;

	a[4].c = nil;
	a[4].v = nil;
	a[4].op = CHANEND;

	for(;;) {
		tcheck();

		scrdraw();
		flushimage(display, 1);
		a[2].op = CHANRCV;
		if(!scrolling && t.qh > t.org+t.f->nchars)
			a[2].op = CHANNOP;;
		switch(alt(a)) {
		default:
			sysfatal("impossible");
		case 0:
			t.m = mc->m;
			mouse();
			break;
		case 1:
			key(r);
			break;
		case 2:
			conswrite((char*)rcbuf[i].data, rcbuf[i].n);
			break;
		case 3:
			doreshape();
			break;
		}
	}
}

void
doreshape(void)
{
	if(getwindow(display, Refnone) < 0)
		sysfatal("can't reattach to window");
	draw(screen, screen->r, cols[BACK], nil, ZP);
	geom();
	scrdraw();
}

void
geom(void)
{
	Point p;
	Rectangle r;

	if(!acmecolors){
		if(_windowhasfocus){
			cols[TEXT] = cols[HTEXT] = display->black;
			hcols[TEXT] = hcols[HTEXT] = blue;
		}else{
			cols[TEXT] = cols[HTEXT] = palegrey;
			hcols[TEXT] = hcols[HTEXT] = paleblue;
		}
	}

	r = screen->r;
	r.min.y++;
	r.max.y--;

	scrollr = r;
	scrollr.max.x = r.min.x+Scrollwid;
	lastsr = Rect(0,0,0,0);

	r.min.x += Scrollwid+Scrollgap;

	frclear(t.f, 0);
	frinit(t.f, r, font, screen, holdon ? hcols : cols);
	t.f->maxtab = maxtab*stringwidth(font, "0");
	fill();
	updatesel();

	p = stringsize(font, "0");
	if(p.x == 0 || p.y == 0)
		return;

	updatewinsize(Dy(r)/p.y, Dx(r)/p.x, Dx(r), Dy(r));
}

void
drawhold(int holdon)
{
	if(holdon)
		setcursor(mc, &whitearrow);
	else
		setcursor(mc, nil);

	draw(screen, screen->r, cols[BACK], nil, ZP);
	geom();
	scrdraw();
}

void
wordclick(uint *q0, uint *q1)
{
	while(*q1<t.nr && !isspace(t.r[*q1]))
		(*q1)++;
	while(*q0>0 && !isspace(t.r[*q0-1]))
		(*q0)--;
}

int
aselect(uint *q0, uint *q1, Image *color)
{
	int cancel;
	uint oldq0, oldq1, newq0, newq1;

	/* save old selection */
	oldq0 = t.q0;
	oldq1 = t.q1;

	/* sweep out area and record it */
	t.f->cols[HIGH] = color;
	t.f->cols[HTEXT] = display->white;
	mselect();
	newq0 = t.q0;
	newq1 = t.q1;

	cancel = 0;
	if(t.m.buttons != 0){
		while(t.m.buttons){
			readmouse(mc);
			t.m = mc->m;
		}
		cancel = 1;
	}

	/* restore old selection */
	t.f->cols[HIGH] = cols[HIGH];
	t.f->cols[HTEXT] = cols[HTEXT];
	t.q0 = oldq0;
	t.q1 = oldq1;
	updatesel();

	if(cancel)
		return -1;

	/* selected a region */
	if(newq0 < newq1){
		*q0 = newq0;
		*q1 = newq1;
		return 0;
	}

	/* clicked inside previous selection */
	/* the "<=" in newq0 <= oldq1 allows us to click the right edge */
	if(oldq0 <= newq0 && newq0 <= oldq1){
		*q0 = oldq0;
		*q1 = oldq1;
		return 0;
	}

	/* just a click */
	*q0 = newq0;
	*q1 = newq1;
	return 0;
}

static Rune Lnl[1] = { '\n' };

void
mouse(void)
{
	int but;
	uint q0, q1;

	but = t.m.buttons;

	if(but != 1 && but != 2 && but != 4 && but != 8 && but != 16)
		return;

	if (ptinrect(t.m.xy, scrollr)) {
		scroll(but);
		if(t.qh<=t.org+t.f->nchars)
			consread();
		return;
	}
		
	switch(but) {
	case 1:
		mselect();
		break;
	case 2:
		if(button2exec){
			if(aselect(&q0, &q1, execcolor) >= 0){
				if(q0 == q1)
					wordclick(&q0, &q1);
				if(q0 == q1)
					break;
				t.q0 = t.q1 = t.nr;
				updatesel();
				paste(t.r+q0, q1-q0, 1);
				if(t.r[q1-1] != '\n')
					paste(Lnl, 1, 1);
			}
			break;
		}
		domenu2(2);
		break;
	case 4:
		bouncemouse(&t.m);
		break;
	/*
		if(aselect(&q0, &q1, plumbcolor) >= 0)
			plumb(q0, q1);
		break;
	*/
	case 8:
		scrollup(mousescrollsize(t.f->maxlines));
		break;
	case 16:
		scrolldown(mousescrollsize(t.f->maxlines));
		break;
	}
}

void
mselect(void)
{
	int b, x, y;
	uint q0;

	b = t.m.buttons;
	q0 = frcharofpt(t.f, t.m.xy) + t.org;
	if(t.m.msec-clickmsec<500 && clickq0==q0 && t.q0==t.q1 && b==1){
		doubleclick(&t.q0, &t.q1);
		updatesel();
/*		t.t.i->flush(); */
		x = t.m.xy.x;
		y = t.m.xy.y;
		/* stay here until something interesting happens */
		do {
			readmouse(mc);
			t.m = mc->m;
		} while(t.m.buttons==b && abs(t.m.xy.x-x)<4 && abs(t.m.xy.y-y)<4);
		t.m.xy.x = x;	/* in case we're calling frselect */
		t.m.xy.y = y;
		clickmsec = 0;
	}

	if(t.m.buttons == b) {
		frselect(t.f, mc);
		t.m = mc->m;
		t.q0 = t.f->p0 + t.org;
		t.q1 = t.f->p1 + t.org;
		clickmsec = t.m.msec;
		clickq0 = t.q0;
	}
	if((t.m.buttons != b) &&(b&1)){
		enum{Cancut = 1, Canpaste = 2} state = Cancut | Canpaste;
		while(t.m.buttons){
			if(t.m.buttons&2){
				if(state&Cancut){
					snarf();
					cut();
					state = Canpaste;
				}
			}else if(t.m.buttons&4){
				if(state&Canpaste){
					snarfupdate();
					if(t.nsnarf){
						paste(t.snarf, t.nsnarf, 0);
					}
					state = Cancut;
				}
			}
			readmouse(mc);
			t.m = mc->m;
		}
	}
}

Rune newline[] = { '\n', 0 };

void
domenu2(int but)
{
	if(scrolling)
		menu2str[Scroll] = "+ scroll";
	else
		menu2str[Scroll] = "- scroll";
	if(cooked)
		menu2str[Cooked] = "+ mustecho";
	else
		menu2str[Cooked] = "- mustecho";

	switch(menuhit(but, mc, &menu2, nil)){
	case -1:
		break;
	case Cut:
		snarf();
		cut();
		if(scrolling)
			show(t.q0);
		break;
	case Paste:
		snarfupdate();
		paste(t.snarf, t.nsnarf, 0);
		if(scrolling)
			show(t.q0);
		break;
	case Snarf:
		snarf();
		if(scrolling)
			show(t.q0);
		break;
	case Send:
		if(t.q0 != t.q1)
			snarf();
		else
			snarfupdate();
		t.q0 = t.q1 = t.nr;
		updatesel();
		paste(t.snarf, t.nsnarf, 1);
		if(t.nsnarf == 0 || t.snarf[t.nsnarf-1] != '\n')
			paste(newline, 1, 1);
		show(t.nr);
		consread();
		break;
	case Scroll:
		scrolling = !scrolling;
		if (scrolling) {
			show(t.nr);
			consread();
		}
		break;
	case Plumb:
		plumb(t.q0, t.q1);
		break;
	case Cooked:
		cooked = !cooked;
		break;
	default:
		sysfatal("bad menu item");
	}
}

int
windfilewidth(uint q0, int oneelement)
{
	uint q;
	Rune r;

	q = q0;
	while(q > 0){
		r = t.r[q-1];
		if(r<=' ')
			break;
		if(oneelement && r=='/')
			break;
		--q;
	}
	return q0-q;
}

void
showcandidates(Completion *c)
{
	int i;
	Fmt f;
	Rune *rp;
	uint nr, qline, q0;
	char *s;

	runefmtstrinit(&f);
	if (c->nmatch == 0)
		s = "[no matches in ";
	else
		s = "[";
	if(c->nfile > 32)
		fmtprint(&f, "%s%d files]\n", s, c->nfile);
	else{
		fmtprint(&f, "%s", s);
		for(i=0; i<c->nfile; i++){
			if(i > 0)
				fmtprint(&f, " ");
			fmtprint(&f, "%s", c->filename[i]);
		}
		fmtprint(&f, "]\n");
	}
	/* place text at beginning of line before host point */
	qline = t.qh;
	while(qline>0 && t.r[qline-1] != '\n')
		qline--;

	rp = runefmtstrflush(&f);
	nr = runestrlen(rp);

	q0 = t.q0;
	q0 += insert(rp, nr, qline, 0) - qline;
	free(rp);
	t.q0 = q0+nr;
	t.q1 = q0+nr;
	updatesel();
}

Rune*
namecomplete(void)
{
	int nstr, npath;
	Rune *rp, *path, *str;
	Completion *c;
	char *s, *dir, *root;

	/* control-f: filename completion; works back to white space or / */
	if(t.q0<t.nr && t.r[t.q0]>' ')	/* must be at end of word */
		return nil;
	nstr = windfilewidth(t.q0, TRUE);
	str = runemalloc(nstr);
	runemove(str, t.r+(t.q0-nstr), nstr);
	npath = windfilewidth(t.q0-nstr, FALSE);
	path = runemalloc(npath);
	runemove(path, t.r+(t.q0-nstr-npath), npath);
	rp = nil;

	/* is path rooted? if not, we need to make it relative to window path */
	if(npath>0 && path[0]=='/'){
		dir = malloc(UTFmax*npath+1);
		sprint(dir, "%.*S", npath, path);
	}else{
		if(strcmp(wdir, "") == 0)
			root = ".";
		else
			root = wdir;
		dir = malloc(strlen(root)+1+UTFmax*npath+1);
		sprint(dir, "%s/%.*S", root, npath, path);
	}
	dir = cleanname(dir);

	s = smprint("%.*S", nstr, str);
	c = complete(dir, s);
	free(s);
	if(c == nil)
		goto Return;

	if(!c->advance)
		showcandidates(c);

	if(c->advance)
		rp = runesmprint("%s", c->string);

  Return:
	freecompletion(c);
	free(dir);
	free(path);
	free(str);
	return rp;
}

void
scrollup(int n)
{
	setorigin(backnl(t.org, n), 1);
}

void
scrolldown(int n)
{
	setorigin(line2q(n), 1);
	if(t.qh<=t.org+t.f->nchars)
		consread();
}

void
key(Rune r)
{
	Rune *rp;
	int nr;

	if(r == 0)
		return;
	switch(r){
	case Kpgup:
		scrollup(t.f->maxlines*2/3);
		return;
	case Kpgdown:
		scrolldown(t.f->maxlines*2/3);
		return;
	case Kup:
		scrollup(t.f->maxlines/3);
		return;
	case Kdown:
		scrolldown(t.f->maxlines/3);
		return;
	case Kleft:
		if(t.q0 > 0){
			t.q0--;
			t.q1 = t.q0;
			updatesel();
			show(t.q0);
		}
		return;
	case Kright:
		if(t.q1 < t.nr){
			t.q1++;
			t.q0 = t.q1;
			updatesel();
			show(t.q1);
		}
		return;
	case Khome:
		show(0);
		return;
	case Kend:
	case 0x05:
		show(t.nr);
		return;

	/*
	 * Non-standard extensions.
	 */
	case CUT:
		snarf();
		cut();
		if(scrolling)
			show(t.q0);
		return;
	case COPY:
		snarf();
		if(scrolling)
			show(t.q0);
		return;
	case PASTE:
		snarfupdate();
		paste(t.snarf, t.nsnarf, 0);
		if(scrolling)
			show(t.q0);
		return;
	}

	if(rawon() && t.q0 == t.nr){
		addraw(&r, 1);
		consread();
		return;
	}

	if(r == 0x7F){	/* DEL: send interrupt; what a mess */
		if(holdon){
			holdon = 0;
			drawhold(holdon);
		}
		t.qh = t.q0 = t.q1 = t.nr;
		show(t.q0);
		write(rcfd, "\x7F", 1);
		return;
	}

	if(r==ESC){	/* toggle hold */
		holdon = !holdon;
		drawhold(holdon);
	/*	replaceintegerproperty("_9WM_HOLD_MODE", 1, 32, holdon); */
		if(!holdon)
			consread();
		return;
	}
	
	snarf();

	switch(r) {
	case 0x06:	/* ^F: file name completion */
	case Kins:		/* Insert: file name completion */
		rp = namecomplete();
		if(rp == nil)
			return;
		nr = runestrlen(rp);
		paste(rp, nr, 1);
		free(rp);
		return;
	case 0x08:	/* ^H: erase character */
	case 0x15:	/* ^U: erase line */
	case 0x17:	/* ^W: erase word */
		if (t.q0 != 0 && t.q0 != t.qh)
			t.q0 -= bswidth(r, t.q0, 1);
		cut();
		break;
	default:
		paste(&r, 1, 1);
		break;
	}
	if(scrolling)
		show(t.q0);
}

int
bswidth(Rune c, uint start, int eatnl)
{
	uint q, eq, stop;
	Rune r;
	int skipping;

	/* there is known to be at least one character to erase */
	if(c == 0x08)	/* ^H: erase character */
		return 1;
	q = start;
	stop = 0;
	if(q > t.qh)
		stop = t.qh;
	skipping = 1;
	while(q > stop){
		r = t.r[q-1];
		if(r == '\n'){		/* eat at most one more character */
			if(q == start && eatnl)	/* eat the newline */
				--q;
			break; 
		}
		if(c == 0x17){
			eq = isexpand(r);
			if(eq && skipping)	/* found one; stop skipping */
				skipping = 0;
			else if(!eq && !skipping)
				break;
		}
		--q;
	}
	return start-q;
}

int
consready(void)
{
	int i, c;

	if(holdon)
		return 0;

	if(rawon()) 
		return t.nraw != 0;

	/* look to see if there is a complete line */
	for(i=t.qh; i<t.nr; i++){
		c = t.r[i];
		if(c=='\n' || c=='\004' || c=='\x7F')
			return 1;
	}
	return 0;
}


void
consread(void)
{
	char buf[8000], *p;
	int c, width, n;
	int s, raw;

	raw = rawon();
	for(;;) {
		if(!consready())
			return;
		n = sizeof(buf);
		p = buf;
		c = 0;
		while(n >= UTFmax && (t.qh<t.nr || t.nraw > 0)) {
			if(t.qh == t.nr){
				width = runetochar(p, &t.raw[0]);
				t.nraw--;
				runemove(t.raw, t.raw+1, t.nraw);
			}else
				width = runetochar(p, &t.r[t.qh++]);
			c = *p;
			p += width;
			n -= width;
			if(!raw && (c == '\n' || c == '\004' || c == '\x7F'))
				break;
		}
		n = p-buf;

		/*
		 * If we've been echoing, make sure the terminal isn't
		 * while we do the write.  This screws up if someone 
		 * else tries to turn off echo at the same time we do
		 * (we'll turn it on again after the write), but that's not
		 * too likely.
		 */
		s = setecho(sfd, 0);
		if(write(rcfd, buf, n) < 0)
			threadexitsall(0);
		if(s)
			setecho(sfd, s);
	}
}

void
conswrite(char *p, int n)
{
	int n2, i;
	Rune buf2[1000], *q;

	/* convert to runes */
	i = t.npart;
	if(i > 0){
		/* handle partial runes */
		while(i < UTFmax && n>0) {
			t.part[i] = *p;
			i++;
			p++;
			n--;
			if(fullrune(t.part, i)) {
				t.npart = 0;
				chartorune(buf2, t.part);
				runewrite(buf2, 1);
				break;
			}
		}
		/* there is a little extra room in a message buf */
	}

	while(n >= UTFmax || fullrune(p, n)) {
		n2 = nelem(buf2);
		q = buf2;

		while(n2) {
			if(n < UTFmax && !fullrune(p, n))
				break;
			i = chartorune(q, p);
			p += i;
			n -= i;
			n2--;
			q++;
		}
		runewrite(buf2, q-buf2);
	}

	if(n != 0) {
		assert(n+t.npart < UTFmax);
		memcpy(t.part+t.npart, p, n);
		t.npart += n;
	}

	if(scrolling)
		show(t.qh);
}

void
runewrite(Rune *r, int n)
{
	int i;
	uint initial;
	uint q0, q1;
	uint p0, p1;
	Rune *p, *q;

	n = label(r, n);
	if(n == 0)
		return;

	/* get rid of backspaces */
	initial = 0;
	p = q = r;
	for(i=0; i<n; i++) {
		if(*p == '\b') {
			if(q == r)
				initial++;
			else
				--q;
		} else if(*p == '\r') {	/* treat like ^U */
			/* convert CR without NL into erased line */
			/* i feel really sleazy about this but it helps */
			while(i<n-1 && *(p+1) == '\r'){
				i++;
				p++;
			}
			if(i<n-1 && *(p+1) != '\n'){
				while(q > r && *(q-1) != '\n')
					q--;
				if(q==r)
					initial = bswidth(0x15, t.qh, 0);
			}
		} else if(*p)
			*q++ = *p;
		p++;
	}
	n = q-r;

	if(initial){
		/* write turned into a delete */

		if(initial > t.qh)
			initial = t.qh;
		q0 = t.qh-initial;
		q1 = t.qh;

		runemove(t.r+q0, t.r+q1, t.nr-q1);
		t.nr -= initial;
		t.qh -= initial;
		if(t.q0 > q1)
			t.q0 -= initial;
		else if(t.q0 > q0)
			t.q0 = q0;
		if(t.q1 > q1)
			t.q1 -= initial;
		else if(t.q1 > q0)
			t.q1 = q0;
		if(t.org > q1)
			t.org -= initial;
		else if(q0 < t.org+t.f->nchars){
			if(t.org < q0)
				p0 = q0 - t.org;
			else {
				t.org = q0;
				p0 = 0;
			}
			p1 = q1 - t.org;
			if(p1 > t.f->nchars)
				p1 = t.f->nchars;
			frdelete(t.f, p0, p1);
			fill();
		}
		updatesel();
	}

	insert(r, n, t.qh, 1);
}


void
cut(void)
{
	uint n, p0, p1;
	uint q0, q1;

	q0 = t.q0;
	q1 = t.q1;

	if (q0 < t.org && q1 >= t.org)
		show(q0);

	n = q1-q0;
	if(n == 0)
		return;
	runemove(t.r+q0, t.r+q1, t.nr-q1);
	t.nr -= n;
	t.q0 = t.q1 = q0;
	if(q1 < t.qh)
		t.qh -= n;
	else if(q0 < t.qh)
		t.qh = q0;
	if(q1 < t.org)
		t.org -= n;
	else if(q0 < t.org+t.f->nchars){
		assert(q0 >= t.org);
		p0 = q0 - t.org;
		p1 = q1 - t.org;
		if(p1 > t.f->nchars)
			p1 = t.f->nchars;
		frdelete(t.f, p0, p1);
		fill();
	}
	updatesel();
}

void
snarfupdate(void)
{
	char *pp;
	int n, i;
	Rune *p;

	pp = getsnarf();
	if(pp == nil)
		return;
	n = strlen(pp);
	if(n <= 0) {
		 /*t.nsnarf = 0;*/
		return;
	}
	t.snarf = runerealloc(t.snarf, n);
	for(i=0,p=t.snarf; i<n; p++)
		i += chartorune(p, pp+i);
	t.nsnarf = p-t.snarf;

}

char sbuf[SnarfSize];
void
snarf(void)
{
	char *p;
	int i, n;
	Rune *rp;

	if(t.q1 == t.q0)
		return;
	n = t.q1-t.q0;
	t.snarf = runerealloc(t.snarf, n);
	for(i=0,p=sbuf,rp=t.snarf; i<n && p < sbuf+SnarfSize-UTFmax; i++){
		*rp++ = *(t.r+t.q0+i);
		p += runetochar(p, t.r+t.q0+i);
	}
	t.nsnarf = rp-t.snarf;
	*p = '\0';
	putsnarf(sbuf);
}

uint
min(uint x, uint y)
{
	if(x < y)
		return x;
	return y;
}

uint
max(uint x, uint y)
{
	if(x > y)
		return x;
	return y;
}

uint
insert(Rune *r, int n, uint q0, int hostwrite)
{
	uint m;

	if(n == 0)
		return q0;
	if(t.nr+n>HiWater && q0>=t.org && q0>=t.qh){
		m = min(HiWater-LoWater, min(t.org, t.qh));
		t.org -= m;
		t.qh -= m;
		if(t.q0 > m)
			t.q0 -= m;
		else
			t.q0 = 0;
		if(t.q1 > m)
			t.q1 -= m;
		else
			t.q1 = 0;
		t.nr -= m;
		runemove(t.r, t.r+m, t.nr);
		q0 -= m;
	}
	if(t.nr+n > t.maxr){
		/*
		 * Minimize realloc breakage:
		 *	Allocate at least MinWater
		 * 	Double allocation size each time
		 *	But don't go much above HiWater
		 */
		m = max(min(2*(t.nr+n), HiWater), t.nr+n)+MinWater;
		if(m > HiWater)
			m = max(HiWater+MinWater, t.nr+n);
		if(m > t.maxr){
			t.r = runerealloc(t.r, m);
			t.maxr = m;
		}
	}
	runemove(t.r+q0+n, t.r+q0, t.nr-q0);
	runemove(t.r+q0, r, n);
	t.nr += n;
	/* if output touches, advance selection, not qh; works best for keyboard and output */
	if(q0 <= t.q1)
		t.q1 += n;
	if(q0 <= t.q0)
		t.q0 += n;
	if(q0 < t.qh || (q0==t.qh && hostwrite))
		t.qh += n;
	else
		consread();
	if(q0 < t.org)
		t.org += n;
	else if(q0 <= t.org+t.f->nchars)
		frinsert(t.f, r, r+n, q0-t.org);
	return q0;
}

void
paste(Rune *r, int n, int advance)
{
	Rune *rbuf;

	if(rawon() && t.q0==t.nr){
		addraw(r, n);
		consread();
		return;
	}

	cut();
	if(n == 0)
		return;

	/*
	 * if this is a button2 execute then we might have been passed
	 * runes inside the buffer.  must save them before realloc.
	 */
	rbuf = nil;
	if(t.r <= r && r < t.r+n){
		rbuf = runemalloc(n);
		runemove(rbuf, r, n);
		r = rbuf;
	}

	insert(r, n, t.q0, 0);
	updatesel();
	free(rbuf);
}

void
fill(void)
{
	if (t.f->nlines >= t.f->maxlines)
		return;
	frinsert(t.f, t.r + t.org + t.f->nchars, t.r + t.nr, t.f->nchars);
}

void
updatesel(void)
{
	Frame *f;
	uint n;

	f = t.f;
	if(t.org+f->p0 == t.q0 && t.org+f->p1 == t.q1)
		return;

	n = t.f->nchars;

	frdrawsel(f, frptofchar(f, f->p0), f->p0, f->p1, 0);
	if (t.q0 >= t.org)
		f->p0 = t.q0-t.org;
	else
		f->p0 = 0;
	if(f->p0 > n)
		f->p0 = n;
	if (t.q1 >= t.org)
		f->p1 = t.q1-t.org;
	else
		f->p1 = 0;
	if(f->p1 > n)
		f->p1 = n;
	frdrawsel(f, frptofchar(f, f->p0), f->p0, f->p1, 1);

/*
	if(t.qh<=t.org+t.f.nchars && t.cwqueue != 0)
		t.cwqueue->wakeup <-= 0;
*/

	tcheck();
}

void
show(uint q0)
{
	int nl;
	uint q, oq;

	if(cansee(q0))
		return;
	
	if (q0<t.org)
		nl = t.f->maxlines/5;
	else
		nl = 4*t.f->maxlines/5;
	q = backnl(q0, nl);
	/* avoid going in the wrong direction */
	if (q0>t.org && q<t.org)
		q = t.org;
	setorigin(q, 0);
	/* keep trying until q0 is on the screen */
	while(!cansee(q0)) {
		assert(q0 >= t.org);
		oq = q;
		q = line2q(t.f->maxlines-nl);
		assert(q > oq);
		setorigin(q, 1);
	}
}

int
cansee(uint q0)
{
	uint qe;

	qe = t.org+t.f->nchars;

	if(q0>=t.org && q0 < qe)
		return 1;
	if (q0 != qe)
		return 0;
	if (t.f->nlines < t.f->maxlines)
		return 1;
	if (q0 > 0 && t.r[t.nr-1] == '\n')
		return 0;
	return 1;
}


void
setorigin(uint org, int exact)
{
	int i, a;
	uint n;
	
	if(org>0 && !exact){
		/* try and start after a newline */
		/* don't try harder than 256 chars */
		for(i=0; i<256 && org<t.nr; i++){
			if(t.r[org-1] == '\n')
				break;
			org++;
		}
	}
	a = org-t.org;

	if(a>=0 && a<t.f->nchars)
		frdelete(t.f, 0, a);
	else if(a<0 && -a<100*t.f->maxlines){
		n = t.org - org;
		frinsert(t.f, t.r+org, t.r+org+n, 0);
	}else
		frdelete(t.f, 0, t.f->nchars);
	t.org = org;
	fill();
	updatesel();
}


uint
line2q(uint n)
{
	Frame *f;

	f = t.f;
	return frcharofpt(f, Pt(f->r.min.x, f->r.min.y + n*font->height))+t.org;
}

uint
backnl(uint p, uint n)
{
	int i, j;

	for (i = n;; i--) {
		/* at 256 chars, call it a line anyway */
		for(j=256; --j>0 && p>0; p--)
			if(t.r[p-1]=='\n')
				break;
		if (p == 0 || i == 0)
			return p;
		p--;
	}
}

void
addraw(Rune *r, int nr)
{
	t.raw = runerealloc(t.raw, t.nraw+nr);
	runemove(t.raw+t.nraw, r, nr);
	t.nraw += nr;
/*
	if(t.crqueue != nil)
		t.crqueue->wakeup <-= 0;
*/	
}


Rune left1[] =  { '{', '[', '(', '<', 0xab, 0 };
Rune right1[] = { '}', ']', ')', '>', 0xbb, 0 };
Rune left2[] =  { '\n', 0 };
Rune left3[] =  { '\'', '"', '`', 0 };

Rune *left[] = {
	left1,
	left2,
	left3,
	0
};

Rune *right[] = {
	right1,
	left2,
	left3,
	0
};

void
doubleclick(uint *q0, uint *q1)
{
	int c, i;
	Rune *r, *l, *p;
	uint q;

	for(i=0; left[i]!=0; i++){
		q = *q0;
		l = left[i];
		r = right[i];
		/* try matching character to left, looking right */
		if(q == 0)
			c = '\n';
		else
			c = t.r[q-1];
		p = strrune(l, c);
		if(p != 0){
			if(clickmatch(c, r[p-l], 1, &q))
				*q1 = q-(c!='\n');
			return;
		}
		/* try matching character to right, looking left */
		if(q == t.nr)
			c = '\n';
		else
			c = t.r[q];
		p = strrune(r, c);
		if(p != 0){
			if(clickmatch(c, l[p-r], -1, &q)){
				*q1 = *q0+(*q0<t.nr && c=='\n');
				*q0 = q;
				if(c!='\n' || q!=0 || t.r[0]=='\n')
					(*q0)++;
			}
			return;
		}
	}
	/* try filling out word to right */
	while(*q1<t.nr && isexpand(t.r[*q1]))
		(*q1)++;
	/* try filling out word to left */
	while(*q0>0 && isexpand(t.r[*q0-1]))
		(*q0)--;
}

int
clickmatch(int cl, int cr, int dir, uint *q)
{
	Rune c;
	int nest;

	nest = 1;
	for(;;){
		if(dir > 0){
			if(*q == t.nr)
				break;
			c = t.r[*q];
			(*q)++;
		}else{
			if(*q == 0)
				break;
			(*q)--;
			c = t.r[*q];
		}
		if(c == cr){
			if(--nest==0)
				return 1;
		}else if(c == cl)
			nest++;
	}
	return cl=='\n' && nest==1;
}

void
tcheck(void)
{
	Frame *f;
		
	f = t.f;

	assert(t.q0 <= t.q1 && t.q1 <= t.nr);
	assert(t.org <= t.nr && t.qh <= t.nr);
	assert(f->p0 <= f->p1 && f->p1 <= f->nchars);
	assert(t.org + f->nchars <= t.nr);
	assert(t.org+f->nchars==t.nr || (f->nlines >= f->maxlines));
}

Rune*
strrune(Rune *s, Rune c)
{
	Rune c1;

	if(c == 0) {
		while(*s++)
			;
		return s-1;
	}

	while(c1 = *s++)
		if(c1 == c)
			return s-1;
	return 0;
}

void
scrdraw(void)
{
	Rectangle r, r1, r2;
	static Image *scrx;

	r = scrollr;
	r.min.x += 1;	/* border between margin and bar */
	r1 = r;
	if(scrx==0 || scrx->r.max.y < r.max.y){
		if(scrx)
			freeimage(scrx);
		scrx = allocimage(display, Rect(0, 0, 32, r.max.y), screen->chan, 1, DPaleyellow);
		if(scrx == 0)
			sysfatal("scroll balloc");
	}
	r1.min.x = 0;
	r1.max.x = Dx(r);
	r2 = scrpos(r1, t.org, t.org+t.f->nchars, t.nr);
	if(!eqrect(r2, lastsr)){
		lastsr = r2;
		draw(scrx, r1, cols[BORD], nil, ZP);
		draw(scrx, r2, cols[BACK], nil, r2.min);
//		r2 = r1;
//		r2.min.x = r2.max.x-1;
//		draw(scrx, r2, cols[BORD], nil, ZP);
		draw(screen, r, scrx, nil, r1.min);
	}
}

Rectangle
scrpos(Rectangle r, ulong p0, ulong p1, ulong tot)
{
	long h;
	Rectangle q;

	q = insetrect(r, 1);
	h = q.max.y-q.min.y;
	if(tot == 0)
		return q;
	if(tot > 1024L*1024L)
		tot >>= 10, p0 >>= 10, p1 >>= 10;
	if(p0 > 0)
		q.min.y += h*p0/tot;
	if(p1 < tot)
		q.max.y -= h*(tot-p1)/tot;
	if(q.max.y < q.min.y+2){
		if(q.min.y+2 <= r.max.y)
			q.max.y = q.min.y+2;
		else
			q.min.y = q.max.y-2;
	}
	return q;
}

void
scroll(int but)
{
	uint p0, oldp0;
	Rectangle s;
	int x, y, my, h, first, exact;

	s = insetrect(scrollr, 1);
	h = s.max.y-s.min.y;
	x = (s.min.x+s.max.x)/2;
	oldp0 = ~0;
	first = 1;
	do{
		if(t.m.xy.x<s.min.x || s.max.x<=t.m.xy.x){
			readmouse(mc);
			t.m = mc->m;
		}else{
			my = t.m.xy.y;
			if(my < s.min.y)
				my = s.min.y;
			if(my >= s.max.y)
				my = s.max.y;
//			if(!eqpt(t.m.xy, Pt(x, my)))
//				cursorset(Pt(x, my));
			exact = 1;
			if(but == 2){
				y = my;
				if(y > s.max.y-2)
					y = s.max.y-2;
				if(t.nr > 1024*1024)
					p0 = ((t.nr>>10)*(y-s.min.y)/h)<<10;
				else
					p0 = t.nr*(y-s.min.y)/h;
				exact = 0;
			} else if(but == 1)
				p0 = backnl(t.org, (my-s.min.y)/font->height);
			else 
				p0 = t.org+frcharofpt(t.f, Pt(s.max.x, my));

			if(oldp0 != p0)
				setorigin(p0, exact);
			oldp0 = p0;
			scrdraw();
			readmouse(mc);
			t.m = mc->m;
		}
	}while(t.m.buttons & (1<<(but-1)));
}

void
plumbstart(void)
{
	if((plumbfd = plumbopen("send", OWRITE)) < 0)
		fprint(2, "9term: plumbopen: %r\n");
}

void
plumb(uint q0, uint q1)
{
	Plumbmsg *pm;
	char *p;
	int i, p0, n;
	char cbuf[100];

	pm = malloc(sizeof(Plumbmsg));
	pm->src = strdup("9term");
	pm->dst = 0;
	pm->wdir = strdup(wdir);
	pm->type = strdup("text");
	pm->data = nil;
	if(q1 > q0)
		pm->attr = nil;
	else{
		p0 = q0;
		wordclick(&q0, &q1);
		sprint(cbuf, "click=%d", p0-q0);
		pm->attr = plumbunpackattr(cbuf);
	}
	if(q0==q1){
		plumbfree(pm);
		return;
	}
	pm->data = malloc(SnarfSize);
	n = q1 - q0;
	for(i=0,p=pm->data; i<n && p < pm->data + SnarfSize-UTFmax; i++)
		p += runetochar(p, t.r+q0+i);
	*p = '\0';
	pm->ndata = strlen(pm->data);
	if(plumbsend(plumbfd, pm) < 0){
		setcursor(mc, &query);
		sleep(500);
		if(holdon)
			setcursor(mc, &whitearrow);
		else
			setcursor(mc, nil);
	}
	plumbfree(pm);
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
	char *p;
	
	er = sr+n;
	for(r=er-1; r>=sr; r--)
		if(*r == '\007')
			break;
	if(r < sr)
		return n;

	el = r+1;
	if(el-sr > sizeof wdir)
		sr = el - sizeof wdir;
	for(sl=el-3; sl>=sr; sl--)
		if(sl[0]=='\033' && sl[1]==']' && sl[2]==';')
			break;
	if(sl < sr)
		return n;

	snprint(wdir, sizeof wdir, "%.*S", (el-1)-(sl+3), sl+3);
	drawsetlabel(wdir);

	/* remove trailing /-sysname if present */
	p = strrchr(wdir, '/');
	if(p && *(p+1) == '-'){
		if(p == wdir)
			p++;
		*p = 0;
	}

	runemove(sl, el, er-el);
	n -= (el-sl);
	return n;
}

int
rawon(void)
{
	return !cooked && !isecho(sfd);
}

/*
 * Clumsy hack to make " and "" work.
 * Then again, what's not a clumsy hack here in Unix land?
 */

char adir[100];
int afd;

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

	USED(arg);
	for(;;){
		fd = listen(adir, dir);
		if(fd < 0){
			close(afd);
			return;
		}
		proccreate(textthread, (void*)fd, STACK);
	}
}

void
textthread(void *arg)
{
	int fd, i, x, n, end;
	Rune r;
	char buf[4096], *p, *ep;

	fd = (int)arg;
	p = buf;
	ep = buf+sizeof buf;
	end = t.org+t.nr;	/* avoid possible output loop */
	for(i=t.org;; i++){
		if(i >= end || ep-p < UTFmax){
			for(x=0; x<p-buf; x+=n)
				if((n = write(fd, buf+x, (p-x)-buf)) <= 0)
					goto break2;
			
			if(i >= end)
				break;
			p = buf;
		}
		if(i < t.org)
			i = t.org;
		r = t.r[i-t.org];
		if(r < Runeself)
			*p++ = r;
		else
			p += runetochar(p, &r);
	}
break2:
	close(fd);
}
