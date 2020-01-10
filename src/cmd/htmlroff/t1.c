#include "a.h"

/*
 * Section 1 - General Explanation.
 */

/* 1.3 - Numerical parameter input.  */
char *units = "icPmnpuvx";
int
scale2units(char c)
{
	int x;

	switch(c){
	case 'i':	/* inch */
		return UPI;
	case 'c':	/* centimeter */
		return 0.3937008 * UPI;
	case 'P':	/* pica = 1/6 inch */
		return UPI / 6;
	case 'm':	/* em = S points */
		return UPI / 72.0 * getnr(L(".s"));
	case 'n':	/* en = em/2 */
		return UPI / 72.0 * getnr(L(".s")) / 2;
	case 'p':	/* point = 1/72 inch */
		return UPI / 72;
	case 'u':	/* basic unit */
		return 1;
	case 'v':	/* vertical line space V */
		x = getnr(L(".v"));
		if(x == 0)
			x = 12 * UPI / 72;
		return x;
	case 'x':	/* pixel (htmlroff addition) */
		return UPX;
	default:
		return 1;
	}
}

/* 1.4 - Numerical expressions. */
int eval0(Rune**, int, int);
int
eval(Rune *s)
{
	return eval0(&s, 1, 1);
}
long
runestrtol(Rune *a, Rune **p)
{
	long n;

	n = 0;
	while('0' <= *a && *a <= '9'){
		n = n*10 + *a-'0';
		a++;
	}
	*p = a;
	return n;
}

int
evalscale(Rune *s, int c)
{
	return eval0(&s, scale2units(c), 1);
}

int
eval0(Rune **pline, int scale, int recur)
{
	Rune *p;
	int neg;
	double f, p10;
	int x, y;

	neg = 0;
	p = *pline;
	while(*p == '-'){
		neg = 1 - neg;
		p++;
	}
	if(*p == '('){
		p++;
		x = eval0(&p, scale, 1);
		if (*p != ')'){
			*pline = p;
			return x;
		}
		p++;
	}else{
		f = runestrtol(p, &p);
		if(*p == '.'){
			p10 = 1.0;
			p++;
			while('0' <= *p && *p <= '9'){
				p10 /= 10;
				f += p10*(*p++ - '0');
			}
		}
		if(*p && strchr(units, *p)){
			if(scale)
				f *= scale2units(*p);
			p++;
		}else if(scale)
			f *= scale;
		x = f;
	}
	if(neg)
		x = -x;
	if(!recur){
		*pline = p;
		return x;
	}

	while(*p){
		switch(*p++) {
		case '+':
			x += eval0(&p, scale, 0);
			continue;
		case '-':
			x -= eval0(&p, scale, 0);
			continue;
		case '*':
			x *= eval0(&p, scale, 0);
			continue;
		case '/':
			y = eval0(&p, scale, 0);
			if (y == 0) {
				fprint(2, "%L: divide by zero %S\n", p);
				y = 1;
			}
			x /= y;
			continue;
		case '%':
			y = eval0(&p, scale, 0);
			if (!y) {
				fprint(2, "%L: modulo by zero %S\n", p);
				y = 1;
			}
			x %= y;
			continue;
		case '<':
			if (*p == '=') {
				p++;
				x = x <= eval0(&p, scale, 0);
				continue;
			}
			x = x < eval0(&p, scale, 0);
			continue;
		case '>':
			if (*p == '=') {
				p++;
				x = x >= eval0(&p, scale, 0);
				continue;
			}
			x = x > eval0(&p, scale, 0);
			continue;
		case '=':
			if (*p == '=')
				p++;
			x = x == eval0(&p, scale, 0);
			continue;
		case '&':
			x &= eval0(&p, scale, 0);
			continue;
		case ':':
			x |= eval0(&p, scale, 0);
			continue;
		}
	}
	*pline = p;
	return x;
}

void
t1init(void)
{
	Tm tm;

	tm = *localtime(time(0));
	nr(L("dw"), tm.wday+1);
	nr(L("dy"), tm.mday);
	nr(L("mo"), tm.mon);
	nr(L("yr"), tm.year%100);
}
