#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <event.h>
#include <ctype.h>
#include "map.h"
#undef	RAD
#undef	TWOPI
#include "sky.h"

	/* convert to milliarcsec */
static int32	c = MILLIARCSEC;	/* 1 degree */
static int32	m5 = 1250*60*60;	/* 5 minutes of ra */

DAngle	ramin;
DAngle	ramax;
DAngle	decmin;
DAngle	decmax;
int		folded;

Image	*grey;
Image	*alphagrey;
Image	*green;
Image	*lightblue;
Image	*lightgrey;
Image	*ocstipple;
Image	*suncolor;
Image	*mooncolor;
Image	*shadowcolor;
Image	*mercurycolor;
Image	*venuscolor;
Image	*marscolor;
Image	*jupitercolor;
Image	*saturncolor;
Image	*uranuscolor;
Image	*neptunecolor;
Image	*plutocolor;
Image	*cometcolor;

Planetrec	*planet;

int32	mapx0, mapy0;
int32	mapra, mapdec;
double	mylat, mylon, mysid;
double	mapscale;
double	maps;
int (*projection)(struct place*, double*, double*);

char *fontname = "/lib/font/bit/luc/unicode.6.font";

/* types Coord and Loc correspond to types in map(3) thus:
   Coord == struct coord;
   Loc == struct place, except longitudes are measured
     positive east for Loc and west for struct place
*/

typedef struct Xyz Xyz;
typedef struct coord Coord;
typedef struct Loc Loc;

struct Xyz{
	double x,y,z;
};

struct Loc{
	Coord nlat, elon;
};

Xyz north = { 0, 0, 1 };

int
plotopen(void)
{
	if(display != nil)
		return 1;
	if(initdraw(drawerror, nil, nil) < 0){
		fprint(2, "initdisplay failed: %r\n");
		return -1;
	}
	grey = allocimage(display, Rect(0, 0, 1, 1), CMAP8, 1, 0x777777FF);
	lightgrey = allocimage(display, Rect(0, 0, 1, 1), CMAP8, 1, 0xAAAAAAFF);
	alphagrey = allocimage(display, Rect(0, 0, 2, 2), RGBA32, 1, 0x77777777);
	green = allocimage(display, Rect(0, 0, 1, 1), CMAP8, 1, 0x00AA00FF);
	lightblue = allocimage(display, Rect(0, 0, 1, 1), CMAP8, 1, 0x009EEEFF);
	ocstipple = allocimage(display, Rect(0, 0, 2, 2), CMAP8, 1, 0xAAAAAAFF);
	draw(ocstipple, Rect(0, 0, 1, 1), display->black, nil, ZP);
	draw(ocstipple, Rect(1, 1, 2, 2), display->black, nil, ZP);

	suncolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0xFFFF77FF);
	mooncolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0xAAAAAAFF);
	shadowcolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0x00000055);
	mercurycolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0xFFAAAAFF);
	venuscolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0xAAAAFFFF);
	marscolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0xFF5555FF);
	jupitercolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0xFFFFAAFF);
	saturncolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0xAAFFAAFF);
	uranuscolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0x77DDDDFF);
	neptunecolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0x77FF77FF);
	plutocolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0x7777FFFF);
	cometcolor = allocimage(display, Rect(0, 0, 1, 1), RGBA32, 1, 0xAAAAFFFF);
	font = openfont(display, fontname);
	if(font == nil)
		fprint(2, "warning: no font %s: %r\n", fontname);
	return 1;
}

/*
 * Function heavens() for setting up observer-eye-view
 * sky charts, plus subsidiary functions.
 * Written by Doug McIlroy.
 */

/* heavens(nlato, wlono, nlatc, wlonc) sets up a map(3)-style
   coordinate change (by calling orient()) and returns a
   projection function heavens for calculating a star map
   centered on nlatc,wlonc and rotated so it will appear
   upright as seen by a ground observer at nlato,wlono.
   all coordinates (north latitude and west longitude)
   are in degrees.
*/

Coord
mkcoord(double degrees)
{
	Coord c;

	c.l = degrees*PI/180;
	c.s = sin(c.l);
	c.c = cos(c.l);
	return c;
}

Xyz
ptov(struct place p)
{
	Xyz v;

	v.z = p.nlat.s;
	v.x = p.nlat.c * p.wlon.c;
	v.y = -p.nlat.c * p.wlon.s;
	return v;
}

double
dot(Xyz a, Xyz b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

Xyz
cross(Xyz a, Xyz b)
{
	Xyz v;

	v.x = a.y*b.z - a.z*b.y;
	v.y = a.z*b.x - a.x*b.z;
	v.z = a.x*b.y - a.y*b.x;
	return v;
}

double
len(Xyz a)
{
	return sqrt(dot(a, a));
}

/* An azimuth vector from a to b is a tangent to
   the sphere at a pointing along the (shorter)
   great-circle direction to b.  It lies in the
   plane of the vectors a and b, is perpendicular
   to a, and has a positive compoent along b,
   provided a and b span a 2-space.  Otherwise
   it is null.  (aXb)Xa, where X denotes cross
   product, is such a vector. */

Xyz
azvec(Xyz a, Xyz b)
{
	return cross(cross(a,b), a);
}

/* Find the azimuth of point b as seen
   from point a, given that a is distinct
   from either pole and from b */

double
azimuth(Xyz a, Xyz b)
{
	Xyz aton = azvec(a,north);
	Xyz atob = azvec(a,b);
	double lenaton = len(aton);
	double lenatob = len(atob);
	double az = acos(dot(aton,atob)/(lenaton*lenatob));

	if(dot(b,cross(north,a)) < 0)
		az = -az;
	return az;
}

/* Find the rotation (3rd argument of orient() in the
   map projection package) for the map described
   below.  The return is radians; it must be converted
   to degrees for orient().
*/

double
rot(struct place center, struct place zenith)
{
	Xyz cen = ptov(center);
	Xyz zen = ptov(zenith);

	if(cen.z > 1-FUZZ) 	/* center at N pole */
		return PI + zenith.wlon.l;
	if(cen.z < FUZZ-1)		/* at S pole */
		return -zenith.wlon.l;
	if(fabs(dot(cen,zen)) > 1-FUZZ)	/* at zenith */
		return 0;
	return azimuth(cen, zen);
}

int (*
heavens(double zlatdeg, double zlondeg, double clatdeg, double clondeg))(struct place*, double*, double*)
{
	struct place center;
	struct place zenith;

	center.nlat = mkcoord(clatdeg);
	center.wlon = mkcoord(clondeg);
	zenith.nlat = mkcoord(zlatdeg);
	zenith.wlon = mkcoord(zlondeg);
	projection = stereographic();
	orient(clatdeg, clondeg, rot(center, zenith)*180/PI);
	return projection;
}

int
maptoxy(int32 ra, int32 dec, double *x, double *y)
{
	double lat, lon;
	struct place pl;

	lat = angle(dec);
	lon = angle(ra);
	pl.nlat.l = lat;
	pl.nlat.s = sin(lat);
	pl.nlat.c = cos(lat);
	pl.wlon.l = lon;
	pl.wlon.s = sin(lon);
	pl.wlon.c = cos(lon);
	normalize(&pl);
	return projection(&pl, x, y);
}

/* end of 'heavens' section */

int
setmap(int32 ramin, int32 ramax, int32 decmin, int32 decmax, Rectangle r, int zenithup)
{
	int c;
	double minx, maxx, miny, maxy;

	c = 1000*60*60;
	mapra = ramax/2+ramin/2;
	mapdec = decmax/2+decmin/2;

	if(zenithup)
		projection = heavens(mylat, mysid, (double)mapdec/c, (double)mapra/c);
	else{
		orient((double)mapdec/c, (double)mapra/c, 0.);
		projection = stereographic();
	}
	mapx0 = (r.max.x+r.min.x)/2;
	mapy0 = (r.max.y+r.min.y)/2;
	maps = ((double)Dy(r))/(double)(decmax-decmin);
	if(maptoxy(ramin, decmin, &minx, &miny) < 0)
		return -1;
	if(maptoxy(ramax, decmax, &maxx, &maxy) < 0)
		return -1;
	/*
	 * It's tricky to get the scale right.  This noble attempt scales
	 * to fit the lengths of the sides of the rectangle.
	 */
	mapscale = Dx(r)/(minx-maxx);
	if(mapscale > Dy(r)/(maxy-miny))
		mapscale = Dy(r)/(maxy-miny);
	/*
	 * near the pole It's not a rectangle, though, so this scales
	 * to fit the corners of the trapezoid (a triangle at the pole).
	 */
	mapscale *= (cos(angle(mapdec))+1.)/2;
	if(maxy  < miny){
		/* upside down, between zenith and pole */
		fprint(2, "reverse plot\n");
		mapscale = -mapscale;
	}
	return 1;
}

Point
map(int32 ra, int32 dec)
{
	double x, y;
	Point p;

	if(maptoxy(ra, dec, &x, &y) > 0){
		p.x = mapscale*x + mapx0;
		p.y = mapscale*-y + mapy0;
	}else{
		p.x = -100;
		p.y = -100;
	}
	return p;
}

int
dsize(int mag)	/* mag is 10*magnitude; return disc size */
{
	double d;

	mag += 25;	/* make mags all positive; sirius is -1.6m */
	d = (130-mag)/10;
	/* if plate scale is huge, adjust */
	if(maps < 100.0/MILLIARCSEC)
		d *= .71;
	if(maps < 50.0/MILLIARCSEC)
		d *= .71;
	return d;
}

void
drawname(Image *scr, Image *col, char *s, int ra, int dec)
{
	Point p;

	if(font == nil)
		return;
	p = addpt(map(ra, dec), Pt(4, -1));	/* font has huge ascent */
	string(scr, p, col, ZP, font, s);
}

int
npixels(DAngle diam)
{
	Point p0, p1;

	diam = DEG(angle(diam)*MILLIARCSEC);	/* diam in milliarcsec */
	diam /= 2;	/* radius */
	/* convert to plate scale */
	/* BUG: need +100 because we sometimes crash in library if we plot the exact center */
	p0 = map(mapra+100, mapdec);
	p1 = map(mapra+100+diam, mapdec);
	return hypot(p0.x-p1.x, p0.y-p1.y);
}

void
drawdisc(Image *scr, DAngle semidiam, int ring, Image *color, Point pt, char *s)
{
	int d;

	d = npixels(semidiam*2);
	if(d < 5)
		d = 5;
	fillellipse(scr, pt, d, d, color, ZP);
	if(ring == 1)
		ellipse(scr, pt, 5*d/2, d/2, 0, color, ZP);
	if(ring == 2)
		ellipse(scr, pt, d, d, 0, grey, ZP);
	if(s){
		d = stringwidth(font, s);
		pt.x -= d/2;
		pt.y -= font->height/2 + 1;
		string(scr, pt, display->black, ZP, font, s);
	}
}

void
drawplanet(Image *scr, Planetrec *p, Point pt)
{
	if(strcmp(p->name, "sun") == 0){
		drawdisc(scr, p->semidiam, 0, suncolor, pt, nil);
		return;
	}
	if(strcmp(p->name, "moon") == 0){
		drawdisc(scr, p->semidiam, 0, mooncolor, pt, nil);
		return;
	}
	if(strcmp(p->name, "shadow") == 0){
		drawdisc(scr, p->semidiam, 2, shadowcolor, pt, nil);
		return;
	}
	if(strcmp(p->name, "mercury") == 0){
		drawdisc(scr, p->semidiam, 0, mercurycolor, pt, "m");
		return;
	}
	if(strcmp(p->name, "venus") == 0){
		drawdisc(scr, p->semidiam, 0, venuscolor, pt, "v");
		return;
	}
	if(strcmp(p->name, "mars") == 0){
		drawdisc(scr, p->semidiam, 0, marscolor, pt, "M");
		return;
	}
	if(strcmp(p->name, "jupiter") == 0){
		drawdisc(scr, p->semidiam, 0, jupitercolor, pt, "J");
		return;
	}
	if(strcmp(p->name, "saturn") == 0){
		drawdisc(scr, p->semidiam, 1, saturncolor, pt, "S");

		return;
	}
	if(strcmp(p->name, "uranus") == 0){
		drawdisc(scr, p->semidiam, 0, uranuscolor, pt, "U");

		return;
	}
	if(strcmp(p->name, "neptune") == 0){
		drawdisc(scr, p->semidiam, 0, neptunecolor, pt, "N");

		return;
	}
	if(strcmp(p->name, "pluto") == 0){
		drawdisc(scr, p->semidiam, 0, plutocolor, pt, "P");

		return;
	}
	if(strcmp(p->name, "comet") == 0){
		drawdisc(scr, p->semidiam, 0, cometcolor, pt, "C");
		return;
	}

	pt.x -= stringwidth(font, p->name)/2;
	pt.y -= font->height/2;
	string(scr, pt, grey, ZP, font, p->name);
}

void
tolast(char *name)
{
	int i, nlast;
	Record *r, rr;

	/* stop early to simplify inner loop adjustment */
	nlast = 0;
	for(i=0,r=rec; i<nrec-nlast; i++,r++)
		if(r->type == Planet)
			if(name==nil || strcmp(r->u.planet.name, name)==0){
				rr = *r;
				memmove(rec+i, rec+i+1, (nrec-i-1)*sizeof(Record));
				rec[nrec-1] = rr;
				--i;
				--r;
				nlast++;
			}
}

int
bbox(int32 extrara, int32 extradec, int quantize)
{
	int i;
	Record *r;
	int ra, dec;
	int rah, ram, d1, d2;
	double r0;

	ramin = 0x7FFFFFFF;
	ramax = -0x7FFFFFFF;
	decmin = 0x7FFFFFFF;
	decmax = -0x7FFFFFFF;

	for(i=0,r=rec; i<nrec; i++,r++){
		if(r->type == Patch){
			radec(r->index, &rah, &ram, &dec);
			ra = 15*rah+ram/4;
			r0 = c/cos(dec*PI/180);
			ra *= c;
			dec *= c;
			if(dec == 0)
				d1 = c, d2 = c;
			else if(dec < 0)
				d1 = c, d2 = 0;
			else
				d1 = 0, d2 = c;
		}else if(r->type==SAO || r->type==NGC || r->type==Planet || r->type==Abell){
			ra = r->u.ngc.ra;
			dec = r->u.ngc.dec;
			d1 = 0, d2 = 0, r0 = 0;
		}else
			continue;
		if(dec+d2+extradec > decmax)
			decmax = dec+d2+extradec;
		if(dec-d1-extradec < decmin)
			decmin = dec-d1-extradec;
		if(folded){
			ra -= 180*c;
			if(ra < 0)
				ra += 360*c;
		}
		if(ra+r0+extrara > ramax)
			ramax = ra+r0+extrara;
		if(ra-extrara < ramin)
			ramin = ra-extrara;
	}

	if(decmax > 90*c)
		decmax = 90*c;
	if(decmin < -90*c)
		decmin = -90*c;
	if(ramin < 0)
		ramin += 360*c;
	if(ramax >= 360*c)
		ramax -= 360*c;

	if(quantize){
		/* quantize to degree boundaries */
		ramin -= ramin%m5;
		if(ramax%m5 != 0)
			ramax += m5-(ramax%m5);
		if(decmin > 0)
			decmin -= decmin%c;
		else
			decmin -= c - (-decmin)%c;
		if(decmax > 0){
			if(decmax%c != 0)
				decmax += c-(decmax%c);
		}else if(decmax < 0){
			if(decmax%c != 0)
				decmax += ((-decmax)%c);
		}
	}

	if(folded){
		if(ramax-ramin > 270*c){
			fprint(2, "ra range too wide %ld°\n", (ramax-ramin)/c);
			return -1;
		}
	}else if(ramax-ramin > 270*c){
		folded = 1;
		return bbox(0, 0, quantize);
	}

	return 1;
}

int
inbbox(DAngle ra, DAngle dec)
{
	int min;

	if(dec < decmin)
		return 0;
	if(dec > decmax)
		return 0;
	min = ramin;
	if(ramin > ramax){	/* straddling 0h0m */
		min -= 360*c;
		if(ra > 180*c)
			ra -= 360*c;
	}
	if(ra < min)
		return 0;
	if(ra > ramax)
		return 0;
	return 1;
}

int
gridra(int32 mapdec)
{
	mapdec = abs(mapdec)+c/2;
	mapdec /= c;
	if(mapdec < 60)
		return m5;
	if(mapdec < 80)
		return 2*m5;
	if(mapdec < 85)
		return 4*m5;
	return 8*m5;
}

#define	GREY	(nogrey? display->white : grey)

void
plot(char *flags)
{
	int i, j, k;
	char *t;
	int32 x, y;
	int ra, dec;
	int m;
	Point p, pts[10];
	Record *r;
	Rectangle rect, r1;
	int dx, dy, nogrid, textlevel, nogrey, zenithup;
	Image *scr;
	char *name, buf[32];
	double v;

	if(plotopen() < 0)
		return;
	nogrid = 0;
	nogrey = 0;
	textlevel = 1;
	dx = 512;
	dy = 512;
	zenithup = 0;
	for(;;){
		if(t = alpha(flags, "nogrid")){
			nogrid = 1;
			flags = t;
			continue;
		}
		if((t = alpha(flags, "zenith")) || (t = alpha(flags, "zenithup")) ){
			zenithup = 1;
			flags = t;
			continue;
		}
		if((t = alpha(flags, "notext")) || (t = alpha(flags, "nolabel")) ){
			textlevel = 0;
			flags = t;
			continue;
		}
		if((t = alpha(flags, "alltext")) || (t = alpha(flags, "alllabel")) ){
			textlevel = 2;
			flags = t;
			continue;
		}
		if(t = alpha(flags, "dx")){
			dx = strtol(t, &t, 0);
			if(dx < 100){
				fprint(2, "dx %d too small (min 100) in plot\n", dx);
				return;
			}
			flags = skipbl(t);
			continue;
		}
		if(t = alpha(flags, "dy")){
			dy = strtol(t, &t, 0);
			if(dy < 100){
				fprint(2, "dy %d too small (min 100) in plot\n", dy);
				return;
			}
			flags = skipbl(t);
			continue;
		}
		if((t = alpha(flags, "nogrey")) || (t = alpha(flags, "nogray"))){
			nogrey = 1;
			flags = skipbl(t);
			continue;
		}
		if(*flags){
			fprint(2, "syntax error in plot\n");
			return;
		}
		break;
	}
	flatten();
	folded = 0;

	if(bbox(0, 0, 1) < 0)
		return;
	if(ramax-ramin<100 || decmax-decmin<100){
		fprint(2, "plot too small\n");
		return;
	}

	scr = allocimage(display, Rect(0, 0, dx, dy), RGB24, 0, DBlack);
	if(scr == nil){
		fprint(2, "can't allocate image: %r\n");
		return;
	}
	rect = scr->r;
	rect.min.x += 16;
	rect = insetrect(rect, 40);
	if(setmap(ramin, ramax, decmin, decmax, rect, zenithup) < 0){
		fprint(2, "can't set up map coordinates\n");
		return;
	}
	if(!nogrid){
		for(x=ramin; ; ){
			for(j=0; j<nelem(pts); j++){
				/* use double to avoid overflow */
				v = (double)j / (double)(nelem(pts)-1);
				v = decmin + v*(decmax-decmin);
				pts[j] = map(x, v);
			}
			bezspline(scr, pts, nelem(pts), Endsquare, Endsquare, 0, GREY, ZP);
			ra = x;
			if(folded){
				ra -= 180*c;
				if(ra < 0)
					ra += 360*c;
			}
			p = pts[0];
			p.x -= stringwidth(font, hm5(angle(ra)))/2;
			string(scr, p, GREY, ZP, font, hm5(angle(ra)));
			p = pts[nelem(pts)-1];
			p.x -= stringwidth(font, hm5(angle(ra)))/2;
			p.y -= font->height;
			string(scr, p, GREY, ZP, font, hm5(angle(ra)));
			if(x == ramax)
				break;
			x += gridra(mapdec);
			if(x > ramax)
				x = ramax;
		}
		for(y=decmin; y<=decmax; y+=c){
			for(j=0; j<nelem(pts); j++){
				/* use double to avoid overflow */
				v = (double)j / (double)(nelem(pts)-1);
				v = ramin + v*(ramax-ramin);
				pts[j] = map(v, y);
			}
			bezspline(scr, pts, nelem(pts), Endsquare, Endsquare, 0, GREY, ZP);
			p = pts[0];
			p.x += 3;
			p.y -= font->height/2;
			string(scr, p, GREY, ZP, font, deg(angle(y)));
			p = pts[nelem(pts)-1];
			p.x -= 3+stringwidth(font, deg(angle(y)));
			p.y -= font->height/2;
			string(scr, p, GREY, ZP, font, deg(angle(y)));
		}
	}
	/* reorder to get planets in front of stars */
	tolast(nil);
	tolast("moon");		/* moon is in front of everything... */
	tolast("shadow");	/* ... except the shadow */

	for(i=0,r=rec; i<nrec; i++,r++){
		dec = r->u.ngc.dec;
		ra = r->u.ngc.ra;
		if(folded){
			ra -= 180*c;
			if(ra < 0)
				ra += 360*c;
		}
		if(textlevel){
			name = nameof(r);
			if(name==nil && textlevel>1 && r->type==SAO){
				snprint(buf, sizeof buf, "SAO%ld", r->index);
				name = buf;
			}
			if(name)
				drawname(scr, nogrey? display->white : alphagrey, name, ra, dec);
		}
		if(r->type == Planet){
			drawplanet(scr, &r->u.planet, map(ra, dec));
			continue;
		}
		if(r->type == SAO){
			m = r->u.sao.mag;
			if(m == UNKNOWNMAG)
				m = r->u.sao.mpg;
			if(m == UNKNOWNMAG)
				continue;
			m = dsize(m);
			if(m < 3)
				fillellipse(scr, map(ra, dec), m, m, nogrey? display->white : lightgrey, ZP);
			else{
				ellipse(scr, map(ra, dec), m+1, m+1, 0, display->black, ZP);
				fillellipse(scr, map(ra, dec), m, m, display->white, ZP);
			}
			continue;
		}
		if(r->type == Abell){
			ellipse(scr, addpt(map(ra, dec), Pt(-3, 2)), 2, 1, 0, lightblue, ZP);
			ellipse(scr, addpt(map(ra, dec), Pt(3, 2)), 2, 1, 0, lightblue, ZP);
			ellipse(scr, addpt(map(ra, dec), Pt(0, -2)), 1, 2, 0, lightblue, ZP);
			continue;
		}
		switch(r->u.ngc.type){
		case Galaxy:
			j = npixels(r->u.ngc.diam);
			if(j < 4)
				j = 4;
			if(j > 10)
				k = j/3;
			else
				k = j/2;
			ellipse(scr, map(ra, dec), j, k, 0, lightblue, ZP);
			break;

		case PlanetaryN:
			p = map(ra, dec);
			j = npixels(r->u.ngc.diam);
			if(j < 3)
				j = 3;
			ellipse(scr, p, j, j, 0, green, ZP);
			line(scr, Pt(p.x, p.y+j+1), Pt(p.x, p.y+j+4),
				Endsquare, Endsquare, 0, green, ZP);
			line(scr, Pt(p.x, p.y-(j+1)), Pt(p.x, p.y-(j+4)),
				Endsquare, Endsquare, 0, green, ZP);
			line(scr, Pt(p.x+j+1, p.y), Pt(p.x+j+4, p.y),
				Endsquare, Endsquare, 0, green, ZP);
			line(scr, Pt(p.x-(j+1), p.y), Pt(p.x-(j+4), p.y),
				Endsquare, Endsquare, 0, green, ZP);
			break;

		case DiffuseN:
		case NebularCl:
			p = map(ra, dec);
			j = npixels(r->u.ngc.diam);
			if(j < 4)
				j = 4;
			r1.min = Pt(p.x-j, p.y-j);
			r1.max = Pt(p.x+j, p.y+j);
			if(r->u.ngc.type != DiffuseN)
				draw(scr, r1, ocstipple, ocstipple, ZP);
			line(scr, Pt(p.x-j, p.y-j), Pt(p.x+j, p.y-j),
				Endsquare, Endsquare, 0, green, ZP);
			line(scr, Pt(p.x-j, p.y+j), Pt(p.x+j, p.y+j),
				Endsquare, Endsquare, 0, green, ZP);
			line(scr, Pt(p.x-j, p.y-j), Pt(p.x-j, p.y+j),
				Endsquare, Endsquare, 0, green, ZP);
			line(scr, Pt(p.x+j, p.y-j), Pt(p.x+j, p.y+j),
				Endsquare, Endsquare, 0, green, ZP);
			break;

		case OpenCl:
			p = map(ra, dec);
			j = npixels(r->u.ngc.diam);
			if(j < 4)
				j = 4;
			fillellipse(scr, p, j, j, ocstipple, ZP);
			break;

		case GlobularCl:
			j = npixels(r->u.ngc.diam);
			if(j < 4)
				j = 4;
			p = map(ra, dec);
			ellipse(scr, p, j, j, 0, lightgrey, ZP);
			line(scr, Pt(p.x-(j-1), p.y), Pt(p.x+j, p.y),
				Endsquare, Endsquare, 0, lightgrey, ZP);
			line(scr, Pt(p.x, p.y-(j-1)), Pt(p.x, p.y+j),
				Endsquare, Endsquare, 0, lightgrey, ZP);
			break;

		}
	}
	flushimage(display, 1);
	displayimage(scr);
}

int
runcommand(char *command, int p[2])
{
	switch(rfork(RFPROC|RFFDG|RFNOWAIT)){
	case -1:
		return -1;
	default:
		break;
	case 0:
		dup(p[1], 1);
		close(p[0]);
		close(p[1]);
		execlp("rc", "rc", "-c", command, nil);
		fprint(2, "can't exec %s: %r", command);
		exits(nil);
	}
	return 1;
}

void
parseplanet(char *line, Planetrec *p)
{
	char *fld[6];
	int i, nfld;
	char *s;

	if(line[0] == '\0')
		return;
	line[10] = '\0';	/* terminate name */
	s = strrchr(line, ' ');
	if(s == nil)
		s = line;
	else
		s++;
	strcpy(p->name, s);
	for(i=0; s[i]!='\0'; i++)
		p->name[i] = tolower(s[i]);
	p->name[i] = '\0';
	nfld = getfields(line+11, fld, nelem(fld), 1, " ");
	p->ra = dangle(getra(fld[0]));
	p->dec = dangle(getra(fld[1]));
	p->az = atof(fld[2])*MILLIARCSEC;
	p->alt = atof(fld[3])*MILLIARCSEC;
	p->semidiam = atof(fld[4])*1000;
	if(nfld > 5)
		p->phase = atof(fld[5]);
	else
		p->phase = 0;
}

void
astro(char *flags, int initial)
{
	int p[2];
	int i, n, np;
	char cmd[256], buf[4096], *lines[20], *fld[10];

	snprint(cmd, sizeof cmd, "astro -p %s", flags);
	if(pipe(p) < 0){
		fprint(2, "can't pipe: %r\n");
		return;
	}
	if(runcommand(cmd, p) < 0){
		close(p[0]);
		close(p[1]);
		fprint(2, "can't run astro: %r");
		return;
	}
	close(p[1]);
	n = readn(p[0], buf, sizeof buf-1);
	if(n <= 0){
		fprint(2, "no data from astro\n");
		return;
	}
	if(!initial)
		Bwrite(&bout, buf, n);
	buf[n] = '\0';
	np = getfields(buf, lines, nelem(lines), 0, "\n");
	if(np <= 1){
		fprint(2, "astro: not enough output\n");
		return;
	}
	Bprint(&bout, "%s\n", lines[0]);
	Bflush(&bout);
	/* get latitude and longitude */
	if(getfields(lines[0], fld, nelem(fld), 1, " ") < 8)
		fprint(2, "astro: can't read longitude: too few fields\n");
	else{
		mysid = getra(fld[5])*180./PI;
		mylat = getra(fld[6])*180./PI;
		mylon = getra(fld[7])*180./PI;
	}
	/*
	 * Each time we run astro, we generate a new planet list
	 * so multiple appearances of a planet may exist as we plot
	 * its motion over time.
	 */
	planet = malloc(NPlanet*sizeof planet[0]);
	if(planet == nil){
		fprint(2, "astro: malloc failed: %r\n");
		exits("malloc");
	}
	memset(planet, 0, NPlanet*sizeof planet[0]);
	for(i=1; i<np; i++)
		parseplanet(lines[i], &planet[i-1]);
}
