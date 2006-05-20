#include <u.h>
#include <libc.h>
#include <draw.h>

Display	*display;
Font	*font;
Image	*screen;
int	_drawdebug;

static char deffontname[] = "*default*";
Screen	*_screen;

int		debuglockdisplay = 1;

/*
static void
drawshutdown(void)
{
	Display *d;

	d = display;
	if(d){
		display = nil;
		closedisplay(d);
	}
}
*/

int
initdraw(void (*error)(Display*, char*), char *fontname, char *label)
{
	Subfont *df;
	char buf[128];

	rfork(RFNOTEG);	/* x11-event.c will postnote hangup */
	display = _initdisplay(error, label); /* sets screen too */
	if(display == nil)
		return -1;

	lockdisplay(display);
	display->screenimage = display->image;

	/*
	 * Set up default font
	 */
	df = getdefont(display);
	display->defaultsubfont = df;
	if(df == nil){
		fprint(2, "imageinit: can't open default subfont: %r\n");
    Error:
		closedisplay(display);
		display = nil;
		return -1;
	}
	if(fontname == nil)
		fontname = getenv("font");	/* leak */

	/*
	 * Build fonts with caches==depth of screen, for speed.
	 * If conversion were faster, we'd use 0 and save memory.
	 */
	if(fontname == nil){
		snprint(buf, sizeof buf, "%d %d\n0 %d\t%s\n", df->height, df->ascent,
			df->n-1, deffontname);
/*BUG: Need something better for this	installsubfont("*default*", df); */
		font = buildfont(display, buf, deffontname);
		if(font == nil){
			fprint(2, "initdraw: can't open default font: %r\n");
			goto Error;
		}
	}else{
		font = openfont(display, fontname);	/* BUG: grey fonts */
		if(font == nil){
			fprint(2, "initdraw: can't open font %s: %r\n", fontname);
			goto Error;
		}
	}
	display->defaultfont = font;

	display->white = allocimage(display, Rect(0,0,1,1), GREY1, 1, DWhite);
	display->black = allocimage(display, Rect(0,0,1,1), GREY1, 1, DBlack);
	if(display->white == nil || display->black == nil){
		fprint(2, "initdraw: can't allocate white and black");
		goto Error;
	}
	display->opaque = display->white;
	display->transparent = display->black;

	_screen = allocscreen(display->image, display->white, 0);
	screen = _allocwindow(nil, _screen, display->image->r, Refnone, DWhite);
	display->screenimage = screen;
	draw(screen, screen->r, display->white, nil, ZP);
	flushimage(display, 1);

	/*
	 * I don't see any reason to go away gracefully,
	 * and if some other proc exits holding the display
	 * lock, this atexit call never finishes.
	 *
	 * atexit(drawshutdown);
	 */
	return 1;
}

/*
 * Call with d unlocked.
 * Note that disp->defaultfont and defaultsubfont are not freed here.
 */
void
closedisplay(Display *disp)
{
	int fd;
	char buf[128];

	if(disp == nil)
		return;
	if(disp == display)
		display = nil;
	if(disp->oldlabel[0]){
		snprint(buf, sizeof buf, "%s/label", disp->windir);
		fd = open(buf, OWRITE);
		if(fd >= 0){
			write(fd, disp->oldlabel, strlen(disp->oldlabel));
			close(fd);
		}
	}

	free(disp->devdir);
	free(disp->windir);
	if(disp->white)
		freeimage(disp->white);
	if(disp->black)
		freeimage(disp->black);
	free(disp);
}

void
lockdisplay(Display *disp)
{
	if(debuglockdisplay){
		/* avoid busy looping; it's rare we collide anyway */
		while(!canqlock(&disp->qlock)){
			fprint(1, "proc %d waiting for display lock...\n", getpid());
			sleep(1000);
		}
	}else
		qlock(&disp->qlock);
}

void
unlockdisplay(Display *disp)
{
	qunlock(&disp->qlock);
}

void
drawerror(Display *d, char *s)
{
	char err[ERRMAX];

	if(d->error)
		d->error(d, s);
	else{
		errstr(err, sizeof err);
		fprint(2, "draw: %s: %s\n", s, err);
		exits(s);
	}
}

static
int
doflush(Display *d)
{
	int n;

	n = d->bufp-d->buf;
	if(n <= 0)
		return 1;

	if(_drawmsgwrite(d, d->buf, n) != n){
		if(_drawdebug)
			fprint(2, "flushimage fail: d=%p: %r\n", d); /**/
		d->bufp = d->buf;	/* might as well; chance of continuing */
		return -1;
	}
	d->bufp = d->buf;
	return 1;
}

int
flushimage(Display *d, int visible)
{
	if(visible){
		*d->bufp++ = 'v';	/* five bytes always reserved for this */
		if(d->_isnewdisplay){
			BPLONG(d->bufp, d->screenimage->id);
			d->bufp += 4;
		}
	}
	return doflush(d);
}

uchar*
bufimage(Display *d, int n)
{
	uchar *p;

	if(n<0 || d == nil || n>d->bufsize){
		abort();
		werrstr("bad count in bufimage");
		return 0;
	}
	if(d->bufp+n > d->buf+d->bufsize)
		if(doflush(d) < 0)
			return 0;
	p = d->bufp;
	d->bufp += n;
	return p;
}

