#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <geometry.h>

typedef struct Vert{
	Point3 world;
	Point3 screen;
	int color;
}Vert;

int		nocubes;
int		ncolor;
Quaternion	q = {1.,0.,0.,0.};
Image		*image;
Image		*bg;
Image		*color[256];
Rectangle	viewrect;
int		prevsel;

Point3
p3(double x, double y, double z, double w)
{
	Point3 p;

	p.x = x;
	p.y = y;
	p.z = z;
	p.w = w;
	return p;
}

int
cmp(Vert *a, Vert *b)
{
	if(a->screen.z>b->screen.z)
		return -1;
	if(a->screen.z<b->screen.z)
		return 1;
	return 0;
}

/* crummy hack */
void
readcolmap(Display *d, RGB *cmap)
{
	int i, rgb, r, g, b;

	for(i=0; i<256; i++){
		rgb = cmap2rgb(i);
		r = rgb>>16;
		g = (rgb>>8)&0xFF;
		b = rgb & 0xFF;
		cmap[i].red = r|(r<<8)|(r<<16)|(r<<24);
		cmap[i].green = g|(g<<8)|(g<<16)|(g<<24);
		cmap[i].blue = b|(b<<8)|(b<<16)|(b<<24);
	}
}

void
colorspace(RGB *cmap, Vert *v)
{
	Space *view;
	int i;

	for(i=0;i!=ncolor;i++){
		v[i].world.x=(cmap[i].red>>24)/255.-.5;
		v[i].world.y=(cmap[i].green>>24)/255.-.5;
		v[i].world.z=(cmap[i].blue>>24)/255.-.5;
		v[i].world.w=1.;
		v[i].color=i;
	}
	view = pushmat(0);
	viewport(view, viewrect, 1.);
	persp(view, 30., 3., 7.);
	look(view, p3(0., 0., -5., 1.), p3(0., 0., 0., 1.),
		p3(0., 1., 0., 1.));
	qrot(view, q);
	for(i=0;i!=ncolor;i++)
		v[i].screen = xformpointd(v[i].world, 0, view);
	popmat(view);
}

void
line3(Vert a, Vert b)
{
	line(image, Pt(a.screen.x, a.screen.y), Pt(b.screen.x, b.screen.y), 0, 0, 0, display->white, ZP);
}


void
redraw(void)
{
	int i, m;
	RGB cmap[256];
	Vert v[256];

	readcolmap(display, cmap);
	colorspace(cmap, v);
	draw(image, image->r, bg, nil, Pt(0, 0));
	m = Dx(viewrect)/2;
	if(m > Dy(viewrect)/2)
		m = Dy(viewrect)/2;
	ellipse(image, addpt(viewrect.min, divpt(Pt(Dx(viewrect), Dy(viewrect)), 2)),
		m, m, 1, display->white, ZP);

	line3(v[0], v[0x36]);
	line3(v[0x36], v[0x32]);
	line3(v[0x32], v[0x3F]);
	line3(v[0x3F], v[0]);

	line3(v[0xF0], v[0xF3]);
	line3(v[0xF3], v[0xFF]);
	line3(v[0xFF], v[0xFC]);
	line3(v[0xFC], v[0xF0]);

	line3(v[0], v[0xF0]);
	line3(v[0x36], v[0xF3]);
	line3(v[0x32], v[0xFF]);
	line3(v[0x3F], v[0xFC]);

	qsort(v, ncolor, sizeof(Vert), (int(*)(const void*, const void*))cmp);
	if(!nocubes)
		for(i=0; i!=ncolor; i++)
			draw(image, rectaddpt(Rect(-3, -3, 4, 4), Pt(v[i].screen.x, v[i].screen.y)),
				color[v[i].color], nil, Pt(0, 0));
	draw(screen, image->r, image, nil, image->r.min);
	flushimage(display, 1);
}

void
eresized(int new)
{
	int dx, dy;

	if(new && getwindow(display, Refnone) < 0){
		fprint(2, "colors: can't reattach to window: %r\n");
		exits("reshaped");
	}
	draw(screen, screen->r, display->black, nil, ZP);
	replclipr(screen, 0, insetrect(screen->r, 3));
	viewrect = screen->clipr;
	viewrect.min.y += stringsize(font, "0i").y + 5;
	if(image)
		freeimage(image);
	image = allocimage(display, viewrect, screen->chan, 0, DNofill);
	dx = viewrect.max.x-viewrect.min.x;
	dy = viewrect.max.y-viewrect.min.y;
	if(dx>dy){
		viewrect.min.x=(viewrect.min.x+viewrect.max.x-dy)/2;
		viewrect.max.x=viewrect.min.x+dy;
	}
	else{
		viewrect.min.y=(viewrect.min.y+viewrect.max.y-dx)/2;
		viewrect.max.y=viewrect.min.y+dx;
	}
	if(image==nil){
		fprint(2, "can't allocate image\n");
		exits("bad allocimage");
	}
	prevsel = -1;
	redraw();
}

void main(int argc, char **argv){
	Vert v[256];
	RGB cmap[256];
	char buf[100];
	Point p;
	Mouse m;
	int i;
	ulong bgcol;

	bgcol = DNofill;
	ARGBEGIN{
	case 'n':
		nocubes = 1;
		break;
	case 'b':
		bgcol = DBlack;
		break;
	case 'w':
		bgcol = DWhite;
		break;
	}ARGEND

	if(initdraw(0,0,0) < 0)
		sysfatal("initdraw: %r");
	ncolor=256;
	for(i=0;i!=ncolor;i++)
		color[i] = allocimage(display, Rect(0, 0, 1, 1), CMAP8, 1, cmap2rgba(i));
	if(bgcol==DNofill){
		bg = allocimage(display, Rect(0, 0, 2, 2), screen->chan, 1, DWhite);
		draw(bg, Rect(0, 0, 1, 1), color[0], nil, Pt(0, 0));
		draw(bg, Rect(1, 1, 2, 2), color[0], nil, Pt(0, 0));
	}else
		bg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, bgcol);

	einit(Emouse);
	eresized(0);

	for(;;){
		m = emouse();
		if(m.buttons&1)
			qball(viewrect, &m, &q, redraw, 0);
		else if(m.buttons & 2){
			readcolmap(display, cmap);
			colorspace(cmap, v);
			qsort(v, ncolor, sizeof(Vert), (int(*)(const void*, const void*))cmp);
			while(m.buttons){
				for(i=ncolor-1; i!=0; i--){
					if(ptinrect(m.xy, rectaddpt(Rect(-3, -3, 4, 4), Pt(v[i].screen.x, v[i].screen.y)))){
						i = v[i].color;
						if(i == prevsel)
							break;
						sprint(buf, "index %3d r %3ld g %3ld b %3ld",
							i,
							cmap[i].red>>24,
							cmap[i].green>>24,
							cmap[i].blue>>24);
						p = addpt(screen->r.min, Pt(2,2));
						draw(screen, Rpt(p, addpt(p, stringsize(font, buf))), display->black, nil, p);
						string(screen, p, display->white, ZP, font, buf);
						prevsel = i;
						break;
					}
				}
				m = emouse();
			}
		}else if(m.buttons&4){
			do
				m = emouse();
			while(m.buttons);
			exits(0);
		}
	}
}
