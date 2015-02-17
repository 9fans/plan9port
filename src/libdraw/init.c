#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>

Display	*display;
Font	*font;
Image	*screen;
int	_drawdebug;

static char deffontname[] = "*default*";
Screen	*_screen;

int		debuglockdisplay = 1;
char	*winsize;

int		visibleclicks = 0;
Image	*mousebuttons;
Image	*mousesave;
Mouse	_drawmouse;

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
geninitdraw(char *devdir, void(*error)(Display*, char*), char *fontname, char *label, char *windir, int ref)
{
	Subfont *df;
	char buf[128], *p;

	if(label == nil)
		label = argv0;
	display = _initdisplay(error, label);
	if(display == nil)
		return -1;

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
		fontname = getenv("font");

	/*
	 * Build fonts with caches==depth of screen, for speed.
	 * If conversion were faster, we'd use 0 and save memory.
	 */
	if(fontname == nil){
		snprint(buf, sizeof buf, "%d %d\n0 %d\t%s\n", df->height, df->ascent,
			df->n-1, deffontname);
//BUG: Need something better for this	installsubfont("*default*", df);
		font = buildfont(display, buf, deffontname);
		if(font == nil){
			fprint(2, "imageinit: can't open default font: %r\n");
			goto Error;
		}
	}else{
		font = openfont(display, fontname);	/* BUG: grey fonts */
		if(font == nil){
			fprint(2, "imageinit: can't open font %s: %r\n", fontname);
			goto Error;
		}
	}
	display->defaultfont = font;

	_screen = allocscreen(display->image, display->white, 0);
	display->screenimage = display->image;	/* _allocwindow wants screenimage->chan */
	screen = _allocwindow(nil, _screen, display->image->r, Refnone, DWhite);
	if(screen == nil){
		fprint(2, "_allocwindow: %r\n");
		goto Error;
	}
	display->screenimage = screen;
	draw(screen, screen->r, display->white, nil, ZP);
	flushimage(display, 1);

	p = getenv("visibleclicks");
	visibleclicks = p != nil && *p == '1';
	if(visibleclicks) {
		Font *f;
		
		f = display->defaultfont;
		mousebuttons = allocimage(display, Rect(0,0,64,22), screen->chan, 0, DWhite);
		border(mousebuttons, mousebuttons->r, 1, display->black, ZP);
		border(mousebuttons, Rect(0, 0, 22, 22), 1, display->black, ZP);
		border(mousebuttons, Rect(42, 0, 64, 22), 1, display->black, ZP);
		string(mousebuttons, Pt(10-stringwidth(display->defaultfont, "1")/2, 11-f->height/2), display->black, ZP, display->defaultfont, "1");
		string(mousebuttons, Pt(21+10-stringwidth(display->defaultfont, "2")/2, 11-f->height/2), display->black, ZP, display->defaultfont, "2");
		string(mousebuttons, Pt(42+10-stringwidth(display->defaultfont, "3")/2, 11-f->height/2), display->black, ZP, display->defaultfont, "3");
		mousesave = allocimage(display, Rect(0,0,64,22), screen->chan, 0, 0);
	}

	/*
	 * I don't see any reason to go away gracefully,
	 * and if some other proc exits holding the display
	 * lock, this atexit call never finishes.
	 *
	 * atexit(drawshutdown);
	 */
	return 1;
}

int
initdraw(void (*error)(Display*, char*), char *fontname, char *label)
{
	return geninitdraw("/dev", error, fontname, label, "/dev", Refnone);
}

extern int _freeimage1(Image*);

static Image*
getimage0(Display *d, Image *image)
{
	char info[12*12+1];
	uchar *a;
	int n;

	/*
	 * If there's an old screen, it has id 0.  The 'J' request below
	 * will try to install the new screen as id 0, so the old one 
	 * must be freed first.
	 */
	if(image){
		_freeimage1(image);
		memset(image, 0, sizeof(Image));
	}

	a = bufimage(d, 2);
	a[0] = 'J';
	a[1] = 'I';
	if(flushimage(d, 0) < 0){
		fprint(2, "cannot read screen info: %r\n");
		return nil;
	}

	n = _displayrddraw(d, info, sizeof info);
	if(n != 12*12){
		fprint(2, "short screen info\n");
		return nil;
	}

	if(image == nil){
		image = mallocz(sizeof(Image), 1);
		if(image == nil){
			fprint(2, "cannot allocate image: %r\n");
			return nil;
		}
	}

	image->display = d;
	image->id = 0;
	image->chan = strtochan(info+2*12);
	image->depth = chantodepth(image->chan);
	image->repl = atoi(info+3*12);
	image->r.min.x = atoi(info+4*12);
	image->r.min.y = atoi(info+5*12);
	image->r.max.x = atoi(info+6*12);
	image->r.max.y = atoi(info+7*12);
	image->clipr.min.x = atoi(info+8*12);
	image->clipr.min.y = atoi(info+9*12);
	image->clipr.max.x = atoi(info+10*12);
	image->clipr.max.y = atoi(info+11*12);
	
	a = bufimage(d, 3);
	a[0] = 'q';
	a[1] = 1;
	a[2] = 'd';
	d->dpi = 100;
	if(flushimage(d, 0) >= 0 && _displayrddraw(d, info, 12) == 12)
		d->dpi = atoi(info);

	return image;
}

/*
 * Attach, or possibly reattach, to window.
 * If reattaching, maintain value of screen pointer.
 */
int
getwindow(Display *d, int ref)
{
	Image *i, *oi;
	Font *f;
	
	/* XXX check for destroyed? */
	
	/*
	 * Libdraw promises not to change the value of "screen",
	 * so we have to reuse the image structure
	 * memory we already have.
	 */
	oi = d->image;
	i = getimage0(d, oi);
	if(i == nil)
		sysfatal("getwindow failed");
	d->image = i;
	/* fprint(2, "getwindow %p -> %p\n", oi, i); */

	freescreen(_screen);
	_screen = allocscreen(i, d->white, 0);
	_freeimage1(screen);
	screen = _allocwindow(screen, _screen, i->r, ref, DWhite);
	d->screenimage = screen;


	if(d->dpi >= DefaultDPI*3/2) {
		for(f=d->firstfont; f != nil; f=f->next)
			loadhidpi(f);
	} else {
		for(f=d->firstfont; f != nil; f=f->next)
			if(f->lodpi != nil && f->lodpi != f)
				swapfont(f, &f->hidpi, &f->lodpi);
	}

	return 0;
}

Display*
_initdisplay(void (*error)(Display*, char*), char *label)
{
	Display *disp;
	Image *image;

	fmtinstall('P', Pfmt);
	fmtinstall('R', Rfmt);

	disp = mallocz(sizeof(Display), 1);
	if(disp == nil){
    Error1:
		return nil;
	}
	disp->srvfd = -1;
	image = nil;
	if(0){
    Error2:
		free(image);
		free(disp);
		goto Error1;
	}
	disp->bufsize = 65500;
	disp->buf = malloc(disp->bufsize+5);	/* +5 for flush message */
	disp->bufp = disp->buf;
	disp->error = error;
	qlock(&disp->qlock);

	if(disp->buf == nil)
		goto Error2;
	if(0){
    Error3:
		free(disp->buf);
		goto Error2;
	}
	
	if(_displaymux(disp) < 0
	|| _displayconnect(disp) < 0
	|| _displayinit(disp, label, winsize) < 0)
		goto Error3;
	if(0){
    Error4:
		close(disp->srvfd);
		goto Error3;
	}

	image = getimage0(disp, nil);
	if(image == nil)
		goto Error4;
	
	disp->image = image;
	disp->white = allocimage(disp, Rect(0, 0, 1, 1), GREY1, 1, DWhite);
	disp->black = allocimage(disp, Rect(0, 0, 1, 1), GREY1, 1, DBlack);
	if(disp->white == nil || disp->black == nil){
		free(disp->white);
		free(disp->black);
		goto Error4;
	}
	
	disp->opaque = disp->white;
	disp->transparent = disp->black;

	return disp;
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
	if(disp->srvfd >= 0)
		close(disp->srvfd);
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

	if(_displaywrdraw(d, d->buf, n) != n){
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
	if(visible == 1 && visibleclicks && mousebuttons && _drawmouse.buttons) {
		Rectangle r, r1;
		int ret;

		r = mousebuttons->r;
		r = rectaddpt(r, _drawmouse.xy);
		r = rectaddpt(r, Pt(-Dx(mousebuttons->r)/2, -Dy(mousebuttons->r)-3));
		drawop(mousesave, mousesave->r, screen, nil, r.min, S);
		
		r1 = rectaddpt(Rect(0, 0, 22, 22), r.min);
		if(_drawmouse.buttons & 1)
			drawop(screen, r1, mousebuttons, nil, ZP, S);
		r1 = rectaddpt(r1, Pt(21, 0));
		if(_drawmouse.buttons & 2)
			drawop(screen, r1, mousebuttons, nil, Pt(21, 0), S);
		r1 = rectaddpt(r1, Pt(21, 0));
		if(_drawmouse.buttons & 4)
			drawop(screen, r1, mousebuttons, nil, Pt(42, 0), S);
		ret = flushimage(d, 2);
		drawop(screen, r, mousesave, nil, ZP, S);
		return ret;
	}
	
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

int
scalesize(Display *d, int n)
{
	if(d == nil || d->dpi <= DefaultDPI)
		return n;
	return (n*d->dpi+DefaultDPI/2)/DefaultDPI;
}
