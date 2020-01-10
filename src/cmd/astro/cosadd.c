#include "astro.h"


void
icosadd(double *fp, char *cp)
{

	cafp = fp;
	cacp = cp;
}

double
cosadd(int n, ...)
{
	double *coefp, coef[10];
	char *cp;
	int i;
	double sum, a1, a2;
	va_list arg;

	sum = 0;
	cp = cacp;
	va_start(arg, n);
	for(i=0; i<n; i++)
		coef[i] = va_arg(arg, double);
	va_end(arg);

loop:
	a1 = *cafp++;
	if(a1 == 0) {
		cacp = cp;
		return sum;
	}
	a2 = *cafp++;
	i = n;
	coefp = coef;
	do
		a2 += *cp++ * *coefp++;
	while(--i);
	sum += a1 * cos(a2);
	goto loop;
}

double
sinadd(int n, ...)
{
	double *coefp, coef[10];
	char *cp;
	int i;
	double sum, a1, a2;
	va_list arg;

	sum = 0;
	cp = cacp;
	va_start(arg, n);
	for(i=0; i<n; i++)
		coef[i] = va_arg(arg, double);
	va_end(arg);

loop:
	a1 = *cafp++;
	if(a1 == 0) {
		cacp = cp;
		return sum;
	}
	a2 = *cafp++;
	i = n;
	coefp = coef;
	do
		a2 += *cp++ * *coefp++;
	while(--i);
	sum += a1 * sin(a2);
	goto loop;
}
