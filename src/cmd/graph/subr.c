#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "iplot.h"
void putnum(int num[], double *ff[]){
	double **fp, *xp;
	int *np, n,i;
	np = num;
	fp = ff;
	while( (n = *np++)){
		xp = *fp++;
		printf("{ ");
		for(i=0; i<n;i++){
			printf("%g %g ",*xp, *(xp+1));
			if(i&1)printf("\n");
		}
		printf("}\n");
	}
}
