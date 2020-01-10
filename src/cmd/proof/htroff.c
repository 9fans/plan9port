#include	<u.h>
#include	<libc.h>
#include	<draw.h>
#include	<cursor.h>
#include	<event.h>
#include	<bio.h>
#include	"proof.h"

int	res;
int	hpos;
int	vpos;
int	DIV = 11;

Point offset;
Point xyoffset = { 0,0 };

Rectangle	view[MAXVIEW];
Rectangle	bound[MAXVIEW];		/* extreme points */
int	nview = 1;

int	lastp;	/* last page number we were on */

#define	NPAGENUMS	200
struct pagenum {
	int	num;
	long	adr;
} pagenums[NPAGENUMS];
int	npagenums;

int	curfont, cursize;

char	*getcmdstr(void);

static void	initpage(void);
static void	view_setup(int);
static Point	scale(Point);
static void	clearview(Rectangle);
static int	addpage(int);
static void	spline(Image *, int, Point *);
static int	skipto(int, int);
static void	wiggly(int);
static void	devcntrl(void);
static void	eatline(void);
static int	getn(void);
static int	botpage(int);
static void	getstr(char *);
/*
static void	getutf(char *);
*/

#define Do screen->r.min
#define Dc screen->r.max

/* declarations and definitions of font stuff are in font.c and main.c */

static void
initpage(void)
{
	int i;

	view_setup(nview);
	for (i = 0; i < nview-1; i++)
		draw(screen, view[i], screen, nil, view[i+1].min);
	clearview(view[nview-1]);
	offset = view[nview-1].min;
	vpos = 0;
}

static void
view_setup(int n)
{
	int i, j, v, dx, dy, r, c;

	switch (n) {
	case 1: r = 1; c = 1; break;
	case 2: r = 1; c = 2; break;
	case 3: r = 1; c = 3; break;
	case 4: r = 2; c = 2; break;
	case 5: case 6: r = 2; c = 3; break;
	case 7: case 8: case 9: r = 3; c = 3; break;
	default: r = (n+2)/3; c = 3; break; /* finking out */
	}
	dx = (Dc.x - Do.x) / c;
	dy = (Dc.y - Do.y) / r;
	v = 0;
	for (i = 0; i < r && v < n; i++)
		for (j = 0; j < c && v < n; j++) {
			view[v] = screen->r;
			view[v].min.x = Do.x + j * dx;
			view[v].max.x = Do.x + (j+1) * dx;
			view[v].min.y = Do.y + i * dy;
			view[v].max.y = Do.y + (i+1) * dy;
			v++;
		}
}

static void
clearview(Rectangle r)
{
	draw(screen, r, display->white, nil, r.min);
}

int resized;
void eresized(int new)
{
	/* this is called if we are resized */
	if(new && getwindow(display, Refnone) < 0)
		drawerror(display, "can't reattach to window");
	initpage();
	resized = 1;
}

static Point
scale(Point p)
{
	p.x /= DIV;
	p.y /= DIV;
	return addpt(xyoffset, addpt(offset,p));
}

static int
addpage(int n)
{
	int i;

	for (i = 0; i < npagenums; i++)
		if (n == pagenums[i].num)
			return i;
	if (npagenums < NPAGENUMS-1) {
		pagenums[npagenums].num = n;
		pagenums[npagenums].adr = offsetc();
		npagenums++;
	}
	return npagenums;
}

void
readpage(void)
{
	int c, i, a, alpha, phi;
	static int first = 0;
	int m, n, gonow = 1;
	Rune r[32], t;
	Point p,q,qq;

	offset = screen->clipr.min;
	esetcursor(&deadmouse);
	while (gonow)
	{
		c = getc();
		switch (c)
		{
		case -1:
			esetcursor(0);
			if (botpage(lastp+1)) {
				initpage();
				break;
			}
			exits(0);
		case 'p':	/* new page */
			lastp = getn();
			addpage(lastp);
			if (first++ > 0) {
				esetcursor(0);
				botpage(lastp);
				esetcursor(&deadmouse);
			}
			initpage();
			break;
		case '\n':	/* when input is text */
		case ' ':
		case 0:		/* occasional noise creeps in */
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/* two motion digits plus a character */
			hpos += (c-'0')*10 + getc()-'0';

		/* FALLS THROUGH */
		case 'c':	/* single ascii character */
			r[0] = getrune();
			r[1] = 0;
			dochar(r);
			break;

		case 'C':
			for(i=0; ; i++){
				t = getrune();
				if(isspace(t))
					break;
				r[i] = t;
			}
			r[i] = 0;
			dochar(r);
			break;

		case 'N':
			r[0] = getn();
			r[1] = 0;
			dochar(r);
			break;

		case 'D':	/* draw function */
			switch (getc())
			{
			case 'l':	/* draw a line */
				n = getn();
				m = getn();
				p = Pt(hpos,vpos);
				q = addpt(p, Pt(n,m));
				hpos += n;
				vpos += m;
				line(screen, scale(p), scale(q), 0, 0, 0, display->black, ZP);
				break;
			case 'c':	/* circle */
				/*nop*/
				m = getn()/2;
				p = Pt(hpos+m,vpos);
				hpos += 2*m;
				ellipse(screen, scale(p), m/DIV, m/DIV, 0, display->black, ZP);
				/* p=currentpt; p.x+=dmap(m/2);circle bp,p,a,ONES,Mode*/
				break;
			case 'e':	/* ellipse */
				/*nop*/
				m = getn()/2;
				n = getn()/2;
				p = Pt(hpos+m,vpos);
				hpos += 2*m;
				ellipse(screen, scale(p), m/DIV, n/DIV, 0, display->black, ZP);
				break;
			case 'a':	/* arc */
				p = scale(Pt(hpos,vpos));
				n = getn();
				m = getn();
				hpos += n;
				vpos += m;
				q = scale(Pt(hpos,vpos));
				n = getn();
				m = getn();
				hpos += n;
				vpos += m;
				qq = scale(Pt(hpos,vpos));
				/*
				  * tricky: convert from 3-point clockwise to
				  * center, angle1, delta-angle counterclockwise.
				 */
				a = hypot(qq.x-q.x, qq.y-q.y);
				phi = atan2(q.y-p.y, p.x-q.x)*180./PI;
				alpha = atan2(q.y-qq.y, qq.x-q.x)*180./PI - phi;
				if(alpha < 0)
					alpha += 360;
				arc(screen, q, a, a, 0, display->black, ZP, phi, alpha);
				break;
			case '~':	/* wiggly line */
				wiggly(0);
				break;
			default:
				break;
			}
			eatline();
			break;
		case 's':
			n = getn();	/* ignore fractional sizes */
			if (cursize == n)
				break;
			cursize = n;
			if (cursize >= NFONT)
				cursize = NFONT-1;
			break;
		case 'f':
			curfont = getn();
			break;
		case 'H':	/* absolute horizontal motion */
			hpos = getn();
			break;
		case 'h':	/* relative horizontal motion */
			hpos += getn();
			break;
		case 'w':	/* word space */
			break;
		case 'V':
			vpos = getn();
			break;
		case 'v':
			vpos += getn();
			break;
		case '#':	/* comment */
		case 'n':	/* end of line */
			eatline();
			break;
		case 'x':	/* device control */
			devcntrl();
			break;
		default:
			fprint(2, "unknown input character %o %c at offset %lud\n", c, c, offsetc());
			exits("bad char");
		}
	}
	esetcursor(0);
}

static void
spline(Image *b, int n, Point *pp)
{
	long w, t1, t2, t3, fac=1000;
	int i, j, steps=10;
	Point p, q;

	for (i = n; i > 0; i--)
		pp[i] = pp[i-1];
	pp[n+1] = pp[n];
	n += 2;
	p = pp[0];
	for(i = 0; i < n-2; i++)
	{
		for(j = 0; j < steps; j++)
		{
			w = fac * j / steps;
			t1 = w * w / (2 * fac);
			w = w - fac/2;
			t2 = 3*fac/4 - w * w / fac;
			w = w - fac/2;
			t3 = w * w / (2*fac);
			q.x = (t1*pp[i+2].x + t2*pp[i+1].x +
				t3*pp[i].x + fac/2) / fac;
			q.y = (t1*pp[i+2].y + t2*pp[i+1].y +
				t3*pp[i].y + fac/2) / fac;
			line(b, p, q, 0, 0, 0, display->black, ZP);
			p = q;
		}
	}
}

/* Have to parse skipped pages, to find out what fonts are loaded. */
static int
skipto(int gotop, int curp)
{
	char *p;
	int i;

	if (gotop == curp)
		return 1;
	for (i = 0; i < npagenums; i++)
		if (pagenums[i].num == gotop) {
			if (seekc(pagenums[i].adr) == Beof) {
				fprint(2, "can't rewind input\n");
				return 0;
			}
			return 1;
		}
	if (gotop <= curp) {
	    restart:
		if (seekc(0) == Beof) {
			fprint(2, "can't rewind input\n");
			return 0;
		}
	}
	for(;;){
		p = rdlinec();
		if (p == 0) {
			if(gotop>curp){
				gotop = curp;
				goto restart;
			}
			return 0;
		} else if (*p == 'p') {
			lastp = curp = atoi(p+1);
			addpage(lastp);	/* maybe 1 too high */
			if (curp>=gotop)
				return 1;
		}
	}
}

static void
wiggly(int skip)
{
	Point p[300];
	int c,i,n;
	for (n = 1; (c = getc()) != '\n' && c>=0; n++) {
		ungetc();
		p[n].x = getn();
		p[n].y = getn();
	}
	p[0] = Pt(hpos, vpos);
	for (i = 1; i < n; i++)
		p[i] = addpt(p[i],p[i-1]);
	hpos = p[n-1].x;
	vpos = p[n-1].y;
	for (i = 0; i < n; i++)
		p[i] = scale(p[i]);
	if (!skip)
		spline(screen,n,p);
}

static void
devcntrl(void)	/* interpret device control functions */
{
        char str[80];
	int n;

	getstr(str);
	switch (str[0]) {	/* crude for now */
	case 'i':	/* initialize */
		break;
	case 'T':	/* device name */
		getstr(devname);
		break;
	case 't':	/* trailer */
		break;
	case 'p':	/* pause -- can restart */
		break;
	case 's':	/* stop */
		break;
	case 'r':	/* resolution assumed when prepared */
		res=getn();
		DIV = floor(.5 + res/(100.0*mag));
		if (DIV < 1)
			DIV = 1;
		mag = res/(100.0*DIV); /* adjust mag according to DIV coarseness */
		break;
	case 'f':	/* font used */
		n = getn();
		getstr(str);
		loadfontname(n, str);
		break;
	/* these don't belong here... */
	case 'H':	/* char height */
		break;
	case 'S':	/* slant */
		break;
	case 'X':
		break;
	}
	eatline();
}

int
isspace(int c)
{
	return c==' ' || c=='\t' || c=='\n';
}

static void
getstr(char *is)
{
	uchar *s = (uchar *) is;

	for (*s = getc(); isspace(*s); *s = getc())
		;
	for (; !isspace(*s); *++s = getc())
		;
	ungetc();
	*s = 0;
}

#if 0
static void
getutf(char *s)		/* get next utf char, as bytes */
{
	int c, i;

	for (i=0;;) {
		c = getc();
		if (c < 0)
			return;
		s[i++] = c;

		if (fullrune(s, i)) {
			s[i] = 0;
			return;
		}
	}
}
#endif

static void
eatline(void)
{
	int c;

	while ((c=getc()) != '\n' && c >= 0)
		;
}

static int
getn(void)
{
	int n, c, sign;

	while (c = getc())
		if (!isspace(c))
			break;
	if(c == '-'){
		sign = -1;
		c = getc();
	}else
		sign = 1;
	for (n = 0; '0'<=c && c<='9'; c = getc())
		n = n*10 + c - '0';
	while (c == ' ')
		c = getc();
	ungetc();
	return(n*sign);
}

static int
botpage(int np)	/* called at bottom of page np-1 == top of page np */
{
	char *p;
	int n;

	while (p = getcmdstr()) {
		if (*p == '\0')
			return 0;
		if (*p == 'q')
			exits(p);
		if (*p == 'c')		/* nop */
			continue;
		if (*p == 'm') {
			mag = atof(p+1);
			if (mag <= .1 || mag >= 10)
				mag = DEFMAG;
			allfree();	/* zap fonts */
			DIV = floor(.5 + res/(100.0*mag));
			if (DIV < 1)
				DIV = 1;
			mag = res/(100.0*DIV);
			return skipto(np-1, np);	/* reprint the page */
		}
		if (*p == 'x') {
			xyoffset.x += atoi(p+1)*100;
			skipto(np-1, np);
			return 1;
		}
		if (*p == 'y') {
			xyoffset.y += atoi(p+1)*100;
			skipto(np-1, np);
			return 1;
		}
		if (*p == '/') {	/* divide into n pieces */
			nview = atoi(p+1);
			if (nview < 1)
				nview = 1;
			else if (nview > MAXVIEW)
				nview = MAXVIEW;
			return skipto(np-1, np);
		}
		if (*p == 'p') {
			if (p[1] == '\0'){	/* bare 'p' */
				if(skipto(np-1, np))
					return 1;
				continue;
			}
			p++;
		}
		if ('0'<=*p && *p<='9') {
			n = atoi(p);
			if(skipto(n, np))
				return 1;
			continue;
		}
		if (*p == '-' || *p == '+') {
			n = atoi(p);
			if (n == 0)
				n = *p == '-' ? -1 : 1;
			if(skipto(np - 1 + n, np))
				return 1;
			continue;
		}
		if (*p == 'd') {
			dbg = 1 - dbg;
			continue;
		}

		fprint(2, "illegal;  try q, 17, +2, -1, p, m.7, /2, x1, y-.5 or return\n");
	}
	return 0;
}
