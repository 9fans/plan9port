#include "mplot.h"
int mapminx, mapminy, mapmaxx, mapmaxy;
Image *offscreen;
/*
 * Clear the window from x0, y0 to x1, y1 (inclusive) to color c
 */
void m_clrwin(int x0, int y0, int x1, int y1, int c){
	draw(offscreen, Rect(x0, y0, x1+1, y1+1), getcolor(c), nil, ZP);
}
/*
 * Draw text between pointers p and q with first character centered at x, y.
 * Use color c.  Centered if cen is non-zero, right-justified if right is non-zero.
 * Returns the y coordinate for any following line of text.
 */
int m_text(int x, int y, char *p, char *q, int c, int cen, int right){
	Point tsize;
	USED(c);
	tsize=stringsize(font, p);
	if(cen) x -= tsize.x/2;
	else if(right) x -= tsize.x;
	stringn(offscreen, Pt(x, y-tsize.y/2), getcolor(c), ZP, font, p, q-p);
	return y+tsize.y;
}
/*
 * Draw the vector from x0, y0 to x1, y1 in color c.
 * Clipped by caller
 */
void m_vector(int x0, int y0, int x1, int y1, int c){
	line(offscreen, Pt(x0, y0), Pt(x1, y1), Endsquare, Endsquare, 0, getcolor(c), ZP);
}
char *scanint(char *s, int *n){
	while(*s<'0' || '9'<*s){
		if(*s=='\0'){
			fprint(2, "plot: bad -Wxmin,ymin,xmax,ymax\n");
			exits("bad arg");
		}
		s++;
	}
	*n=0;
	while('0'<=*s && *s<='9'){
		*n=*n*10+*s-'0';
		s++;
	}
	return s;
}
char *rdenv(char *name){
	char *v;
	int fd, size;
	fd=open(name, OREAD);
	if(fd<0) return 0;
	size=seek(fd, 0, 2);
	v=malloc(size+1);
	if(v==0){
		fprint(2, "Can't malloc: %r\n");
		exits("no mem");
	}
	seek(fd, 0, 0);
	read(fd, v, size);
	v[size]=0;
	close(fd);
	return v;
}
/*
 * Startup initialization
 */
void m_initialize(char *s){
	static int first=1;
	int dx, dy;
	USED(s);
	if(first){
		if(initdraw(0,0,"plot") < 0)
			sysfatal("plot: can't open display: %r");
		einit(Emouse);
		clipminx=mapminx=screen->r.min.x+4;
		clipminy=mapminy=screen->r.min.y+4;
		clipmaxx=mapmaxx=screen->r.max.x-5;
		clipmaxy=mapmaxy=screen->r.max.y-5;
		dx=clipmaxx-clipminx;
		dy=clipmaxy-clipminy;
		if(dx>dy){
			mapminx+=(dx-dy)/2;
			mapmaxx=mapminx+dy;
		}
		else{
			mapminy+=(dy-dx)/2;
			mapmaxy=mapminy+dx;
		}
		first=0;
		offscreen = screen;
	}
}
/*
 * Clean up when finished
 */
void m_finish(void){
	m_swapbuf();
}
void m_swapbuf(void){
	if(offscreen!=screen)
		draw(screen, offscreen->r, offscreen, nil, offscreen->r.min);
	flushimage(display, 1);
}
void m_dblbuf(void){
	if(offscreen==screen){
		offscreen=allocimage(display, insetrect(screen->r, 4), screen->chan, 0, -1);
		if(offscreen==0){
			fprintf(stderr, "Can't double buffer\n");
			offscreen=screen;
		}
	}
}
/* Assume colormap entry because
 * Use cache to avoid repeated allocation.
 */
struct{
	int		v;
	Image	*i;
}icache[32];

Image*
getcolor(int v)
{
	Image *i;
	int j;

	for(j=0; j<nelem(icache); j++)
		if(icache[j].v==v && icache[j].i!=nil)
			return icache[j].i;

	i = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1, v);
	if(i == nil){
		fprint(2, "plot: can't allocate image for color: %r\n");
		exits("allocimage");
	}
	for(j=0; j<nelem(icache); j++)
		if(icache[j].i == nil){
			icache[j].v = v;
			icache[j].i = i;
			break;
		}

	return i;
}
