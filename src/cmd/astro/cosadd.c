#include "astro.h"


void
icosadd(double *fp, char *cp)
{

	cafp = fp;
	cacp = cp;
}

double
cosadd(int n, double coef, ...)
{
	double *coefp;
	char *cp;
	int i;
	double sum, a1, a2;

	sum = 0;
	cp = cacp;

loop:
	a1 = *cafp++;
	if(a1 == 0) {
		cacp = cp;
		return sum;
	}
	a2 = *cafp++;
	i = n;
	coefp = &coef;
	do
		a2 += *cp++ * *coefp++;
	while(--i);
	sum += a1 * cos(a2);
	goto loop;
}

double
sinadd(int n, double coef, ...)
{
	double *coefp;
	char *cp;
	int i;
	double sum, a1, a2;

	sum = 0;
	cp = cacp;

loop:
	a1 = *cafp++;
	if(a1 == 0) {
		cacp = cp;
		return sum;
	}
	a2 = *cafp++;
	i = n;
	coefp = &coef;
	do
		a2 += *cp++ * *coefp++;
	while(--i);
	sum += a1 * sin(a2);
	goto loop;
}
