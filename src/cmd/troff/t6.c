/*
 * t6.c
 *
 * width functions, sizes and fonts
 */

#include "tdef.h"
#include "fns.h"
#include "ext.h"

int	fontlab[MAXFONTS+1];
int	cstab[MAXFONTS+1];
int	ccstab[MAXFONTS+1];
int	bdtab[MAXFONTS+1];
int	sbold = 0;

int
t_width(Tchar j)
{
	int i, k;

	if (iszbit(j))
		return 0;
	if (ismot(j)) {
		if (isvmot(j))
			return(0);
		k = absmot(j);
		if (isnmot(j))
			k = -k;
		return(k);
	}
	i = cbits(j);
	if (i < ' ') {
		if (i == '\b')
			return(-widthp);
		if (i == PRESC)
			i = eschar;
		else if (i == HX)
			return(0);
	}
	if (i == ohc)
		return(0);
	i = trtab[i];
	if (i < ' ')
		return(0);
	if (sfbits(j) == oldbits) {
		xfont = pfont;
		xpts = ppts;
	} else
		xbits(j, 0);
	if (i < nchnames + ALPHABET && widcache[i].fontpts == (xfont<<8) + xpts && !setwdf)
		k = widcache[i].width;
	else {
		k = getcw(i);
		if (bd)
			k += (bd - 1) * HOR;
		if (cs)
			k = cs;
	}
	widthp = k;
	return(k);
}

/*
 * clear width cache-- s means just space
 */
void zapwcache(int s)
{
	int i;

	if (s) {
		widcache[' '].fontpts = 0;
		return;
	}
	for (i=0; i<NWIDCACHE; i++)
		widcache[i].fontpts = 0;
}

int
onfont(int n, int f)	/* is char n on font f? */
{
	int i;
	Font *fp = &fonts[f];
	Chwid *cp, *ep;
	char *np;

	if (n < ALPHABET) {
		if (fp->wp[n].num == n)	/* ascii at front */
			return n;
		else
			return -1;
	}
	cp = &fp->wp[ALPHABET];
	ep = &fp->wp[fp->nchars];
	for ( ; cp < ep; cp++)	/* search others */
		if (cp->num == n)
			return cp - &fp->wp[0];
	/* maybe it was a \N... */
	np = chname(n);
	if (*np == Number) {
		i = atoi(np+1);		/* sscanf(np+1, "%d", &i); */
		cp = &fp->wp[0];
		ep = &fp->wp[fp->nchars];
		for ( ; cp < ep; cp++) {	/* search others */
			if (cp->code == i)
				return cp - &fp->wp[0];
		}
		return -2;	/* a \N that doesn't have an entry */
	}
	return -1;	/* vanilla not found */
}

int
getcw(int i)
{
	int k, n, x;
	Font *fp;
	int nocache = 0;
	if (i < ' ')
		return 0;
	bd = 0;
	fp = &fonts[xfont];
	if (i == ' ') {	/* a blank */
		k = (fp->spacewidth * spacesz + 6) / 12;
		/* this nonsense because .ss cmd uses 1/36 em as its units */
		/* 	and default is 12 */
	} else if ((n = onfont(i, xfont)) >= 0) {	/* on this font at n */
		k = fp->wp[n].wid;
		if (setwdf)
			numtabp[CT].val |= fp->wp[n].kern;
	} else if (n == -2) {		/* \N with default width */

		k = fp->defaultwidth;
	} else {			/* not on current font */
		nocache = 1;
		k = fp->defaultwidth;	/* default-size space */
		if (smnt) {
			int ii, jj;
			for (ii=smnt, jj=0; jj < nfonts; jj++, ii=ii % nfonts + 1) {
				if ((n = onfont(i, ii)) >= 0) {
					k = fonts[ii].wp[n].wid;
					if (xfont == sbold)
						bd = bdtab[ii];
					if (setwdf)
						numtabp[CT].val |= fonts[ii].wp[n].kern;
					break;
				}
			}
		}
	}
	if (!bd)
		bd = bdtab[xfont];
	if (cs = cstab[xfont]) {
		nocache = 1;
		if (ccs = ccstab[xfont])
			x = ccs;
		else
			x = xpts;
		cs = (cs * EMPTS(x)) / 36;
	}
	/* was (k & BYTEMASK);  since .wid is unsigned, should never happen */
	if (k < 0)
		ERROR "can't happen: negative width %d in getcw %d\n", k, i WARN;
	k = (k * xpts + (Unitwidth / 2)) / Unitwidth;
	if (nocache|bd)
		widcache[i].fontpts = 0;
	else {
		widcache[i].fontpts = (xfont<<8) + xpts;
		widcache[i].width = k;
	}
	return(k);
	/* Unitwidth is Units/Point, where
	/* Units is the fundamental digitization
	/* of the character set widths, and
	/* Point is the number of goobies in a point
	/* e.g., for cat, Units=36, Point=6, so Unitwidth=36/6=6
	/* In effect, it's the size at which the widths
	/* translate directly into units.
	*/
}

void xbits(Tchar i, int bitf)
{
	int k;

	if(TROFF) {
		xfont = fbits(i);
		k = sbits(i);
		if(k) {
			xpts = pstab[k-1];
			oldbits = sfbits(i);
			pfont = xfont;
			ppts = xpts;
			return;
		}
		switch(bitf) {
		case 0:
			xfont = font;
			xpts = pts;
			break;
		case 1:
			xfont = pfont;
			xpts = ppts;
			break;
		case 2:
			xfont = mfont;
			xpts = mpts;
		}
	}
}


/* these next two functions ought to be the same in troff and nroff, */
/* but the data structures they search are different. */
/* silly historical problem. */


Tchar t_setch(int c)
{
#ifndef UNICODE
	int j;
#endif
	char temp[50];
	char *s;

#ifndef UNICODE
	j = 0;
#endif
	s = temp;
	if (c == '(') {	/* \(xx */
		if ((*s++ = getach()) == 0 || (*s++ = getach()) == 0)
			return(0);
	} else {	/* \C'...' */
		c = getach();
		while ((*s = getach()) != c && *s != 0 && s < temp + sizeof(temp) - 1)
			s++;
	}
	*s = '\0';
#ifdef UNICODE
	return chadd(temp, Troffchar, Install) | chbits; /* add name even if haven't seen it */
#else
	if (NROFF) {
		j = chadd(temp, Troffchar, Lookup);
		if ( j == -1)
			return 0;
		else
			return j | chbits;
	} else
		return chadd(temp, Troffchar, Install) | chbits; /* add name even if haven't seen it */

#endif /*UNICODE*/
}

Tchar t_setabs(void)		/* set absolute char from \N'...' */
{
	int n;
	char temp[10];

	getch();	/* delim */
	n = 0;
	n = inumb(&n);
	getch();	/* delim */
	if (nonumb)
		return 0;
	sprintf(temp, "%d", n);	/* convert into "#n" */
	n = chadd(temp, Number, Install);
	return n | chbits;
}


/*
 * fontlab[] is a cache that contains font information
 * for each font.
 * fontlab[] contains the 1- or 2-character name of the
 * font current associated with that font.
 * fonts 1..nfonts correspond to the mounted fonts;
 * the last of these are the special fonts.
 * If we don't use the (named) font in one of the
 * standard positions, we install the name in the next
 * free slot of fontlab[] and font[].
 * Whenever we need info about the font, we
 * read in the data into the next free slot with getfont.
 * The ptfont() (t10.c) routine will tell
 * the device filter to put the font always at position
 * zero if xfont > nfonts, so no need to change these filters.
 * Yes, this is a bit kludgy.
 *
 * This gives the new specs of findft:
 * 	find the font name i, where i also can be a number.
 * 	Installs the font(name) i when not present
 * 	returns -1 on error
 */

int
t_findft(int i)
{
	int k;
	Uchar *p;

	p = unpair(i);

	if (isdigit(p[0])) {		/* first look for numbers */
		k = p[0] - '0';
		if (p[1] > 0 && isdigit(p[1]))
			k = 10 * k + p[1] - '0';
		if (k > 0 && k <= nfonts && k < smnt)
			return(k);	/* mounted font:  .ft 3 */
		if (fontlab[k] && k <= MAXFONTS) {	/* translate */
			return(k);			/*number to a name */
		} else {
			fprintf(stderr, "troff: no font at position %d\n", k);
			return(-1);	/* wild number */
		}
	}

	/*
	 * Now we look for font names
	 */
	for (k = 1; fontlab[k] != i; k++) {
		if (k > MAXFONTS)
			return(-1);	/* running out of fontlab space */
		if (fontlab[k] == 0) {	/* passed all existing names */
			if (setfp(k, i, (char *) 0, 1) == -1)
				return(-1);
			else {
				fontlab[k] = i;	/* install the name */
				return(k);
			}
		}
	}
	return(k);			/* was one of the existing names */
}


void caseps(void)
{
	int i;

	if (TROFF) {
		if(skip())
			i = apts1;
		else {
			noscale++;
			i = inumb(&apts);	/* this is a disaster for fractional point sizes */
			noscale = 0;
			if(nonumb)
				i = apts1;
		}
		casps1(i);
	}
}


void casps1(int i)
{

/*
 * in olden times, it used to ignore changes to 0 or negative.
 * this is meant to allow the requested size to be anything,
 * in particular so eqn can generate lots of \s-3's and still
 * get back by matching \s+3's.

	if (i <= 0)
		return;
*/
	apts1 = apts;
	apts = i;
	pts1 = pts;
	pts = findps(i);
	mchbits();
}

int
findps(int i)
{
	int j, k;

	for (j=k=0 ; pstab[j] != 0 ; j++)
		if (abs(pstab[j]-i) < abs(pstab[k]-i))
			k = j;

	return(pstab[k]);
}


void t_mchbits(void)
{
	int i, j, k;

	i = pts;
	for (j = 0; i > (k = pstab[j]); j++)
		if (!k) {
			j--;
			break;
		}
	chbits = 0;
	setsbits(chbits, ++j);
	setfbits(chbits, font);
	sps = width(' ' | chbits);
	zapwcache(1);
}

void t_setps(void)
{
	int i, j;

	j = 0;
	i = cbits(getch());
	if (isdigit(i)) {		/* \sd or \sdd */
		i -= '0';
		if (i == 0)		/* \s0 */
			j = apts1;
		else if (i <= 3 && (ch=getch()) && isdigit(j = cbits(ch))) {	/* \sdd */
			j = 10 * i + j - '0';
			ch = 0;
		} else		/* \sd */
			j = i;
	} else if (i == '(') {		/* \s(dd */
		j = cbits(getch()) - '0';
		j = 10 * j + cbits(getch()) - '0';
		if (j == 0)		/* \s(00 */
			j = apts1;
	} else if (i == '+' || i == '-') {	/* \s+, \s- */
		j = cbits(getch());
		if (isdigit(j)) {		/* \s+d, \s-d */
			j -= '0';
		} else if (j == '(') {		/* \s+(dd, \s-(dd */
			j = cbits(getch()) - '0';
			j = 10 * j + cbits(getch()) - '0';
		}
		if (i == '-')
			j = -j;
		j += apts;
	}
	casps1(j);
}


Tchar t_setht(void)		/* set character height from \H'...' */
{
	int n;
	Tchar c;

	getch();
	n = inumb(&apts);
	getch();
	if (n == 0 || nonumb)
		n = apts;	/* does this work? */
	c = CHARHT;
	c |= ZBIT;
	setsbits(c, n);
	setfbits(c, pts);	/* sneaky, CHARHT font bits are size bits */
	return(c);
}

Tchar t_setslant(void)		/* set slant from \S'...' */
{
	int n;
	Tchar c;

	getch();
	n = 0;
	n = inumb(&n);
	getch();
	if (nonumb)
		n = 0;
	c = SLANT;
	c |= ZBIT;
	setsfbits(c, n+180);
	return(c);
}


void caseft(void)
{
	if (!TROFF) {
		n_caseft();
		return;
	}
	skip();
	setfont(1);
}


void t_setfont(int a)
{
	int i, j;

	if (a)
		i = getrq();
	else
		i = getsn();
	if (!i || i == 'P') {
		j = font1;
		goto s0;
	}
	if (/* i == 'S' || */ i == '0')	/* an experiment -- why can't we change to it? */
		return;
	if ((j = findft(i)) == -1)
		if ((j = setfp(0, i, (char*) 0, 1)) == -1)	/* try to put it in position 0 */
			return;
s0:
	font1 = font;
	font = j;
	mchbits();
}


void t_setwd(void)
{
	int base, wid;
	Tchar i;
	int delim, emsz, k;
	int savhp, savapts, savapts1, savfont, savfont1, savpts, savpts1;

	base = numtabp[ST].val = numtabp[SB].val = wid = numtabp[CT].val = 0;
	if (ismot(i = getch()))
		return;
	delim = cbits(i);
	savhp = numtabp[HP].val;
	numtabp[HP].val = 0;
	savapts = apts;
	savapts1 = apts1;
	savfont = font;
	savfont1 = font1;
	savpts = pts;
	savpts1 = pts1;
	setwdf++;
	while (cbits(i = getch()) != delim && !nlflg) {
		k = width(i);
		wid += k;
		numtabp[HP].val += k;
		if (!ismot(i)) {
			emsz = (INCH/72) * xpts;
		} else if (isvmot(i)) {
			k = absmot(i);
			if (isnmot(i))
				k = -k;
			base -= k;
			emsz = 0;
		} else
			continue;
		if (base < numtabp[SB].val)
			numtabp[SB].val = base;
		if ((k = base + emsz) > numtabp[ST].val)
			numtabp[ST].val = k;
	}
	setn1(wid, 0, (Tchar) 0);
	numtabp[HP].val = savhp;
	apts = savapts;
	apts1 = savapts1;
	font = savfont;
	font1 = savfont1;
	pts = savpts;
	pts1 = savpts1;
	mchbits();
	setwdf = 0;
}


Tchar t_vmot(void)
{
	dfact = lss;
	vflag++;
	return t_mot();
}


Tchar t_hmot(void)
{
	dfact = EM;
	return t_mot();
}


Tchar t_mot(void)
{
	int j, n;
	Tchar i;

	j = HOR;
	getch(); /*eat delim*/
	if (n = atoi0()) {
		if (vflag)
			j = VERT;
		i = makem(quant(n, j));
	} else
		i = 0;
	getch();
	vflag = 0;
	dfact = 1;
	return(i);
}


Tchar t_sethl(int k)
{
	int j;
	Tchar i;

	j = EM / 2;
	if (k == 'u')
		j = -j;
	else if (k == 'r')
		j = -2 * j;
	vflag++;
	i = makem(j);
	vflag = 0;
	return(i);
}


Tchar t_makem(int i)
{
	Tchar j;

	if (i >= 0)
		j = i;
	else
		j = -i;
	if (Hor > 1 && !vflag)
		j = (j + Hor/2)/Hor * Hor;
	j |= MOT;
	if (i < 0)
		j |= NMOT;
	if (vflag)
		j |= VMOT;
	return(j);
}


Tchar getlg(Tchar i)
{
	Tchar j, k;
	int lf;

	if (!TROFF)
		return i;
	if ((lf = fonts[fbits(i)].ligfont) == 0) /* font lacks ligatures */
		return(i);
	j = getch0();
	if (cbits(j) == 'i' && (lf & LFI))
		j = LIG_FI;
	else if (cbits(j) == 'l' && (lf & LFL))
		j = LIG_FL;
	else if (cbits(j) == 'f' && (lf & LFF)) {
		if ((lf & (LFFI|LFFL)) && lg != 2) {
			k = getch0();
			if (cbits(k)=='i' && (lf&LFFI))
				j = LIG_FFI;
			else if (cbits(k)=='l' && (lf&LFFL))
				j = LIG_FFL;
			else {
				*pbp++ = k;
				j = LIG_FF;
			}
		} else
			j = LIG_FF;
	} else {
		*pbp++ = j;
		j = i;
	}
	return(i & SFMASK | j);
}


void caselg(void)
{

	if(TROFF) {
		skip();
		lg = atoi0();
		if (nonumb)
			lg = 1;
	}
}

void casefp(void)
{
	int i, j;

	if (!TROFF) {
		n_casefp();
		return;
	}
	skip();
	i = cbits(getch());
	if (isdigit(i)) {
		i -= '0';
		j = cbits(getch());
		if (isdigit(j))
			i = 10 * i + j - '0';
	}
	if (i <= 0 || i > nfonts)
		ERROR "fp: bad font position %d", i WARN;
	else if (skip() || !(j = getrq()))
		ERROR "fp: no font name" WARN;
	else if (skip() || !getname())
		setfp(i, j, (char*) 0, 1);
	else		/* 3rd argument = filename */
		setfp(i, j, nextf, 1);
}

char *strdupl(const char *s)	/* make a copy of s */
{
	char *t;

	t = (char *) malloc(strlen(s) + 1);
	if (t == NULL)
		ERROR "out of space in strdupl(%s)", s FATAL;
	strcpy(t, s);
	return t;
}

int
setfp(int pos, int f, char *truename, int print)	/* mount font f at position pos[0...nfonts] */
{
	char pathname[NS], shortname[NS];

	zapwcache(0);
	if (truename)
		strcpy(shortname, truename);
	else
		strcpy(shortname, (char *) unpair(f));
	if (truename && strrchr(truename, '/')) {	/* .fp 1 R dir/file: use verbatim */
		snprintf(pathname, NS, "%s", truename);
		if (fonts[pos].truename)
			free(fonts[pos].truename);
		fonts[pos].truename = strdupl(truename);
	} else if (truename) {			/* synonym: .fp 1 R Avant */
		snprintf(pathname, NS, "%s/dev%s/%s", fontdir, devname, truename);
		truename = 0;	/* so doesn't get repeated by ptfpcmd */
	} else					/* vanilla: .fp 5 XX */
		snprintf(pathname, NS, "%s/dev%s/%s", fontdir, devname, shortname);
	if (truename == 0 && fonts[pos].truename != 0) {
		free(fonts[pos].truename);
		fonts[pos].truename = 0;
	}
	if (getfont(pathname, pos) < 0) {
		ERROR "Can't open font file %s", pathname WARN;
		return -1;
	}
	if (print && !ascii) {
		ptfpcmd(pos, fonts[pos].longname, truename);
		ptfont();
	}
	if (pos == smnt) {
		smnt = 0;
		sbold = 0;
	}
	fontlab[pos] = f;
	if (smnt == 0 && fonts[pos].specfont)
		smnt = pos;
	bdtab[pos] = cstab[pos] = ccstab[pos] = 0;
	return pos;
}

/*
 * .cs request; don't check legality of optional arguments
 */
void casecs(void)
{
	int i, j;

	if (TROFF) {
		int savtr = trace;

		trace = 0;
		noscale++;
		skip();
		if (!(i = getrq()) || (i = findft(i)) < 0)
			goto rtn;
		skip();
		cstab[i] = atoi0();
		skip();
		j = atoi0();
		if(nonumb)
			ccstab[i] = 0;
		else
			ccstab[i] = findps(j);
	rtn:
		zapwcache(0);
		noscale = 0;
		trace = savtr;
	}
}


void casebd(void)
{
	int i, j, k;

	j=0;
	if (!TROFF) {
		n_casebd();
		return;
	}
	zapwcache(0);
	k = 0;
bd0:
	if (skip() || !(i = getrq()) || (j = findft(i)) == -1) {
		if (k)
			goto bd1;
		else
			return;
	}
	if (j == smnt) {
		k = smnt;
		goto bd0;
	}
	if (k) {
		sbold = j;
		j = k;
	}
bd1:
	skip();
	noscale++;
	bdtab[j] = atoi0();
	noscale = 0;
}


void casevs(void)
{
	int i;

	if (!TROFF) {
		n_casevs();
		return;
	}
	skip();
	vflag++;
	dfact = INCH; /* default scaling is points! */
	dfactd = 72;
	res = VERT;
	i = inumb(&lss);
	if (nonumb)
		i = lss1;
	if (i < VERT)
		i = VERT;
	lss1 = lss;
	lss = i;
}


void casess(void)
{
	int i;

	if(TROFF) {
		noscale++;
		skip();
		if(i = atoi0()) {
			spacesz = i & 0177;
			zapwcache(0);
			sps = width(' ' | chbits);
		}
		noscale = 0;
	}
}


Tchar t_xlss(void)
{
	/* stores \x'...' into two successive Tchars.
	/* the first contains HX, the second the value,
	/* encoded as a vertical motion.
	/* decoding is done in n2.c by pchar().
	*/
	int i;

	getch();
	dfact = lss;
	i = quant(atoi0(), VERT);
	dfact = 1;
	getch();
	if (i >= 0)
		*pbp++ = MOT | VMOT | i;
	else
		*pbp++ = MOT | VMOT | NMOT | -i;
	return(HX);
}

Uchar *unpair(int i)
{
	static Uchar name[3];

	name[0] = i & SHORTMASK;
	name[1] = (i >> SHORT) & SHORTMASK;
	name[2] = 0;
	return name;
}
