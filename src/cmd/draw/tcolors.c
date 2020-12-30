#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>

enum
{
	STACK = 8192
};

int nbit, npix;
Image *pixel;
Rectangle crect[256];
Image *color[256];
char *fmt;
int ramp;

Mousectl *mousectl;
Keyboardctl *keyboardctl;

void keyboardthread(void*);
void mousethread(void*);
void resizethread(void*);

ulong
grey(int i)
{
	if(i < 0)
		return grey(0);
	if(i > 255)
		return grey(255);
	return (i<<16)+(i<<8)+i;
}

int
dither[16] =  {
	0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5
};

extern int chattydrawclient;

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char *argv[])
{
	int i, j, k, l;
	Image *dark;

	ramp = 0;

	fmt = "index %3d r %3lud g %3lud b %3lud 0x%.8luX        ";
	ARGBEGIN{
	default:
		goto Usage;
	case 'D':
		chattydrawclient = 1;
		break;
	case 'x':
		fmt = "index %2luX r %3luX g %3luX b %3luX 0x%.8luX       ";
		break;
	case 'r':
		ramp = 1;
		break;
	}ARGEND

	if(argc){
	Usage:
		fprint(2, "Usage: %s [-rx]\n", argv0);
		threadexitsall("usage");
	}

	if(initdraw(0, nil, "colors") < 0)
		sysfatal("initdraw failed: %r");

	mousectl = initmouse(nil, display->image);
	if(mousectl == nil)
		sysfatal("initmouse: %r");

	keyboardctl = initkeyboard(nil);
	if(keyboardctl == nil)
		sysfatal("initkeyboard: %r");

	for(i=0; i<256; i++){
		if(ramp){
			if(screen->chan == CMAP8){
				/* dither the fine grey */
				j = i-(i%17);
				dark = allocimage(display, Rect(0,0,1,1), screen->chan, 1, (grey(j)<<8)+0xFF);
				color[i] = allocimage(display, Rect(0,0,4,4), screen->chan, 1, (grey(j+17)<<8)+0xFF);
				for(j=0; j<16; j++){
					k = j%4;
					l = j/4;
					if(dither[j] > (i%17))
						draw(color[i], Rect(k, l, k+1, l+1), dark, nil, ZP);
				}
				freeimage(dark);
			}else
				color[i] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, (grey(i)<<8)+0xFF);
		}else
			color[i] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, (cmap2rgb(i)<<8)+0xFF);
		if(color[i] == nil)
			sysfatal("can't allocate image: %r");
	}

	threadcreate(mousethread, nil, STACK);
	threadcreate(keyboardthread, nil, STACK);
	threadcreate(resizethread, nil, STACK);
}

void
keyboardthread(void *v)
{
	Rune r;

	USED(v);

	while(recv(keyboardctl->c, &r) >= 0)
		;
}

char *buttons[] =
{
	"exit",
	0
};

Menu menu =
{
	buttons
};

void
mousethread(void *v)
{
	Point p;
	Mouse m;
	int i, n, prev;
	char buf[100];
	ulong rgb;

	prev = -1;
	while(readmouse(mousectl) >= 0){
		m = mousectl->m;
		switch(m.buttons){
		case 1:
			while(m.buttons){
				if(screen->depth > 8)
					n = 256;
				else
					n = 1<<screen->depth;
				for(i=0; i!=n; i++)
					if(i!=prev && ptinrect(m.xy, crect[i])){
						if(ramp)
							rgb = grey(i);
						else
							rgb = cmap2rgb(i);
						sprint(buf, fmt,
							i,
							(rgb>>16)&0xFF,
							(rgb>>8)&0xFF,
							rgb&0xFF,
							(rgb<<8) | 0xFF);
						p = addpt(screen->r.min, Pt(2,2));
						draw(screen, Rpt(p, addpt(p, stringsize(font, buf))), display->white, nil, p);
						string(screen, p, display->black, ZP, font, buf);
						prev=i;
						break;
					}
				readmouse(mousectl);
				m = mousectl->m;
			}
			break;

		case 4:
			switch(menuhit(3, mousectl, &menu, nil)){
			case 0:
				threadexitsall(0);
			}
		}
	}
}

void
eresized(int new)
{
	int x, y, i, n, nx, ny;
	Rectangle r, b;

	if(new && getwindow(display, Refnone) < 0){
		fprint(2, "colors: can't reattach to window: %r\n");
		threadexitsall("resized");
	}
	if(screen->depth > 8){
		n = 256;
		nx = 16;
	}else{
		n = 1<<screen->depth;
		nx = 1<<(screen->depth/2);
	}

	ny = n/nx;
	draw(screen, screen->r, display->white, nil, ZP);
	r = insetrect(screen->r, 5);
	r.min.y+=20;
	b.max.y=r.min.y;
	for(i=n-1, y=0; y!=ny; y++){
		b.min.y=b.max.y;
		b.max.y=r.min.y+(r.max.y-r.min.y)*(y+1)/ny;
		b.max.x=r.min.x;
		for(x=0; x!=nx; x++, --i){
			b.min.x=b.max.x;
			b.max.x=r.min.x+(r.max.x-r.min.x)*(x+1)/nx;
			crect[i]=insetrect(b, 1);
			draw(screen, crect[i], color[i], nil, ZP);
		}
	}
	flushimage(display, 1);
}

void
resizethread(void *v)
{
	ulong x;

	USED(v);

	eresized(0);
	while(recv(mousectl->resizec, &x) >= 0)
		eresized(1);
}
