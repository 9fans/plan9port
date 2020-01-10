/*
 * troff4.c
 *
 * number registers, conversion, arithmetic
 */

#include "tdef.h"
#include "fns.h"
#include "ext.h"


int	regcnt = NNAMES;
int	falsef	= 0;	/* on if inside false branch of if */

#define	NHASHSIZE	128	/* must be 2**n */
#define	NHASH(i)	((i>>6)^i) & (NHASHSIZE-1)
Numtab	*nhash[NHASHSIZE];

Numtab *numtabp = NULL;
#define NDELTA 400
int ncnt = 0;

void setn(void)
{
	int i, j, f;
	Tchar ii;
	Uchar *p;
	char buf[NTM];		/* for \n(.S */

	f = nform = 0;
	if ((i = cbits(ii = getach())) == '+')
		f = 1;
	else if (i == '-')
		f = -1;
	else if (ii)	/* don't put it back if it's already back (thanks to jaap) */
		ch = ii;
	if (falsef)
		f = 0;
	if ((i = getsn()) == 0)
		return;
	p = unpair(i);
	if (p[0] == '.')
		switch (p[1]) {
		case 's':
			i = pts;
			break;
		case 'v':
			i = lss;
			break;
		case 'f':
			i = font;
			break;
		case 'p':
			i = pl;
			break;
		case 't':
			i = findt1();
			break;
		case 'o':
			i = po;
			break;
		case 'l':
			i = ll;
			break;
		case 'i':
			i = in;
			break;
		case '$':
			i = frame->nargs;
			break;
		case 'A':
			i = ascii;
			break;
		case 'c':
			i = numtabp[CD].val;
			break;
		case 'n':
			i = lastl;
			break;
		case 'a':
			i = ralss;
			break;
		case 'h':
			i = dip->hnl;
			break;
		case 'd':
			if (dip != d)
				i = dip->dnl;
			else
				i = numtabp[NL].val;
			break;
		case 'u':
			i = fi;
			break;
		case 'j':
			i = ad + 2 * admod;
			break;
		case 'w':
			i = widthp;
			break;
		case 'x':
			i = nel;
			break;
		case 'y':
			i = un;
			break;
		case 'T':
			i = dotT;
			break;	 /* -Tterm used in nroff */
		case 'V':
			i = VERT;
			break;
		case 'H':
			i = HOR;
			break;
		case 'k':
			i = ne;
			break;
		case 'P':
			i = print;
			break;
		case 'L':
			i = ls;
			break;
		case 'R':	/* maximal # of regs that can be addressed */
			i = 255*256 - regcnt;
			break;
		case 'z':
			p = unpair(dip->curd);
			*pbp++ = p[1];	/* watch order */
			*pbp++ = p[0];
			return;
		case 'b':
			i = bdtab[font];
			break;
		case 'F':
			cpushback(cfname[ifi]);
			return;
 		case 'S':
 			buf[0] = j = 0;
 			for( i = 0; tabtab[i] != 0 && i < NTAB; i++) {
 				if (i > 0)
 					buf[j++] = ' ';
 				sprintf(&buf[j], "%ld", tabtab[i] & TABMASK);
 				j = strlen(buf);
 				if ( tabtab[i] & RTAB)
 					sprintf(&buf[j], "uR");
 				else if (tabtab[i] & CTAB)
 					sprintf(&buf[j], "uC");
 				else
 					sprintf(&buf[j], "uL");
 				j += 2;
 			}
 			cpushback(buf);
 			return;
		default:
			goto s0;
		}
	else {
s0:
		if ((j = findr(i)) == -1)
			i = 0;
		else {
			i = numtabp[j].val = numtabp[j].val + numtabp[j].inc * f;
			nform = numtabp[j].fmt;
		}
	}
	setn1(i, nform, (Tchar) 0);
}

Tchar	numbuf[25];
Tchar	*numbufp;

int wrc(Tchar i)
{
	if (numbufp >= &numbuf[24])
		return(0);
	*numbufp++ = i;
	return(1);
}



/* insert into input number i, in format form, with size-font bits bits */
void setn1(int i, int form, Tchar bits)
{
	numbufp = numbuf;
	nrbits = bits;
	nform = form;
	fnumb(i, wrc);
	*numbufp = 0;
	pushback(numbuf);
}

void prnumtab(Numtab *p)
{
	int i;
	for (i = 0; i < ncnt; i++)
		if (p)
			if (p[i].r != 0)
				fprintf(stderr, "slot %d, %s, val %d\n", i, unpair(p[i].r), p[i].val);
			else
				fprintf(stderr, "slot %d empty\n", i);
		else
			fprintf(stderr, "slot %d empty\n", i);
}

void nnspace(void)
{
	ncnt = sizeof(numtab)/sizeof(Numtab) + NDELTA;
	numtabp = (Numtab *) grow((char *)numtabp, ncnt, sizeof(Numtab));
	if (numtabp == NULL) {
		ERROR "not enough memory for registers (%d)", ncnt WARN;
		exit(1);
	}
	numtabp = (Numtab *) memcpy((char *)numtabp, (char *)numtab,
							sizeof(numtab));
	if (numtabp == NULL) {
		ERROR "Cannot initialize registers" WARN;
		exit(1);
	}
}

void grownumtab(void)
{
	ncnt += NDELTA;
	numtabp = (Numtab *) grow((char *) numtabp, ncnt, sizeof(Numtab));
	if (numtabp == NULL) {
		ERROR "Too many number registers (%d)", ncnt WARN;
		done2(04);
	} else {
		memset((char *)(numtabp) + (ncnt - NDELTA) * sizeof(Numtab),
						0, NDELTA * sizeof(Numtab));
		nrehash();
	}
}

void nrehash(void)
{
	Numtab *p;
	int i;

	for (i=0; i<NHASHSIZE; i++)
		nhash[i] = 0;
	for (p=numtabp; p < &numtabp[ncnt]; p++)
		p->link = 0;
	for (p=numtabp; p < &numtabp[ncnt]; p++) {
		if (p->r == 0)
			continue;
		i = NHASH(p->r);
		p->link = nhash[i];
		nhash[i] = p;
	}
}

void nunhash(Numtab *rp)
{
	Numtab *p;
	Numtab **lp;

	if (rp->r == 0)
		return;
	lp = &nhash[NHASH(rp->r)];
	p = *lp;
	while (p) {
		if (p == rp) {
			*lp = p->link;
			p->link = 0;
			return;
		}
		lp = &p->link;
		p = p->link;
	}
}

int findr(int i)
{
	Numtab *p;
	int h = NHASH(i);

	if (i == 0)
		return(-1);
a0:
	for (p = nhash[h]; p; p = p->link)
		if (i == p->r)
			return(p - numtabp);
	for (p = numtabp; p < &numtabp[ncnt]; p++) {
		if (p->r == 0) {
			p->r = i;
			p->link = nhash[h];
			nhash[h] = p;
			regcnt++;
			return(p - numtabp);
		}
	}
	grownumtab();
	goto a0;
}

int usedr(int i)	/* returns -1 if nr i has never been used */
{
	Numtab *p;

	if (i == 0)
		return(-1);
	for (p = nhash[NHASH(i)]; p; p = p->link)
		if (i == p->r)
			return(p - numtabp);
	return -1;
}


int fnumb(int i, int (*f)(Tchar))
{
	int j;

	j = 0;
	if (i < 0) {
		j = (*f)('-' | nrbits);
		i = -i;
	}
	switch (nform) {
	default:
	case '1':
	case 0:
		return decml(i, f) + j;
	case 'i':
	case 'I':
		return roman(i, f) + j;
	case 'a':
	case 'A':
		return abc(i, f) + j;
	}
}


int decml(int i, int (*f)(Tchar))
{
	int j, k;

	k = 0;
	nform--;
	if ((j = i / 10) || (nform > 0))
		k = decml(j, f);
	return(k + (*f)((i % 10 + '0') | nrbits));
}


int roman(int i, int (*f)(Tchar))
{

	if (!i)
		return((*f)('0' | nrbits));
	if (nform == 'i')
		return(roman0(i, f, "ixcmz", "vldw"));
	else
		return(roman0(i, f, "IXCMZ", "VLDW"));
}


int roman0(int i, int (*f)(Tchar), char *onesp, char *fivesp)
{
	int q, rem, k;

	if (!i)
		return(0);
	k = roman0(i / 10, f, onesp + 1, fivesp + 1);
	q = (i = i % 10) / 5;
	rem = i % 5;
	if (rem == 4) {
		k += (*f)(*onesp | nrbits);
		if (q)
			i = *(onesp + 1);
		else
			i = *fivesp;
		return(k += (*f)(i | nrbits));
	}
	if (q)
		k += (*f)(*fivesp | nrbits);
	while (--rem >= 0)
		k += (*f)(*onesp | nrbits);
	return(k);
}


int abc(int i, int (*f)(Tchar))
{
	if (!i)
		return((*f)('0' | nrbits));
	else
		return(abc0(i - 1, f));
}


int abc0(int i, int (*f)(Tchar))
{
	int j, k;

	k = 0;
	if (j = i / 26)
		k = abc0(j - 1, f);
	return(k + (*f)((i % 26 + nform) | nrbits));
}

long atoi0(void)
{
	int c, k, cnt;
	Tchar ii;
	long i, acc;

	acc = 0;
	nonumb = 0;
	cnt = -1;
a0:
	cnt++;
	ii = getch();
	c = cbits(ii);
	switch (c) {
	default:
		ch = ii;
		if (cnt)
			break;
	case '+':
		i = ckph();
		if (nonumb)
			break;
		acc += i;
		goto a0;
	case '-':
		i = ckph();
		if (nonumb)
			break;
		acc -= i;
		goto a0;
	case '*':
		i = ckph();
		if (nonumb)
			break;
		acc *= i;
		goto a0;
	case '/':
		i = ckph();
		if (nonumb)
			break;
		if (i == 0) {
			flusho();
			ERROR "divide by zero." WARN;
			acc = 0;
		} else
			acc /= i;
		goto a0;
	case '%':
		i = ckph();
		if (nonumb)
			break;
		acc %= i;
		goto a0;
	case '&':	/*and*/
		i = ckph();
		if (nonumb)
			break;
		if ((acc > 0) && (i > 0))
			acc = 1;
		else
			acc = 0;
		goto a0;
	case ':':	/*or*/
		i = ckph();
		if (nonumb)
			break;
		if ((acc > 0) || (i > 0))
			acc = 1;
		else
			acc = 0;
		goto a0;
	case '=':
		if (cbits(ii = getch()) != '=')
			ch = ii;
		i = ckph();
		if (nonumb) {
			acc = 0;
			break;
		}
		if (i == acc)
			acc = 1;
		else
			acc = 0;
		goto a0;
	case '>':
		k = 0;
		if (cbits(ii = getch()) == '=')
			k++;
		else
			ch = ii;
		i = ckph();
		if (nonumb) {
			acc = 0;
			break;
		}
		if (acc > (i - k))
			acc = 1;
		else
			acc = 0;
		goto a0;
	case '<':
		k = 0;
		if (cbits(ii = getch()) == '=')
			k++;
		else
			ch = ii;
		i = ckph();
		if (nonumb) {
			acc = 0;
			break;
		}
		if (acc < (i + k))
			acc = 1;
		else
			acc = 0;
		goto a0;
	case ')':
		break;
	case '(':
		acc = atoi0();
		goto a0;
	}
	return(acc);
}


long ckph(void)
{
	Tchar i;
	long j;

	if (cbits(i = getch()) == '(')
		j = atoi0();
	else {
		j = atoi1(i);
	}
	return(j);
}


/*
 * print error about illegal numeric argument;
 */
void prnumerr(void)
{
	char err_buf[40];
	static char warn[] = "Numeric argument expected";
	int savcd = numtabp[CD].val;

	if (numerr.type == RQERR)
		sprintf(err_buf, "%c%s: %s", nb ? cbits(c2) : cbits(cc),
						unpair(numerr.req), warn);
	else
		sprintf(err_buf, "\\%c'%s': %s", numerr.esc, &numerr.escarg,
									warn);
	if (frame != stk)	/* uncertainty correction */
		numtabp[CD].val--;
	ERROR "%s", err_buf WARN;
	numtabp[CD].val = savcd;
}


long atoi1(Tchar ii)
{
	int i, j, digits;
	double acc;	/* this is the only double in troff! */
	int neg, abs, field, decpnt;
	extern int ifnum;


	neg = abs = field = decpnt = digits = 0;
	acc = 0;
	for (;;) {
		i = cbits(ii);
		switch (i) {
		default:
			break;
		case '+':
			ii = getch();
			continue;
		case '-':
			neg = 1;
			ii = getch();
			continue;
		case '|':
			abs = 1 + neg;
			neg = 0;
			ii = getch();
			continue;
		}
		break;
	}
a1:
	while (i >= '0' && i <= '9') {
		field++;
		digits++;
		acc = 10 * acc + i - '0';
		ii = getch();
		i = cbits(ii);
	}
	if (i == '.' && !decpnt++) {
		field++;
		digits = 0;
		ii = getch();
		i = cbits(ii);
		goto a1;
	}
	if (!field) {
		ch = ii;
		goto a2;
	}
	switch (i) {
	case 'u':
		i = j = 1;	/* should this be related to HOR?? */
		break;
	case 'v':	/*VSs - vert spacing*/
		j = lss;
		i = 1;
		break;
	case 'm':	/*Ems*/
		j = EM;
		i = 1;
		break;
	case 'n':	/*Ens*/
		j = EM;
		if (TROFF)
			i = 2;
		else
			i = 1;	/*Same as Ems in NROFF*/
		break;
	case 'p':	/*Points*/
		j = INCH;
		i = 72;
		break;
	case 'i':	/*Inches*/
		j = INCH;
		i = 1;
		break;
	case 'c':	/*Centimeters*/
		/* if INCH is too big, this will overflow */
		j = INCH * 50;
		i = 127;
		break;
	case 'P':	/*Picas*/
		j = INCH;
		i = 6;
		break;
	default:
		j = dfact;
		ch = ii;
		i = dfactd;
	}
	if (neg)
		acc = -acc;
	if (!noscale) {
		acc = (acc * j) / i;
	}
	if (field != digits && digits > 0)
		while (digits--)
			acc /= 10;
	if (abs) {
		if (dip != d)
			j = dip->dnl;
		else
			j = numtabp[NL].val;
		if (!vflag) {
			j = numtabp[HP].val;
		}
		if (abs == 2)
			j = -j;
		acc -= j;
	}
a2:
	nonumb = (!field || field == decpnt);
	if (nonumb && (trace & TRNARGS) && !ismot(ii) && !nlflg && !ifnum) {
		if (cbits(ii) != RIGHT ) /* Too painful to do right */
			prnumerr();
	}
	return(acc);
}


void caserr(void)
{
	int i, j;
	Numtab *p;

	lgf++;
	while (!skip() && (i = getrq()) ) {
		j = usedr(i);
		if (j < 0)
			continue;
		p = &numtabp[j];
		nunhash(p);
		p->r = p->val = p->inc = p->fmt = 0;
		regcnt--;
	}
}

/*
 * .nr request; if tracing, don't check optional
 * 2nd argument because tbl generates .in 1.5n
 */
void casenr(void)
{
	int i, j;
	int savtr = trace;

	lgf++;
	skip();
	if ((i = findr(getrq())) == -1)
		goto rtn;
	skip();
	j = inumb(&numtabp[i].val);
	if (nonumb)
		goto rtn;
	numtabp[i].val = j;
	skip();
	trace = 0;
	j = atoi0();		/* BUG??? */
	trace = savtr;
	if (nonumb)
		goto rtn;
	numtabp[i].inc = j;
rtn:
	return;
}

void caseaf(void)
{
	int i, k;
	Tchar j;

	lgf++;
	if (skip() || !(i = getrq()) || skip())
		return;
	k = 0;
	j = getch();
	if (!isalpha(cbits(j))) {
		ch = j;
		while ((j = cbits(getch())) >= '0' &&  j <= '9')
			k++;
	}
	if (!k)
		k = j;
	numtabp[findr(i)].fmt = k;	/* was k & BYTEMASK */
}

void setaf(void)	/* return format of number register */
{
	int i, j;

	i = usedr(getsn());
	if (i == -1)
		return;
	if (numtabp[i].fmt > 20)	/* it was probably a, A, i or I */
		*pbp++ = numtabp[i].fmt;
	else
		for (j = (numtabp[i].fmt ? numtabp[i].fmt : 1); j; j--)
			*pbp++ = '0';
}


int vnumb(int *i)
{
	vflag++;
	dfact = lss;
	res = VERT;
	return(inumb(i));
}


int hnumb(int *i)
{
	dfact = EM;
	res = HOR;
	return(inumb(i));
}


int inumb(int *n)
{
	int i, j, f;
	Tchar ii;

	f = 0;
	if (n) {
		if ((j = cbits(ii = getch())) == '+')
			f = 1;
		else if (j == '-')
			f = -1;
		else
			ch = ii;
	}
	i = atoi0();
	if (n && f)
		i = *n + f * i;
	i = quant(i, res);
	vflag = 0;
	res = dfactd = dfact = 1;
	if (nonumb)
		i = 0;
	return(i);
}


int quant(int n, int m)
{
	int i, neg;

	neg = 0;
	if (n < 0) {
		neg++;
		n = -n;
	}
	/* better as i = ((n + m/2)/m)*m */
	i = n / m;
	if (n - m * i > m / 2)
		i += 1;
	i *= m;
	if (neg)
		i = -i;
	return(i);
}
