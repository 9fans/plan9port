#include "mplot.h"
void plotpoly(int num[], double *ff[]){
	double *xp, *yp, **fp;
	int i, *n;
	n = num;
	fp = ff;
	while((i = *n++)){
		xp = *fp++;
		yp = xp+1;
		move(*xp, *yp);
		while(--i){
			xp += 2;
			yp += 2;
			vec(*xp, *yp);
		}
	}
}
