#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#include	"sky.h"

char	rad28[] = "0123456789abcdefghijklmnopqr";

Picture*
image(Angle ra, Angle dec, Angle wid, Angle hig)
{
	Pix *p;
	uchar *b, *up;
	int i, j, sx, sy, x, y;
	char file[50];
	Picture *pic;
	Img* ip;
	int lowx, lowy, higx, higy;
	int slowx, slowy, shigx, shigy;
	Header *h;
	Angle d, bd;
	Plate *pp, *bp;

	if(gam.gamma == 0)
		gam.gamma = -1;
	if(gam.max == gam.min) {
		gam.max = 17600;
		gam.min = 2500;
	}
	gam.absgamma = gam.gamma;
	gam.neg = 0;
	if(gam.absgamma < 0) {
		gam.absgamma = -gam.absgamma;
		gam.neg = 1;
	}
	gam.mult1 = 1. / (gam.max - gam.min);
	gam.mult2 = 255. * gam.mult1;

	if(nplate == 0)
		getplates();

	bp = 0;
	bd = 0;
	for(i=0; i<nplate; i++) {
		pp = &plate[i];
		d = dist(ra, dec, pp->ra, pp->dec);
		if(bp == 0 || d < bd) {
			bp = pp;
			bd = d;
		}
	}

	if(debug)
		Bprint(&bout, "best plate: %s %s disk %d %s\n",
			hms(bp->ra), dms(bp->dec),
			bp->disk, bp->rgn);

	h = getheader(bp->rgn);
	xypos(h, ra, dec, 0, 0);
	if(wid <= 0 || hig <= 0) {
		lowx = h->x;
		lowy = h->y;
		lowx = (lowx/500) * 500;
		lowy = (lowy/500) * 500;
		higx = lowx + 500;
		higy = lowy + 500;
	} else {
		lowx = h->x - wid*ARCSECONDS_PER_RADIAN*1000 /
			(h->param[Pxpixelsz]*h->param[Ppltscale]*2);
		lowy = h->y - hig*ARCSECONDS_PER_RADIAN*1000 /
			(h->param[Pypixelsz]*h->param[Ppltscale]*2);
		higx = h->x + wid*ARCSECONDS_PER_RADIAN*1000 /
			(h->param[Pxpixelsz]*h->param[Ppltscale]*2);
		higy = h->y + hig*ARCSECONDS_PER_RADIAN*1000 /
			(h->param[Pypixelsz]*h->param[Ppltscale]*2);
	}
	free(h);

	if(lowx < 0) lowx = 0;
	if(higx < 0) higx = 0;
	if(lowy < 0) lowy = 0;
	if(higy < 0) higy = 0;
	if(lowx > 14000) lowx = 14000;
	if(higx > 14000) higx = 14000;
	if(lowy > 14000) lowy = 14000;
	if(higy > 14000) higy = 14000;

	if(debug)
		Bprint(&bout, "xy on plate: %d,%d %d,%d\n",
			lowx,lowy, higx, higy);

	if(lowx >= higx || lowy >=higy) {
		Bprint(&bout, "no image found\n");
		return 0;
	}

	b = malloc((higx-lowx)*(higy-lowy)*sizeof(*b));
	if(b == 0) {
 emalloc:
		fprint(2, "malloc error\n");
		return 0;
	}
	memset(b, 0, (higx-lowx)*(higy-lowy)*sizeof(*b));

	slowx = lowx/500;
	shigx = (higx-1)/500;
	slowy = lowy/500;
	shigy = (higy-1)/500;

	for(sx=slowx; sx<=shigx; sx++)
	for(sy=slowy; sy<=shigy; sy++) {
		if(sx < 0 || sx >= nelem(rad28) || sy < 0 || sy >= nelem(rad28)) {
			fprint(2, "bad subplate %d %d\n", sy, sx);
			free(b);
			return 0;
		}
		sprint(file, "%s/%s/%s.%c%c",
			dssmount(bp->disk),
			bp->rgn, bp->rgn,
			rad28[sy],
			rad28[sx]);

		ip = dssread(file);
		if(ip == 0) {
			fprint(2, "can't read %s: %r\n", file);
			free(b);
			return 0;
		}

		x = sx*500;
		y = sy*500;
		for(j=0; j<ip->ny; j++) {
			if(y+j < lowy || y+j >= higy)
				continue;
			p = &ip->a[j*ip->ny];
			up = b + (higy - (y+j+1))*(higx-lowx) + (x - lowx);
			for(i=0; i<ip->nx; i++) {
				if(x+i >= lowx && x+i < higx)
					*up = dogamma(*p);
				up++;
				p += 1;
			}
		}
		free(ip);
	}

	pic = malloc(sizeof(Picture));
	if(pic == 0){
		free(b);
		goto emalloc;
	}
	pic->minx = lowx;
	pic->miny = lowy;
	pic->maxx = higx;
	pic->maxy = higy;
	pic->data = b;
	strcpy(pic->name, bp->rgn);
	return pic;
}
