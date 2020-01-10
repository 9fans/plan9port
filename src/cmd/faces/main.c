#include <u.h>
#include <libc.h>
#include <draw.h>
#include <plumb.h>
#include <regexp.h>
#include <bio.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <9pclient.h>
#include "faces.h"

int	history = 0;	/* use old interface, showing history of mailbox rather than current state */
int	initload = 0;	/* initialize program with contents of mail box */

enum
{
	Facesep = 6,	/* must be even to avoid damaging background stipple */
	Infolines = 9,

	HhmmTime = 18*60*60,	/* max age of face to display hh:mm time */

	STACK = 32768
};

enum
{
	Mainp,
	Timep,
	Mousep,
	Resizep,
	NPROC
};

char *procnames[] = {
	"main",
	"time",
	"mouse",
	"resize"
};

Rectangle leftright = {0, 0, 20, 15};

uchar leftdata[] = {
	0x00, 0x80, 0x00, 0x01, 0x80, 0x00, 0x03, 0x80,
	0x00, 0x07, 0x80, 0x00, 0x0f, 0x00, 0x00, 0x1f,
	0xff, 0xf0, 0x3f, 0xff, 0xf0, 0xff, 0xff, 0xf0,
	0x3f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x0f, 0x00,
	0x00, 0x07, 0x80, 0x00, 0x03, 0x80, 0x00, 0x01,
	0x80, 0x00, 0x00, 0x80, 0x00
};

uchar rightdata[] = {
	0x00, 0x10, 0x00, 0x00, 0x18, 0x00, 0x00, 0x1c,
	0x00, 0x00, 0x1e, 0x00, 0x00, 0x0f, 0x00, 0xff,
	0xff, 0x80, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xf0,
	0xff, 0xff, 0xc0, 0xff, 0xff, 0x80, 0x00, 0x0f,
	0x00, 0x00, 0x1e, 0x00, 0x00, 0x1c, 0x00, 0x00,
	0x18, 0x00, 0x00, 0x10, 0x00
};

CFsys	*mailfs;
Mousectl	*mousectl;
Image	*blue;		/* full arrow */
Image	*bgrnd;		/* pale blue background color */
Image	*left;		/* left-pointing arrow mask */
Image	*right;		/* right-pointing arrow mask */
Font	*tinyfont;
Font	*mediumfont;
Font	*datefont;
int	first, last;	/* first and last visible face; last is first invisible */
int	nfaces;
int	mousefd;
int	nacross;
int	ndown;

char	date[64];
Face	**faces;
char	*maildir = "mbox";
ulong	now;

Point	datep = { 8, 6 };
Point	facep = { 8, 6+0+4 };	/* 0 updated to datefont->height in init() */
Point	enddate;			/* where date ends on display; used to place arrows */
Rectangle	leftr;			/* location of left arrow on display */
Rectangle	rightr;		/* location of right arrow on display */
void updatetimes(void);
void eresized(int);

void
setdate(void)
{
	now = time(nil);
	strcpy(date, ctime(now));
	date[4+4+3+5] = '\0';	/* change from Thu Jul 22 14:28:43 EDT 1999\n to Thu Jul 22 14:28 */
}

void
init(void)
{
	mailfs = nsmount("mail", nil);
	if(mailfs == nil)
		sysfatal("mount mail: %r");
	mousectl = initmouse(nil, screen);
	if(mousectl == nil)
		sysfatal("initmouse: %r");
	initplumb();

	/* make background color */
	bgrnd = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DWhite);
	blue = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x008888FF);	/* blue-green */
	left = allocimage(display, leftright, GREY1, 0, DWhite);
	right = allocimage(display, leftright, GREY1, 0, DWhite);
	if(bgrnd==nil || blue==nil || left==nil || right==nil){
		fprint(2, "faces: can't create images: %r\n");
		threadexitsall("image");
	}

	loadimage(left, leftright, leftdata, sizeof leftdata);
	loadimage(right, leftright, rightdata, sizeof rightdata);

	/* initialize little fonts */
	tinyfont = openfont(display, "/lib/font/bit/misc/ascii.5x7.font");
	if(tinyfont == nil)
		tinyfont = font;
	mediumfont = openfont(display, "/lib/font/bit/pelm/latin1.8.font");
	if(mediumfont == nil)
		mediumfont = font;
	datefont = font;

	facep.y += datefont->height;
	if(datefont->height & 1)	/* stipple parity */
		facep.y++;
	faces = nil;
}

void
drawtime(void)
{
	Rectangle r;

	r.min = addpt(screen->r.min, datep);
	if(eqpt(enddate, ZP)){
		enddate = r.min;
		enddate.x += stringwidth(datefont, "Wed May 30 22:54");	/* nice wide string */
		enddate.x += Facesep;	/* for safety */
	}
	r.max.x = enddate.x;
	r.max.y = enddate.y+datefont->height;
	draw(screen, r, bgrnd, nil, ZP);
	string(screen, r.min, display->black, ZP, datefont, date);
}

void
timeproc(void *dummy)
{
	for(;;){
		lockdisplay(display);
		drawtime();
		updatetimes();
		flushimage(display, 1);
		unlockdisplay(display);
		sleep(60000);
		setdate();
	}
}

int
alreadyseen(char *digest)
{
	int i;
	Face *f;

	if(!digest)
		return 0;

	/* can do accurate check */
	for(i=0; i<nfaces; i++){
		f = faces[i];
		if(f->str[Sdigest]!=nil && strcmp(digest, f->str[Sdigest])==0)
			return 1;
	}
	return 0;
}

int
torune(Rune *r, char *s, int nr)
{
	int i;

	for(i=0; i<nr-1 && *s!='\0'; i++)
		s += chartorune(r+i, s);
	r[i] = L'\0';
	return i;
}

void
center(Font *f, Point p, char *s, Image *color)
{
	int i, n, dx;
	Rune rbuf[32];
	char sbuf[32*UTFmax+1];

	dx = stringwidth(f, s);
	if(dx > Facesize){
		n = torune(rbuf, s, nelem(rbuf));
		for(i=0; i<n; i++){
			dx = runestringnwidth(f, rbuf, i+1);
			if(dx > Facesize)
				break;
		}
		sprint(sbuf, "%.*S", i, rbuf);
		s = sbuf;
		dx = stringwidth(f, s);
	}
	p.x += (Facesize-dx)/2;
	string(screen, p, color, ZP, f, s);
}

Rectangle
facerect(int index)	/* index is geometric; 0 is always upper left face */
{
	Rectangle r;
	int x, y;

	x = index % nacross;
	y = index / nacross;
	r.min = addpt(screen->r.min, facep);
	r.min.x += x*(Facesize+Facesep);
	r.min.y += y*(Facesize+Facesep+2*mediumfont->height);
	r.max = addpt(r.min, Pt(Facesize, Facesize));
	r.max.y += 2*mediumfont->height;
	/* simple fix to avoid drawing off screen, allowing customers to use position */
	if(index<0 || index>=nacross*ndown)
		r.max.x = r.min.x;
	return r;
}

static char *mon = "JanFebMarAprMayJunJulAugSepOctNovDec";
char*
facetime(Face *f, int *recent)
{
	static char buf[30];

	if((long)(now - f->time) > HhmmTime){
		*recent = 0;
		sprint(buf, "%.3s %2d", mon+3*f->tm.mon, f->tm.mday);
		return buf;
	}else{
		*recent = 1;
		sprint(buf, "%02d:%02d", f->tm.hour, f->tm.min);
		return buf;
	}
}

void
drawface(Face *f, int i)
{
	char *tstr;
	Rectangle r;
	Point p;

	if(f == nil)
		return;
	if(i<first || i>=last)
		return;
	r = facerect(i-first);
	draw(screen, r, bgrnd, nil, ZP);
	draw(screen, r, f->bit, f->mask, ZP);
	r.min.y += Facesize;
	center(mediumfont, r.min, f->str[Suser], display->black);
	r.min.y += mediumfont->height;
	tstr = facetime(f, &f->recent);
	center(mediumfont, r.min, tstr, display->black);
	if(f->unknown){
		r.min.y -= mediumfont->height + tinyfont->height + 2;
		for(p.x=-1; p.x<=1; p.x++)
			for(p.y=-1; p.y<=1; p.y++)
				center(tinyfont, addpt(r.min, p), f->str[Sdomain], display->white);
		center(tinyfont, r.min, f->str[Sdomain], display->black);
	}
}

void
updatetimes(void)
{
	int i;
	Face *f;

	for(i=0; i<nfaces; i++){
		f = faces[i];
		if(f == nil)
			continue;
		if(((long)(now - f->time) <= HhmmTime) != f->recent)
			drawface(f, i);
	}
}

void
setlast(void)
{
	last = first+nacross*ndown;
	if(last > nfaces)
		last = nfaces;
}

void
drawarrows(void)
{
	Point p;

	p = enddate;
	p.x += Facesep;
	if(p.x & 1)
		p.x++;	/* align background texture */
	leftr = rectaddpt(leftright, p);
	p.x += Dx(leftright) + Facesep;
	rightr = rectaddpt(leftright, p);
	draw(screen, leftr, first>0? blue : bgrnd, left, leftright.min);
	draw(screen, rightr, last<nfaces? blue : bgrnd, right, leftright.min);
}

void
addface(Face *f)	/* always adds at 0 */
{
	Face **ofaces;
	Rectangle r0, r1, r;
	int y, nx, ny;

	if(f == nil)
		return;
	if(first != 0){
		first = 0;
		eresized(0);
	}
	findbit(f);

	nx = nacross;
	ny = (nfaces+(nx-1)) / nx;

	lockdisplay(display);
	for(y=ny; y>=0; y--){
		/* move them along */
		r0 = facerect(y*nx+0);
		r1 = facerect(y*nx+1);
		r = r1;
		r.max.x = r.min.x + (nx - 1)*(Facesize+Facesep);
		draw(screen, r, screen, nil, r0.min);
		/* copy one down from row above */
		if(y != 0){
			r = facerect((y-1)*nx+nx-1);
			draw(screen, r0, screen, nil, r.min);
		}
	}

	ofaces = faces;
	faces = emalloc((nfaces+1)*sizeof(Face*));
	memmove(faces+1, ofaces, nfaces*(sizeof(Face*)));
	free(ofaces);
	nfaces++;
	setlast();
	drawarrows();
	faces[0] = f;
	drawface(f, 0);
	flushimage(display, 1);
	unlockdisplay(display);
}

void
loadmboxfaces(char *maildir)
{
	CFid *dirfd;
	Dir *d;
	int i, n;

	dirfd = fsopen(mailfs, maildir, OREAD);
	if(dirfd != nil){
		while((n = fsdirread(dirfd, &d)) > 0){
			for(i=0; i<n; i++)
				addface(dirface(maildir, d[i].name));
			free(d);
		}
		fsclose(dirfd);
	}else
		sysfatal("open %s: %r", maildir);
}

void
freeface(Face *f)
{
	int i;

	if(f->file!=nil && f->bit!=f->file->image)
		freeimage(f->bit);
	freefacefile(f->file);
	for(i=0; i<Nstring; i++)
		free(f->str[i]);
	free(f);
}

void
delface(int j)
{
	Rectangle r0, r1, r;
	int nx, ny, x, y;

	if(j < first)
		first--;
	else if(j < last){
		nx = nacross;
		ny = (nfaces+(nx-1)) / nx;
		x = (j-first)%nx;
		for(y=(j-first)/nx; y<ny; y++){
			if(x != nx-1){
				/* move them along */
				r0 = facerect(y*nx+x);
				r1 = facerect(y*nx+x+1);
				r = r0;
				r.max.x = r.min.x + (nx - x - 1)*(Facesize+Facesep);
				draw(screen, r, screen, nil, r1.min);
			}
			if(y != ny-1){
				/* copy one up from row below */
				r = facerect((y+1)*nx);
				draw(screen, facerect(y*nx+nx-1), screen, nil, r.min);
			}
			x = 0;
		}
		if(last < nfaces)	/* first off-screen becomes visible */
			drawface(faces[last], last-1);
		else{
			/* clear final spot */
			r = facerect(last-first-1);
			draw(screen, r, bgrnd, nil, r.min);
		}
	}
	freeface(faces[j]);
	memmove(faces+j, faces+j+1, (nfaces-(j+1))*sizeof(Face*));
	nfaces--;
	setlast();
	drawarrows();
}

void
dodelete(int i)
{
	Face *f;

	f = faces[i];
	if(history){
		free(f->str[Sshow]);
		f->str[Sshow] = estrdup("");
	}else{
		delface(i);
		flushimage(display, 1);
	}
}

void
delete(char *s, char *digest)
{
	int i;
	Face *f;

	lockdisplay(display);
	for(i=0; i<nfaces; i++){
		f = faces[i];
		if(digest != nil){
			if(f->str[Sdigest]!=nil && strcmp(digest, f->str[Sdigest]) == 0){
				dodelete(i);
				break;
			}
		}else{
			if(f->str[Sshow] && strcmp(s, f->str[Sshow]) == 0){
				dodelete(i);
				break;
			}
		}
	}
	unlockdisplay(display);
}

void
faceproc(void)
{
	for(;;)
		addface(nextface());
}

void
resized(void)
{
	int i;

	nacross = (Dx(screen->r)-2*facep.x+Facesep)/(Facesize+Facesep);
	for(ndown=1; rectinrect(facerect(ndown*nacross), screen->r); ndown++)
		;
	setlast();
	draw(screen, screen->r, bgrnd, nil, ZP);
	enddate = ZP;
	drawtime();
	for(i=0; i<nfaces; i++)
		drawface(faces[i], i);
	drawarrows();
	flushimage(display, 1);
}

void
eresized(int new)
{
	lockdisplay(display);
	if(new && getwindow(display, Refnone) < 0) {
		fprint(2, "can't reattach to window\n");
		killall("reattach");
	}
	resized();
	unlockdisplay(display);
}

void
resizeproc(void *v)
{
	USED(v);

	while(recv(mousectl->resizec, 0) == 1)
		eresized(1);
}

int
getmouse(Mouse *m)
{
	static int eof;

	if(eof)
		return 0;
	if(readmouse(mousectl) < 0){
		eof = 1;
		m->buttons = 0;
		return 0;
	}
	*m = mousectl->m;
	return 1;
}

enum
{
	Clicksize	= 3,		/* pixels */
};

int
scroll(int but, Point p)
{
	int delta;

	delta = 0;
	lockdisplay(display);
	if(ptinrect(p, leftr) && first>0){
		if(but == 2)
			delta = -first;
		else{
			delta = nacross;
			if(delta > first)
				delta = first;
			delta = -delta;
		}
	}else if(ptinrect(p, rightr) && last<nfaces){
		if(but == 2)
			delta = (nfaces-nacross*ndown) - first;
		else{
			delta = nacross;
			if(delta > nfaces-last)
				delta = nfaces-last;
		}
	}
	first += delta;
	last += delta;
	unlockdisplay(display);
	if(delta)
		eresized(0);
	return delta;
}

void
click(int button, Mouse *m)
{
	Point p;
	int i;

	p = m->xy;
	while(m->buttons == (1<<(button-1)))
		getmouse(m);
	if(m->buttons)
		return;
	if(abs(p.x-m->xy.x)>Clicksize || abs(p.y-m->xy.y)>Clicksize)
		return;
	switch(button){
	case 1:
		if(scroll(1, p))
			break;
		if(history){
			/* click clears display */
			lockdisplay(display);
			for(i=0; i<nfaces; i++)
				freeface(faces[i]);
			free(faces);
			faces=nil;
			nfaces = 0;
			unlockdisplay(display);
			eresized(0);
			return;
		}else{
			for(i=first; i<last; i++)	/* clear vwhois faces */
				if(ptinrect(p, facerect(i-first))
				&& strstr(faces[i]->str[Sshow], "/XXXvwhois")){
					lockdisplay(display);
					delface(i);
					flushimage(display, 1);
					unlockdisplay(display);
					break;
				}
		}
		break;
	case 2:
		scroll(2, p);
		break;
	case 3:
		scroll(3, p);
		lockdisplay(display);
		for(i=first; i<last; i++)
			if(ptinrect(p, facerect(i-first))){
				showmail(faces[i]);
				break;
			}
		unlockdisplay(display);
		break;
	}
}

void
mouseproc(void *v)
{
	Mouse mouse;
	USED(v);

	while(getmouse(&mouse)){
		if(mouse.buttons == 1)
			click(1, &mouse);
		else if(mouse.buttons == 2)
			click(2, &mouse);
		else if(mouse.buttons == 4)
			click(3, &mouse);

		while(mouse.buttons)
			getmouse(&mouse);
	}
}

void
killall(char *s)
{
	threadexitsall(s);
}

void
usage(void)
{
	fprint(2, "usage: faces [-hi] [-m maildir] -W winsize\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	int i;

	rfork(RFNOTEG);

	ARGBEGIN{
	case 'h':
		history++;
		break;
	case 'i':
		initload++;
		break;
	case 'm':
		addmaildir(EARGF(usage()));
		maildir = nil;
		break;
	case 'W':
		winsize = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(initdraw(nil, nil, "faces") < 0){
		fprint(2, "faces: initdraw failed: %r\n");
		threadexitsall("initdraw");
	}
	if(maildir)
		addmaildir(maildir);
	init();
	unlockdisplay(display);	/* initdraw leaves it locked */
	display->locking = 1;	/* tell library we're using the display lock */
	setdate();
	eresized(0);

	proccreate(timeproc, nil, STACK);
	proccreate(mouseproc, nil, STACK);
	proccreate(resizeproc, nil, STACK);
	if(initload)
		for(i = 0; i < nmaildirs; i++)
			loadmboxfaces(maildirs[i]);
	faceproc();
	killall(nil);
}
