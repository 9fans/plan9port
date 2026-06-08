/*
 * frametest: oracle harness for the Go port of libframe (go/frame).
 *
 * Reads a line-based script from stdin, drives a Frame rendered into an
 * offscreen image, and writes a deterministic textual dump of the frame's
 * box list and metrics to stdout. The Go frame package runs the same script
 * and must produce byte-identical output. See go/frame/HACKING.md.
 *
 * Script commands (one per line):
 *	font PATH		load the font used for all subsequent layout
 *	init W H		alloc a W x H offscreen image and frinit into it
 *	insert Q TEXT		frinsert TEXT (C escapes \t \n \\) at rune offset Q
 *	delete Q0 Q1		frdelete [Q0,Q1)
 *	select X,Y,B ...	frselect driven by the listed mouse samples
 *				(first is the initial button-down state)
 *	ptofchar Q		print frptofchar(Q)
 *	charofpt X Y		print frcharofpt(Pt(X,Y))
 *	redraw			frredraw
 *	clear			frclear (keeping the font)
 *	dump			emit frame metrics + box list
 *	#...			comment (ignored)
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <frame.h>

Frame	f;
Image	*cols[NCOL];
Font	*thefont;
int	inited;

int	parsesamples(char*, Mouse*, int);
void	cmdselect(char*);
void	cmdselscroll(char*);
void	cmddump(void);

void
usage(void)
{
	fprint(2, "usage: frametest <script\n");
	threadexitsall("usage");
}

/* parse a Go-escaped string in place; returns number of runes written to r */
int
unescape(char *s, Rune *r)
{
	int n;
	Rune rune;

	if(*s++ != '"')
		sysfatal("bad quoted string");

	n = 0;
	while(*s && *s != '"'){
		if(*s == '\\'){
			s++;
			switch(*s){
			case 't': r[n++] = '\t'; s++; break;
			case 'n': r[n++] = '\n'; s++; break;
			case '\\': r[n++] = '\\'; s++; break;
			case 0: r[n++] = '\\'; break;
			default: r[n++] = *s++; break;
			}
		}else
			s += chartorune(&rune, s), r[n++] = rune;
	}
	if(*s++ != '"')
		sysfatal("bad quoted string");
	return n;
}

void
setcols(void)
{
	cols[BACK] = display->white;
	cols[HIGH] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPaleyellow);
	cols[BORD] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DYellowgreen);
	cols[TEXT] = display->black;
	cols[HTEXT] = display->black;
}

void
cmdinit(int w, int h)
{
	Image *b;

	b = allocimage(display, Rect(0,0,w,h), screen->chan, 0, DWhite);
	if(b == nil)
		sysfatal("allocimage: %r");
	frclear(&f, 1);
	memset(&f, 0, sizeof f);
	if(thefont == nil)
		thefont = display->defaultfont;
	frinit(&f, b->r, thefont, b, cols);
	inited = 1;
}

void
dumpbox(int i)
{
	Frbox *b;

	b = &f.box[i];
	if(b->nrune < 0)
		print("box[%d] brk wid=%ld minwid=%d bc=%d\n", i, b->wid, b->minwid, b->bc);
	else
		print("box[%d] run wid=%ld nrune=%ld \"%s\"\n", i, b->wid, b->nrune, (char*)b->ptr);
}

/* select X,Y,B X,Y,B ...: drive frselect with the listed mouse samples */
void
cmdselect(char *p)
{
	Mousectl mc;
	Mouse samples[256];
	int i, ns;

	ns = parsesamples(p, samples, nelem(samples));
	if(ns == 0)
		return;
	memset(&mc, 0, sizeof mc);
	mc.display = display;
	mc.c = chancreate(sizeof(Mouse), ns);
	mc.m = samples[0];
	for(i=1; i<ns; i++)
		send(mc.c, &samples[i]);
	frselect(&f, &mc);
	chanfree(mc.c);
}

/*
 * A self-terminating scroll callback for testing frselect's scroll branch:
 * after two invocations it zeroes the mouse buttons so the frselect loop
 * exits (a live scroll would instead move text under the held button).
 */
Mousectl	*gmc;
int		gscrolln;

void
testscroll(Frame *fr, int n)
{
	USED(fr);
	USED(n);
	if(++gscrolln >= 2)
		gmc->m.buttons = 0;
}

int
parsesamples(char *p, Mouse *samples, int max)
{
	char *toks[256], *q;
	int i, ns, nt;

	nt = tokenize(p, toks, nelem(toks));
	ns = 0;
	for(i=0; i<nt && ns<max; i++){
		q = toks[i];
		memset(&samples[ns], 0, sizeof samples[ns]);
		samples[ns].xy.x = strtol(q, &q, 10);
		if(*q == ',')
			q++;
		samples[ns].xy.y = strtol(q, &q, 10);
		if(*q == ',')
			q++;
		samples[ns].buttons = strtol(q, &q, 10);
		ns++;
	}
	return ns;
}

/* selscroll X,Y,B ...: like select, but with a terminating scroll callback */
void
cmdselscroll(char *p)
{
	Mousectl mc;
	Mouse samples[256];
	int i, ns;

	ns = parsesamples(p, samples, nelem(samples));
	if(ns == 0)
		return;
	memset(&mc, 0, sizeof mc);
	mc.display = display;
	mc.c = chancreate(sizeof(Mouse), ns);
	mc.m = samples[0];
	for(i=1; i<ns; i++)
		send(mc.c, &samples[i]);
	gmc = &mc;
	gscrolln = 0;
	f.scroll = testscroll;
	frselect(&f, &mc);
	f.scroll = nil;
	chanfree(mc.c);
}

void
cmddump(void)
{
	int i;

	print("frame nbox=%d nchars=%d nlines=%d maxlines=%d lastlinefull=%d modified=%d p0=%lud p1=%lud\n",
		f.nbox, f.nchars, f.nlines, f.maxlines, f.lastlinefull, f.modified, f.p0, f.p1);
	print("  r=(%d %d)(%d %d) entire=(%d %d)(%d %d) maxtab=%d fh=%d\n",
		f.r.min.x, f.r.min.y, f.r.max.x, f.r.max.y,
		f.entire.min.x, f.entire.min.y, f.entire.max.x, f.entire.max.y,
		f.maxtab, f.font->height);
	for(i=0; i<f.nbox; i++)
		dumpbox(i);
}

void
flush(void)
{
	draw(screen, screen->r, display->white, nil, ZP);
	draw(screen, rectaddpt(f.r, screen->r.min), f.b, nil, f.r.min);
	flushimage(display, 1);
}

void
threadmain(int argc, char *argv[])
{
	Biobuf *b;
	char *line, *args[4], *p;
	Rune *rbuf;
	int n, q0, q1, w, h;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	/* fixed window size so geometry doesn't depend on the screen size */
	winsize = "600x400";
	if(initdraw(nil, nil, "frametest") < 0)
		sysfatal("initdraw: %r");
	setcols();

	rbuf = malloc(64*1024*sizeof(Rune));
	b = Bfdopen(0, OREAD);
	if(b == nil)
		sysfatal("Bfdopen: %r");

	while((line = Brdline(b, '\n')) != nil){
		line[Blinelen(b)-1] = 0;
		if(line[0] == '#' || line[0] == 0)
			continue;
		/* split off command word; keep the remainder intact for insert */
		p = line;
		while(*p && *p != ' ')
			p++;
		if(*p)
			*p++ = 0;
		while(*p == ' ')
			p++;
		if(strcmp(line, "charofpt") == 0){
			Point pt;
			ulong cp;
			if(tokenize(p, args, nelem(args)) != 2)
				sysfatal("charofpt X Y");
			pt = Pt(atoi(args[0]), atoi(args[1]));
			cp = frcharofpt(&f, pt);
			print("charofpt %d %d = %lud\n", pt.x, pt.y, cp);
		}else if(strcmp(line, "clear") == 0){
			frclear(&f, 0);
			flush();
		}else if(strcmp(line, "clearfont") == 0){
			frclear(&f, 1);
			flush();
		}else if(strcmp(line, "delete") == 0){
			if(tokenize(p, args, nelem(args)) != 2)
				sysfatal("delete Q0 Q1");
			q0 = atoi(args[0]);
			q1 = atoi(args[1]);
			frdelete(&f, q0, q1);
			flush();
		}else if(strcmp(line, "dump") == 0){
			cmddump();
		}else if(strcmp(line, "font") == 0){
			thefont = openfont(display, p);
			if(thefont == nil)
				sysfatal("openfont %s: %r", p);
		}else if(strcmp(line, "init") == 0){
			if(tokenize(p, args, nelem(args)) != 2)
				sysfatal("init W H");
			w = atoi(args[0]);
			h = atoi(args[1]);
			cmdinit(w, h);
			flush();
		}else if(strcmp(line, "inittick") == 0){
			/* re-init tick images (mirrors a font-size change) */
			frinittick(&f);
			flush();
		}else if(strcmp(line, "insert") == 0){
			q0 = strtol(p, &p, 10);
			while(*p == ' ')
				p++;
			n = unescape(p, rbuf);
			frinsert(&f, rbuf, rbuf+n, q0);
			flush();
		}else if(strcmp(line, "output") == 0){
			flush();
		}else if(strcmp(line, "ptofchar") == 0){
			Point pt;
			q0 = atoi(p);
			pt = frptofchar(&f, q0);
			print("ptofchar %d = (%d %d)\n", q0, pt.x, pt.y);
		}else if(strcmp(line, "redraw") == 0){
			frredraw(&f);
			flush();
		}else if(strcmp(line, "select") == 0){
			cmdselect(p);
			flush();
		}else if(strcmp(line, "selscroll") == 0){
			cmdselscroll(p);
			flush();
		}else
			sysfatal("unknown command: %s", line);
	}
	threadexitsall(nil);
}
