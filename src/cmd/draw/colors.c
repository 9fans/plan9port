#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

int nbit, npix;
Image *pixel;
Rectangle crect[256];

Image *color[256];

void
eresized(int new)
{
	int x, y, i, n, nx, ny;
	Rectangle r, b;

	if(new && getwindow(display, Refnone) < 0){
		fprint(2, "colors: can't reattach to window: %r\n");
		exits("resized");
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

char *buttons[] =
{
	"exit",
	0
};

ulong
grey(int i)
{
	if(i < 0)
		return grey(0);
	if(i > 255)
		return grey(255);
	return (i<<16)+(i<<8)+i;
}

Menu menu =
{
	buttons
};

int
dither[16] =  {
	0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5
};

void
main(int argc, char *argv[])
{
	Point p;
	Mouse m;
	int i, j, k, l, n, ramp, prev;
	char buf[100];
	char *fmt;
	Image *dark;
	ulong rgb;

	ramp = 0;

	fmt = "index %3d r %3lud g %3lud b %3lud 0x%.8luX        ";
	ARGBEGIN{
	default:
		goto Usage;
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
		exits("usage");
	}

	if(initdraw(nil, nil, "colors") < 0)
		sysfatal("initdraw failed: %r");
	einit(Emouse);

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
	eresized(0);
	prev = -1;
	for(;;){
		m = emouse();
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
				m = emouse();
			}
			break;

		case 4:
			switch(emenuhit(3, &m, &menu)){
			case 0:
				exits(0);
			}
		}
	}
}
