/*
Produce spline (uniform knots, second order)
from guiding points
*/
#include "mplot.h"
void splin(int mode, int num[], double *ff[]){
	int	i,  *np, n;
	double	xa, ya, xc, yc, *xp, *yp, *xp0, *yp0, *xpe, *ype;
	double **fp;
	np = num;
	fp = ff;
	while((n = *np++)){
		xp = *fp++;
		yp = xp + 1;
		xp0 = xp;
		yp0 = yp;
		xpe = xp0 + 2 * (n - 1);
		ype = yp0 + 2 * (n - 1);
		if (n < 3) {
			plotline(*xp, *yp, *(xp + 2), *(yp + 2));
			continue;
		}
		if (mode == 4) {	/*closed curve*/
			xa = 0.5 * (*xpe + *(xpe - 2));
			xc = 0.5 * (*xpe + *xp0);
			ya = 0.5 * (*ype + *(ype - 2));
			yc = 0.5 * (*ype + *yp0);
			parabola(xa, ya, xc, yc, *xpe, *ype);
			xa = 0.5 * (*xpe + *xp0);
			xc = 0.5 * (*(xp0 + 2) + *xp0);
			ya = 0.5 * (*ype + *yp0);
			yc = 0.5 * (*(yp0 + 2) + *yp0);
			parabola(xa, ya, xc, yc, *xp0, *yp0);
		}
		else {	/*open curve with multiple endpoints*/
			if (mode % 2) /*odd mode makes first point double*/
				plotline(*xp0,*yp0,0.5*(*xp0+*(xp0+2)),0.5*(*yp0+*(yp0+2)));
		}
		xp += 2;
		yp += 2;
		for (i = 1; i < (n - 1); i++, xp += 2, yp += 2) {
			xa = 0.5 * (*(xp - 2) + *xp);
			xc = 0.5 * ( *xp + *(xp + 2));
			ya = 0.5 * (*(yp - 2) + *yp);
			yc = 0.5 * ( *yp + *(yp + 2));
			parabola(xa, ya, xc, yc, *xp, *yp);
		}
		if(mode >= 2 && mode != 4)
			plotline(0.5*(*(xpe-2)+*xpe),0.5*(*(ype-2)+*ype),*xpe,*ype);
	}
}
