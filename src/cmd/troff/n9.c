#include "tdef.h"
#include "ext.h"
#include "fns.h"

/*
 * troff9.c
 *
 * misc functions
 */

Tchar setz(void)
{
	Tchar i;

	if (!ismot(i = getch()))
		i |= ZBIT;
	return(i);
}

void setline(void)
{
	Tchar *i;
	Tchar c;
	int length;
	int j, w, cnt, delim, rem, temp;
	Tchar linebuf[NC];

	if (ismot(c = getch()))
		return;
	delim = cbits(c);
	vflag = 0;
	dfact = EM;
	length = quant(atoi0(), HOR);
	dfact = 1;
	if (!length) {
		eat(delim);
		return;
	}
s0:
	if ((j = cbits(c = getch())) == delim || j == '\n') {
		ch = c;
		c = RULE | chbits;
	} else if (cbits(c) == FILLER)
		goto s0;
	w = width(c);
	if (w <= 0) {
		ERROR "zero-width underline character ignored" WARN;
		c = RULE | chbits;
		w = width(c);
	}
	i = linebuf;
	if (length < 0) {
		*i++ = makem(length);
		length = -length;
	}
	if (!(cnt = length / w)) {
		*i++ = makem(-(temp = ((w - length) / 2)));
		*i++ = c;
		*i++ = makem(-(w - length - temp));
		goto s1;
	}
	if (rem = length % w) {
		if (cbits(c) == RULE || cbits(c) == UNDERLINE || cbits(c) == ROOTEN)
			*i++ = c | ZBIT;
		*i++ = makem(rem);
	}
	if (cnt) {
		*i++ = RPT;
		*i++ = cnt;
		*i++ = c;
	}
s1:
	*i = 0;
	eat(delim);
	pushback(linebuf);
}

int
eat(int c)
{
	int i;

	while ((i = cbits(getch())) != c && i != '\n')
		;
	return(i);
}


void setov(void)
{
	int j, k;
	Tchar i, o[NOV+1];
	int delim, w[NOV+1];

	if (ismot(i = getch()))
		return;
	delim = cbits(i);
	for (k = 0; k < NOV && (j = cbits(i = getch())) != delim && j != '\n'; k++) {
		o[k] = i;
		w[k] = width(i);
	}
	o[k] = w[k] = 0;
	if (o[0])
		for (j = 1; j; ) {
			j = 0;
			for (k = 1; o[k] ; k++) {
				if (w[k-1] < w[k]) {
					j++;
					i = w[k];
					w[k] = w[k-1];
					w[k-1] = i;
					i = o[k];
					o[k] = o[k-1];
					o[k-1] = i;
				}
			}
		}
	else
		return;
	*pbp++ = makem(w[0] / 2);
	for (k = 0; o[k]; k++)
		;
	while (k>0) {
		k--;
		*pbp++ = makem(-((w[k] + w[k+1]) / 2));
		*pbp++ = o[k];
	}
}


void setbra(void)
{
	int k;
	Tchar i, *j, dwn;
	int cnt, delim;
	Tchar brabuf[NC];

	if (ismot(i = getch()))
		return;
	delim = cbits(i);
	j = brabuf + 1;
	cnt = 0;
	if (NROFF)
		dwn = (2 * t.Halfline) | MOT | VMOT;
	else
		dwn = EM | MOT | VMOT;
	while ((k = cbits(i = getch())) != delim && k != '\n' && j <= brabuf + NC - 4) {
		*j++ = i | ZBIT;
		*j++ = dwn;
		cnt++;
	}
	if (--cnt < 0)
		return;
	else if (!cnt) {
		ch = *(j - 2);
		return;
	}
	*j = 0;
	if (NROFF)
		*--j = *brabuf = (cnt * t.Halfline) | MOT | NMOT | VMOT;
	else
		*--j = *brabuf = (cnt * EM) / 2 | MOT | NMOT | VMOT;
	*--j &= ~ZBIT;
	pushback(brabuf);
}


void setvline(void)
{
	int i;
	Tchar c, rem, ver, neg;
	int cnt, delim, v;
	Tchar vlbuf[NC];
	Tchar *vlp;

	if (ismot(c = getch()))
		return;
	delim = cbits(c);
	dfact = lss;
	vflag++;
	i = quant(atoi0(), VERT);
	dfact = 1;
	if (!i) {
		eat(delim);
		vflag = 0;
		return;
	}
	if ((cbits(c = getch())) == delim) {
		c = BOXRULE | chbits;	/*default box rule*/
	} else
		getch();
	c |= ZBIT;
	neg = 0;
	if (i < 0) {
		i = -i;
		neg = NMOT;
	}
	if (NROFF)
		v = 2 * t.Halfline;
	else {
		v = EM;
		if (v < VERT)		/* ATT EVK hack: Erik van Konijnenburg, */
			v = VERT;	/* hvlpb!evkonij, ATT NSI Hilversum, Holland */
	}

	cnt = i / v;
	rem = makem(i % v) | neg;
	ver = makem(v) | neg;
	vlp = vlbuf;
	if (!neg)
		*vlp++ = ver;
	if (absmot(rem) != 0) {
		*vlp++ = c;
		*vlp++ = rem;
	}
	while (vlp < vlbuf + NC - 3 && cnt--) {
		*vlp++ = c;
		*vlp++ = ver;
	}
	*(vlp - 2) &= ~ZBIT;
	if (!neg)
		vlp--;
	*vlp = 0;
	pushback(vlbuf);
	vflag = 0;
}

#define	NPAIR	(NC/2-6)	/* max pairs in spline, etc. */

void setdraw(void)	/* generate internal cookies for a drawing function */
{
	int i, j, k, dx[NPAIR], dy[NPAIR], delim, type;
	Tchar c, drawbuf[NC];
	int drawch = '.';	/* character to draw with */

	/* input is \D'f dx dy dx dy ... c' (or at least it had better be) */
	/* this does drawing function f with character c and the */
	/* specified dx,dy pairs interpreted as appropriate */
	/* pairs are deltas from last point, except for radii */

	/* l dx dy:	line from here by dx,dy */
	/* c x:		circle of diameter x, left side here */
	/* e x y:	ellipse of diameters x,y, left side here */
	/* a dx1 dy1 dx2 dy2:
			ccw arc: ctr at dx1,dy1, then end at dx2,dy2 from there */
	/* ~ dx1 dy1 dx2 dy2...:
			spline to dx1,dy1 to dx2,dy2 ... */
	/* b x c:
			built-up character of type c, ht x */
	/* f dx dy ...:	f is any other char:  like spline */

	if (ismot(c = getch()))
		return;
	delim = cbits(c);
	numerr.escarg = type = cbits(getch());
	if (type == '~')	/* head off the .tr ~ problem */
		type = 's';
	for (i = 0; i < NPAIR ; i++) {
		skip();
		vflag = 0;
		dfact = EM;
		dx[i] = quant(atoi0(), HOR);
		if (dx[i] > MAXMOT)
			dx[i] = MAXMOT;
		else if (dx[i] < -MAXMOT)
			dx[i] = -MAXMOT;
		skip();
		if (type == 'c') {
			dy[i] = 0;
			goto eat;
		}
		vflag = 1;
		dfact = lss;
		dy[i] = quant(atoi0(), VERT);
		if (dy[i] > MAXMOT)
			dy[i] = MAXMOT;
		else if (dy[i] < -MAXMOT)
			dy[i] = -MAXMOT;
eat:
		if (cbits(c = getch()) != ' ') {	/* must be the end */
			if (cbits(c) != delim) {
				drawch = cbits(c);
				getch();
			}
			i++;
			break;
		}
	}
	dfact = 1;
	vflag = 0;
	if (TROFF) {
		drawbuf[0] = DRAWFCN | chbits | ZBIT;
		drawbuf[1] = type | chbits | ZBIT;
		drawbuf[2] = drawch | chbits | ZBIT;
		for (k = 0, j = 3; k < i; k++) {
			drawbuf[j++] = MOT | ((dx[k] >= 0) ? dx[k] : (NMOT | -dx[k]));
			drawbuf[j++] = MOT | VMOT | ((dy[k] >= 0) ? dy[k] : (NMOT | -dy[k]));
		}
		if (type == DRAWELLIPSE) {
			drawbuf[5] = drawbuf[4] | NMOT;	/* so the net vertical is zero */
			j = 6;
		} else if (type == DRAWBUILD) {
			drawbuf[4] = drawbuf[3] | NMOT;	/* net horizontal motion is zero */
			drawbuf[2] &= ~ZBIT;		/* width taken from drawing char */
			j = 5;
		}
		drawbuf[j++] = DRAWFCN | chbits | ZBIT;	/* marks end for ptout */
		drawbuf[j] = 0;
		pushback(drawbuf);
	}
}


void casefc(void)
{
	int i;
	Tchar j;

	gchtab[fc] &= ~FCBIT;
	fc = IMP;
	padc = ' ';
	if (skip() || ismot(j = getch()) || (i = cbits(j)) == '\n')
		return;
	fc = i;
	gchtab[fc] |= FCBIT;
	if (skip() || ismot(ch) || (ch = cbits(ch)) == fc)
		return;
	padc = ch;
}


Tchar setfield(int x)
{
	Tchar ii, jj, *fp;
	int i, j;
	int length, ws, npad, temp, type;
	Tchar **pp, *padptr[NPP];
	Tchar fbuf[FBUFSZ];
	int savfc, savtc, savlc;
	Tchar rchar;
	int savepos;
	static Tchar wbuf[] = { WORDSP, 0};

	rchar = 0;
	if (x == tabch)
		rchar = tabc | chbits;
	else if (x ==  ldrch)
		rchar = dotc | chbits;
	temp = npad = ws = 0;
	savfc = fc;
	savtc = tabch;
	savlc = ldrch;
	tabch = ldrch = fc = IMP;
	savepos = numtabp[HP].val;
	gchtab[tabch] &= ~TABBIT;
	gchtab[ldrch] &= ~LDRBIT;
	gchtab[fc] &= ~FCBIT;
	gchtab[IMP] |= TABBIT|LDRBIT|FCBIT;
	for (j = 0; ; j++) {
		if ((tabtab[j] & TABMASK) == 0) {
			if (x == savfc)
				ERROR "zero field width." WARN;
			jj = 0;
			goto rtn;
		}
		if ((length = ((tabtab[j] & TABMASK) - numtabp[HP].val)) > 0 )
			break;
	}
	type = tabtab[j] & ~TABMASK;
	fp = fbuf;
	pp = padptr;
	if (x == savfc) {
		while (1) {
			j = cbits(ii = getch());
			jj = width(ii);
			widthp = jj;
			numtabp[HP].val += jj;
			if (j == padc) {
				npad++;
				*pp++ = fp;
				if (pp > padptr + NPP - 1)
					break;
				goto s1;
			} else if (j == savfc)
				break;
			else if (j == '\n') {
				temp = j;
				if (nlflg && ip == 0) {
					numtabp[CD].val--;
					nlflg = 0;
				}
				break;
			}
			ws += jj;
s1:
			*fp++ = ii;
			if (fp > fbuf + FBUFSZ - 3)
				break;
		}
		if (ws)
			*fp++ = WORDSP;
		if (!npad) {
			npad++;
			*pp++ = fp;
			*fp++ = 0;
		}
		*fp++ = temp;
		*fp = 0;
		temp = i = (j = length - ws) / npad;
		i = (i / HOR) * HOR;
		if ((j -= i * npad) < 0)
			j = -j;
		ii = makem(i);
		if (temp < 0)
			ii |= NMOT;
		for (; npad > 0; npad--) {
			*(*--pp) = ii;
			if (j) {
				j -= HOR;
				(*(*pp)) += HOR;
			}
		}
		pushback(fbuf);
		jj = 0;
	} else if (type == 0) {
		/*plain tab or leader*/
		if ((j = width(rchar)) > 0) {
			int nchar = length / j;
			while (nchar-->0 && pbp < &pbbuf[NC-3]) {
				numtabp[HP].val += j;
				widthp = j;
				*pbp++ = rchar;
			}
			length %= j;
		}
		if (length)
			jj = length | MOT;
		else
			jj = getch0();
		if (savepos > 0)
			pushback(wbuf);
	} else {
		/*center tab*/
		/*right tab*/
		while ((j = cbits(ii = getch())) != savtc && j != '\n' && j != savlc) {
			jj = width(ii);
			ws += jj;
			numtabp[HP].val += jj;
			widthp = jj;
			*fp++ = ii;
			if (fp > fbuf + FBUFSZ - 3)
				break;
		}
		*fp++ = ii;
		*fp = 0;
		if (type == RTAB)
			length -= ws;
		else
			length -= ws / 2; /*CTAB*/
		pushback(fbuf);
		if ((j = width(rchar)) != 0 && length > 0) {
			int nchar = length / j;
			while (nchar-- > 0 && pbp < &pbbuf[NC-3])
				*pbp++ = rchar;
			length %= j;
		}
		if (savepos > 0)
			pushback(wbuf);
		length = (length / HOR) * HOR;
		jj = makem(length);
		if (nlflg) {
			if (ip == 0)
				numtabp[CD].val--;
			nlflg = 0;
		}
	}
rtn:
	gchtab[fc] &= ~FCBIT;
	gchtab[tabch] &= ~TABBIT;
	gchtab[ldrch] &= ~LDRBIT;
	fc = savfc;
	tabch = savtc;
	ldrch = savlc;
	gchtab[fc] |= FCBIT;
	gchtab[tabch] = TABBIT;
	gchtab[ldrch] |= LDRBIT;
	numtabp[HP].val = savepos;
	return(jj);
}
