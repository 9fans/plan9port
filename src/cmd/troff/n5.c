/*
 * troff5.c
 *
 * misc processing requests
 */

#include "tdef.h"
#include "fns.h"
#include "ext.h"

int	iflist[NIF];
int	ifx;
int	ifnum = 0;	/* trying numeric expression for .if or .ie condition */

void casead(void)
{
	int i;

	ad = 1;
	/* leave admod alone */
	if (skip())
		return;
	switch (i = cbits(getch())) {
	case 'r':	/* right adj, left ragged */
		admod = 2;
		break;
	case 'l':	/* left adj, right ragged */
		admod = ad = 0;	/* same as casena */
		break;
	case 'c':	/*centered adj*/
		admod = 1;
		break;
	case 'b':
	case 'n':
		admod = 0;
		break;
	case '0':
	case '2':
	case '4':
		ad = 0;
	case '1':
	case '3':
	case '5':
		admod = (i - '0') / 2;
	}
}


void casena(void)
{
	ad = 0;
}


void casefi(void)
{
	tbreak();
	fi = 1;
	pendnf = 0;
}


void casenf(void)
{
	tbreak();
	fi = 0;
}


void casers(void)
{
	dip->nls = 0;
}


void casens(void)
{
	dip->nls++;
}

int
chget(int c)
{
	Tchar i;

	i = 0;
	if (skip() || ismot(i = getch()) || cbits(i) == ' ' || cbits(i) == '\n') {
		ch = i;
		return(c);
	} else
		return cbits(i);	/* was (i & BYTEMASK) */
}


void casecc(void)
{
	cc = chget('.');
}


void casec2(void)
{
	c2 = chget('\'');
}


void casehc(void)
{
	ohc = chget(OHC);
}


void casetc(void)
{
	tabc = chget(0);
}


void caselc(void)
{
	dotc = chget(0);
}


void casehy(void)
{
	int i;

	hyf = 1;
	if (skip())
		return;
	noscale++;
	i = atoi0();
	noscale = 0;
	if (nonumb)
		return;
	hyf = max(i, 0);
}


void casenh(void)
{
	hyf = 0;
}

int
max(int aa, int bb)
{
	if (aa > bb)
		return(aa);
	else
		return(bb);
}


void casece(void)
{
	int i;

	noscale++;
	skip();
	i = max(atoi0(), 0);
	if (nonumb)
		i = 1;
	tbreak();
	ce = i;
	noscale = 0;
}


void casein(void)
{
	int i;

	if (skip())
		i = in1;
	else {
		i = max(hnumb(&in), 0);
		if (nonumb)
			i = in1;
	}
	tbreak();
	in1 = in;
	in = i;
	if (!nc) {
		un = in;
		setnel();
	}
}


void casell(void)
{
	int i;

	if (skip())
		i = ll1;
	else {
		i = max(hnumb(&ll), INCH / 10);
		if (nonumb)
			i = ll1;
	}
	ll1 = ll;
	ll = i;
	setnel();
}


void caselt(void)
{
	int i;

	if (skip())
		i = lt1;
	else {
		i = max(hnumb(&lt), 0);
		if (nonumb)
			i = lt1;
	}
	lt1 = lt;
	lt = i;
}


void caseti(void)
{
	int i;

	if (skip())
		return;
	i = max(hnumb(&in), 0);
	tbreak();
	un1 = i;
	setnel();
}


void casels(void)
{
	int i;

	noscale++;
	if (skip())
		i = ls1;
	else {
		i = max(inumb(&ls), 1);
		if (nonumb)
			i = ls1;
	}
	ls1 = ls;
	ls = i;
	noscale = 0;
}


void casepo(void)
{
	int i;

	if (skip())
		i = po1;
	else {
		i = max(hnumb(&po), 0);
		if (nonumb)
			i = po1;
	}
	po1 = po;
	po = i;
	if (TROFF & !ascii)
		esc += po - po1;
}


void casepl(void)
{
	int i;

	skip();
	if ((i = vnumb(&pl)) == 0)
		pl = 11 * INCH; /*11in*/
	else
		pl = i;
	if (numtabp[NL].val > pl)
		numtabp[NL].val = pl;
}


void casewh(void)
{
	int i, j, k;

	lgf++;
	skip();
	i = vnumb((int *)0);
	if (nonumb)
		return;
	skip();
	j = getrq();
	if ((k = findn(i)) != NTRAP) {
		mlist[k] = j;
		return;
	}
	for (k = 0; k < NTRAP; k++)
		if (mlist[k] == 0)
			break;
	if (k == NTRAP) {
		flusho();
		ERROR "cannot plant trap." WARN;
		return;
	}
	mlist[k] = j;
	nlist[k] = i;
}


void casech(void)
{
	int i, j, k;

	lgf++;
	skip();
	if (!(j = getrq()))
		return;
	else
		for (k = 0; k < NTRAP; k++)
			if (mlist[k] == j)
				break;
	if (k == NTRAP)
		return;
	skip();
	i = vnumb((int *)0);
	if (nonumb)
		mlist[k] = 0;
	nlist[k] = i;
}

int
findn(int i)
{
	int k;

	for (k = 0; k < NTRAP; k++)
		if ((nlist[k] == i) && (mlist[k] != 0))
			break;
	return(k);
}


void casepn(void)
{
	int i;

	skip();
	noscale++;
	i = max(inumb(&numtabp[PN].val), 0);
	noscale = 0;
	if (!nonumb) {
		npn = i;
		npnflg++;
	}
}


void casebp(void)
{
	int i;
	Stack *savframe;

	if (dip != d)
		return;
	savframe = frame;
	skip();
	if ((i = inumb(&numtabp[PN].val)) < 0)
		i = 0;
	tbreak();
	if (!nonumb) {
		npn = i;
		npnflg++;
	} else if (dip->nls)
		return;
	eject(savframe);
}

void casetm(void)
{
	casetm1(0, stderr);
}


void casefm(void)
{
	static struct fcache {
		char *name;
		FILE *fp;
	} fcache[15];
	int i;

	if ( skip() || !getname()) {
		ERROR "fm: missing filename" WARN;
		return;
	}

	for (i = 0; i < 15 && fcache[i].fp != NULL; i++) {
		if (strcmp(nextf, fcache[i].name) == 0)
			break;
	}
	if (i >= 15) {
		ERROR "fm: too many streams" WARN;
		return;
	}
	if (fcache[i].fp == NULL) {
		if( (fcache[i].fp = fopen(unsharp(nextf), "w")) == NULL) {
			ERROR "fm: cannot open %s", nextf WARN;
			return;
		}
		fcache[i].name = strdupl(nextf);
	}
	casetm1(0, fcache[i].fp);
}

void casetm1(int ab, FILE *out)
{
	int i, j, c;
	char *p;
	char tmbuf[NTM];

	lgf++;
	copyf++;
	if (ab) {
		if (skip())
			ERROR "User Abort" WARN;
		else {
			extern int error;
			int savtrac = trace;
			i = trace = 0;
			noscale++;
			i = inumb(&trace);
			noscale--;
			if (i) {
				error = i;
				if (nlflg || skip())
					ERROR "User Abort, exit code %d", i WARN;
			}
			trace = savtrac;
		}
	} else
		skip();
	for (i = 0; i < NTM - 2; ) {
		if ((c = cbits(getch())) == '\n' || c == RIGHT)
			break;
		else if (c == MINUS) {	/* special pleading for strange encodings */
			tmbuf[i++] = '\\';
			tmbuf[i++] = '-';
		} else if (c == PRESC) {
			tmbuf[i++] = '\\';
			tmbuf[i++] = 'e';
		} else if (c == FILLER) {
			tmbuf[i++] = '\\';
			tmbuf[i++] = '&';
		} else if (c == UNPAD) {
			tmbuf[i++] = '\\';
			tmbuf[i++] = ' ';
		} else if (c == OHC) {
			tmbuf[i++] = '\\';
			tmbuf[i++] = '%';
		} else if (c >= ALPHABET) {
			p = chname(c);
			switch (*p) {
			case MBchar:
				strcpy(&tmbuf[i], p+1);
				break;
			case Number:
				sprintf(&tmbuf[i], "\\N'%s'", p+1);
				break;
			case Troffchar:
				if ((j = strlen(p+1)) == 2)
					sprintf(&tmbuf[i], "\\(%s", p+1);
				else
					sprintf(&tmbuf[i], "\\C'%s'", p+1);
				break;
			default:
				sprintf(&tmbuf[i]," %s? ", p);
				break;
			}
			j = strlen(&tmbuf[i]);
			i += j;
		} else
			tmbuf[i++] = c;
	}
	tmbuf[i] = 0;
	if (ab)	/* truncate output */
		obufp = obuf;	/* should be a function in n2.c */
	flusho();
	if (i)
		fprintf(out, "%s\n", tmbuf);
	fflush(out);
	copyf--;
	lgf--;
}


void casesp(void)
{
	casesp1(0);
}

void casesp1(int a)
{
	int i, j, savlss;

	tbreak();
	if (dip->nls || trap)
		return;
	i = findt1();
	if (!a) {
		skip();
		j = vnumb((int *)0);
		if (nonumb)
			j = lss;
	} else
		j = a;
	if (j == 0)
		return;
	if (i < j)
		j = i;
	savlss = lss;
	if (dip != d)
		i = dip->dnl;
	else
		i = numtabp[NL].val;
	if ((i + j) < 0)
		j = -i;
	lss = j;
	newline(0);
	lss = savlss;
}


void casert(void)
{
	int a, *p;

	skip();
	if (dip != d)
		p = &dip->dnl;
	else
		p = &numtabp[NL].val;
	a = vnumb(p);
	if (nonumb)
		a = dip->mkline;
	if ((a < 0) || (a >= *p))
		return;
	nb++;
	casesp1(a - *p);
}


void caseem(void)
{
	lgf++;
	skip();
	em = getrq();
}


void casefl(void)
{
	tbreak();
	if (!ascii)
		ptflush();
	flusho();
}


void caseev(void)
{
	int nxev;

	if (skip()) {
e0:
		if (evi == 0)
			return;
		nxev =  evlist[--evi];
		goto e1;
	}
	noscale++;
	nxev = atoi0();
	noscale = 0;
	if (nonumb)
		goto e0;
	flushi();
	if (nxev >= NEV || nxev < 0 || evi >= EVLSZ) {
		flusho();
		ERROR "cannot do .ev %d", nxev WARN;
		if (error)
			done2(040);
		else
			edone(040);
		return;
	}
	evlist[evi++] = ev;
e1:
	if (ev == nxev)
		return;
	ev = nxev;
	envp = &env[ev];
}

void envcopy(Env *e1, Env *e2)	/* copy env e2 to e1 */
{
	*e1 = *e2;	/* rumor hath that this fails on some machines */
}


void caseel(void)
{
	if (--ifx < 0) {
		ifx = 0;
		iflist[0] = 0;
	}
	caseif1(2);
}


void caseie(void)
{
	if (ifx >= NIF) {
		ERROR "if-else overflow." WARN;
		ifx = 0;
		edone(040);
	}
	caseif1(1);
	ifx++;
}


void caseif(void)
{
	caseif1(0);
}

void caseif1(int x)
{
	extern int falsef;
	int notflag, true;
	Tchar i;

	if (x == 2) {
		notflag = 0;
		true = iflist[ifx];
		goto i1;
	}
	true = 0;
	skip();
	if ((cbits(i = getch())) == '!') {
		notflag = 1;
	} else {
		notflag = 0;
		ch = i;
	}
	ifnum++;
	i = atoi0();
	ifnum = 0;
	if (!nonumb) {
		if (i > 0)
			true++;
		goto i1;
	}
	i = getch();
	switch (cbits(i)) {
	case 'e':
		if (!(numtabp[PN].val & 01))
			true++;
		break;
	case 'o':
		if (numtabp[PN].val & 01)
			true++;
		break;
	case 'n':
		if (NROFF)
			true++;
		break;
	case 't':
		if (TROFF)
			true++;
		break;
	case ' ':
		break;
	default:
		true = cmpstr(i);
	}
i1:
	true ^= notflag;
	if (x == 1)
		iflist[ifx] = !true;
	if (true) {
i2:
		while ((cbits(i = getch())) == ' ')
			;
		if (cbits(i) == LEFT)
			goto i2;
		ch = i;
		nflush++;
	} else {
		if (!nlflg) {
			copyf++;
			falsef++;
			eatblk(0);
			copyf--;
			falsef--;
		}
	}
}

void eatblk(int inblk)
{
	int cnt, i;

	cnt = 0;
	do {
		if (ch)	{
			i = cbits(ch);
			ch = 0;
		} else
			i = cbits(getch0());
		if (i == ESC)
			cnt++;
		else {
			if (cnt == 1)
				switch (i) {
				case '{':  i = LEFT; break;
				case '}':  i = RIGHT; break;
				case '\n': i = 'x'; break;
				}
			cnt = 0;
		}
		if (i == LEFT) eatblk(1);
	} while ((!inblk && (i != '\n')) || (inblk && (i != RIGHT)));
	if (i == '\n') {
		nlflg++;
		if (ip == 0)
			numtabp[CD].val++;
	}
}

int
cmpstr(Tchar c)
{
	int j, delim;
	Tchar i;
	int val;
	int savapts, savapts1, savfont, savfont1, savpts, savpts1;
	Tchar string[1280];
	Tchar *sp;

	if (ismot(c))
		return(0);
	delim = cbits(c);
	savapts = apts;
	savapts1 = apts1;
	savfont = font;
	savfont1 = font1;
	savpts = pts;
	savpts1 = pts1;
	sp = string;
	while ((j = cbits(i = getch()))!=delim && j!='\n' && sp<&string[1280-1])
		*sp++ = i;
	if (sp >= string + 1280) {
		ERROR "too-long string compare." WARN;
		edone(0100);
	}
	if (nlflg) {
		val = sp==string;
		goto rtn;
	}
	*sp = 0;
	apts = savapts;
	apts1 = savapts1;
	font = savfont;
	font1 = savfont1;
	pts = savpts;
	pts1 = savpts1;
	mchbits();
	val = 1;
	sp = string;
	while ((j = cbits(i = getch())) != delim && j != '\n') {
		if (*sp != i) {
			eat(delim);
			val = 0;
			goto rtn;
		}
		sp++;
	}
	if (*sp)
		val = 0;
rtn:
	apts = savapts;
	apts1 = savapts1;
	font = savfont;
	font1 = savfont1;
	pts = savpts;
	pts1 = savpts1;
	mchbits();
	return(val);
}


void caserd(void)
{

	lgf++;
	skip();
	getname();
	if (!iflg) {
		if (quiet) {
			if (NROFF) {
				echo_off();
				flusho();
			}
			fprintf(stderr, "\007"); /*bell*/
		} else {
			if (nextf[0]) {
				fprintf(stderr, "%s:", nextf);
			} else {
				fprintf(stderr, "\007"); /*bell*/
			}
		}
	}
	collect();
	tty++;
	pushi(RD_OFFSET, PAIR('r','d'));
}

int
rdtty(void)
{
	char	onechar;

	onechar = 0;
	if (read(0, &onechar, 1) == 1) {
		if (onechar == '\n')
			tty++;
		else
			tty = 1;
		if (tty != 3)
			return(onechar);
	}
	tty = 0;
	if (NROFF && quiet)
		echo_on();
	return(0);
}


void caseec(void)
{
	eschar = chget('\\');
}


void caseeo(void)
{
	eschar = 0;
}


void caseta(void)
{
	int i, j, k;

	tabtab[0] = nonumb = 0;
	for (i = 0; ((i < (NTAB - 1)) && !nonumb); i++) {
		if (skip())
			break;
		k = tabtab[max(i-1, 0)] & TABMASK;
		if ((j = max(hnumb(&k), 0)) > TABMASK) {
			ERROR "Tab too far away" WARN;
			j = TABMASK;
		}
		tabtab[i] = j & TABMASK;
		if (!nonumb)
			switch (cbits(ch)) {
			case 'C':
				tabtab[i] |= CTAB;
				break;
			case 'R':
				tabtab[i] |= RTAB;
				break;
			default: /*includes L*/
				break;
			}
		nonumb = ch = 0;
	}
	if (!skip())
		ERROR "Too many tab stops" WARN;
	tabtab[i] = 0;
}


void casene(void)
{
	int i, j;

	skip();
	i = vnumb((int *)0);
	if (nonumb)
		i = lss;
	if (dip == d && numtabp[NL].val == -1) {
		newline(1);
		return;
	}
	if (i > (j = findt1())) {
		i = lss;
		lss = j;
		dip->nls = 0;
		newline(0);
		lss = i;
	}
}


void casetr(void)
{
	int i, j;
	Tchar k;

	lgf++;
	skip();
	while ((i = cbits(k=getch())) != '\n') {
		if (ismot(k))
			return;
		if (ismot(k = getch()))
			return;
		if ((j = cbits(k)) == '\n')
			j = ' ';
		trtab[i] = j;
	}
}


void casecu(void)
{
	cu++;
	caseul();
}


void caseul(void)
{
	int i;

	noscale++;
	skip();
	i = max(atoi0(), 0);
	if (nonumb)
		i = 1;
	if (ul && (i == 0)) {
		font = sfont;
		ul = cu = 0;
	}
	if (i) {
		if (!ul) {
			sfont = font;
			font = ulfont;
		}
		ul = i;
	}
	noscale = 0;
	mchbits();
}


void caseuf(void)
{
	int i, j;

	if (skip() || !(i = getrq()) || i == 'S' ||  (j = findft(i))  == -1)
		ulfont = ULFONT; /*default underline position*/
	else
		ulfont = j;
	if (NROFF && ulfont == FT)
		ulfont = ULFONT;
}


void caseit(void)
{
	int i;

	lgf++;
	it = itmac = 0;
	noscale++;
	skip();
	i = atoi0();
	skip();
	if (!nonumb && (itmac = getrq()))
		it = i;
	noscale = 0;
}


void casemc(void)
{
	int i;

	if (icf > 1)
		ic = 0;
	icf = 0;
	if (skip())
		return;
	ic = getch();
	icf = 1;
	skip();
	i = max(hnumb((int *)0), 0);
	if (!nonumb)
		ics = i;
}


void casemk(void)
{
	int i, j;

	if (dip != d)
		j = dip->dnl;
	else
		j = numtabp[NL].val;
	if (skip()) {
		dip->mkline = j;
		return;
	}
	if ((i = getrq()) == 0)
		return;
	numtabp[findr(i)].val = j;
}


void casesv(void)
{
	int i;

	skip();
	if ((i = vnumb((int *)0)) < 0)
		return;
	if (nonumb)
		i = 1;
	sv += i;
	caseos();
}


void caseos(void)
{
	int savlss;

	if (sv <= findt1()) {
		savlss = lss;
		lss = sv;
		newline(0);
		lss = savlss;
		sv = 0;
	}
}


void casenm(void)
{
	int i;

	lnmod = nn = 0;
	if (skip())
		return;
	lnmod++;
	noscale++;
	i = inumb(&numtabp[LN].val);
	if (!nonumb)
		numtabp[LN].val = max(i, 0);
	getnm(&ndf, 1);
	getnm(&nms, 0);
	getnm(&ni, 0);
	getnm(&nmwid, 3);	/* really kludgy! */
	noscale = 0;
	nmbits = chbits;
}

/*
 * .nm relies on the fact that illegal args are skipped; don't warn
 * for illegality of these
 */
void getnm(int *p, int min)
{
	int i;
	int savtr = trace;

	eat(' ');
	if (skip())
		return;
	trace = 0;
	i = atoi0();
	if (nonumb)
		return;
	*p = max(i, min);
	trace = savtr;
}


void casenn(void)
{
	noscale++;
	skip();
	nn = max(atoi0(), 1);
	noscale = 0;
}


void caseab(void)
{
	casetm1(1, stderr);
	done3(0);
}


/* nroff terminal handling has been pretty well excised */
/* as part of the merge with troff.  these are ghostly remnants, */
/* called, but doing nothing. restore them at your peril. */


void save_tty(void)			/*save any tty settings that may be changed*/
{
}


void restore_tty(void)			/*restore tty settings from beginning*/
{
}


void set_tty(void)
{
}


void echo_off(void)			/*turn off ECHO for .rd in "-q" mode*/
{
}


void echo_on(void)			/*restore ECHO after .rd in "-q" mode*/
{
}
