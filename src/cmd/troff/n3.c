/*
 * troff3.c
 *
 * macro and string routines, storage allocation
 */

#include "tdef.h"
#include "fns.h"
#include "ext.h"

Tchar	*argtop;
int	pagech = '%';
int	strflg;

#define	MHASHSIZE	128	/* must be 2**n */
#define	MHASH(x)	((x>>6)^x) & (MHASHSIZE-1)
Contab	*mhash[MHASHSIZE];


Blockp	*blist;		/* allocated blocks for macros and strings */
int	nblist;		/* how many there are */
int	bfree = -1;	/* first (possible) free block in the list */

Contab *contabp = NULL;
#define MDELTA 500
int	nm = 0;

int savname;		/* name of macro/string being defined */
int savslot;		/* place in Contab of savname */
int freeslot = -1;	/* first (possible) free slot in contab */

void prcontab(Contab *p)
{
	int i;
	for (i = 0; i < nm; i++)
		if (p)
			if (p[i].rq != 0)
				fprintf(stderr, "slot %d, %-2.2s\n", i, unpair(p[i].rq));
			else
				fprintf(stderr, "slot %d empty\n", i);
		else
			fprintf(stderr, "slot %d empty\n", i);
}


void blockinit(void)
{
	blist = (Blockp *) calloc(NBLIST, sizeof(Blockp));
	if (blist == NULL) {
		ERROR "not enough room for %d blocks", NBLIST WARN;
		done2(1);
	}
	nblist = NBLIST;
	blist[0].nextoff = blist[1].nextoff = -1;
	blist[0].bp = (Tchar *) calloc(BLK, sizeof(Tchar));
	blist[1].bp = (Tchar *) calloc(BLK, sizeof(Tchar));
		/* -1 prevents blist[0] from being used; temporary fix */
		/* for a design botch: offset==0 is overloaded. */
		/* blist[1] reserved for .rd indicator -- also unused. */
		/* but someone unwittingly looks at these, so allocate something */
	bfree = 2;
}


char *grow(char *ptr, int num, int size)	/* make array bigger */
{
	char *p;

	if (ptr == NULL)
		p = (char *) calloc(num, size);
	else
		p = (char *) realloc(ptr, num * size);
	return p;
}

void mnspace(void)
{
	nm = sizeof(contab)/sizeof(Contab) + MDELTA;
	freeslot = sizeof(contab)/sizeof(Contab) + 1;
	contabp = (Contab *) grow((char *) contabp, nm, sizeof(Contab));
	if (contabp == NULL) {
		ERROR "not enough memory for namespace of %d marcos", nm WARN;
		exit(1);
	}
	contabp = (Contab *) memcpy((char *) contabp, (char *)contab,
							sizeof(contab));
	if (contabp == NULL) {
		ERROR "Cannot reinitialize macro/request name list" WARN;
		exit(1);
	}

}

void caseig(void)
{
	int i;
	Offset oldoff = offset;

	offset = 0;
	i = copyb();
	offset = oldoff;
	if (i != '.')
		control(i, 1);
}


void casern(void)
{
	int i, j, k;

	lgf++;
	skip();
	if ((i = getrq()) == 0 || (oldmn = findmn(i)) < 0)
		return;
	skip();
	clrmn(findmn(j = getrq()));
	if (j) {
		munhash(&contabp[oldmn]);
		contabp[oldmn].rq = j;
		maddhash(&contabp[oldmn]);
		if (dip != d )
			for (k = dilev; k; k--)
				if (d[k].curd == i)
					d[k].curd = j;
	}
}

void maddhash(Contab *rp)
{
	Contab **hp;

	if (rp->rq == 0)
		return;
	hp = &mhash[MHASH(rp->rq)];
	rp->link = *hp;
	*hp = rp;
}

void munhash(Contab *mp)
{
	Contab *p;
	Contab **lp;

	if (mp->rq == 0)
		return;
	lp = &mhash[MHASH(mp->rq)];
	p = *lp;
	while (p) {
		if (p == mp) {
			*lp = p->link;
			p->link = 0;
			return;
		}
		lp = &p->link;
		p = p->link;
	}
}

void mrehash(void)
{
	Contab *p;
	int i;

	for (i=0; i < MHASHSIZE; i++)
		mhash[i] = 0;
	for (p=contabp; p < &contabp[nm]; p++)
		p->link = 0;
	for (p=contabp; p < &contabp[nm]; p++) {
		if (p->rq == 0)
			continue;
		i = MHASH(p->rq);
		p->link = mhash[i];
		mhash[i] = p;
	}
}

void caserm(void)
{
	int j;
	int k = 0;

	lgf++;
g0:
	while (!skip() && (j = getrq()) != 0) {
		if (dip != d)
			for (k = dilev; k; k--)
				if (d[k].curd == j) {
					ERROR "cannot remove diversion %s during definition",
								unpair(j) WARN;
					goto g0;
				}
		clrmn(findmn(j));
	}
	lgf--;
}


void caseas(void)
{
	app++;
	caseds();
}


void caseds(void)
{
	ds++;
	casede();
}


void caseam(void)
{
	app++;
	casede();
}


void casede(void)
{
	int i, req;
	Offset savoff;

	req = '.';
	lgf++;
	skip();
	if ((i = getrq()) == 0)
		goto de1;
	if ((offset = finds(i)) == 0)
		goto de1;
	if (newmn)
		savslot = newmn;
	else
		savslot = findmn(i);
	savname = i;
	if (ds)
		copys();
	else
		req = copyb();
	clrmn(oldmn);
	if (newmn) {
		if (contabp[newmn].rq)
			munhash(&contabp[newmn]);
		contabp[newmn].rq = i;
		maddhash(&contabp[newmn]);

	}
	if (apptr) {
		savoff = offset;
		offset = apptr;
		wbf((Tchar) IMP);
		offset = savoff;
	}
	offset = dip->op;
	if (req != '.')
		control(req, 1);
de1:
	ds = app = 0;
}


int findmn(int i)
{
	Contab *p;

	for (p = mhash[MHASH(i)]; p; p = p->link)
		if (i == p->rq)
			return(p - contabp);
	return(-1);
}


void clrmn(int i)
{
	if (i >= 0) {
		if (contabp[i].mx)
			ffree(contabp[i].mx);
		munhash(&contabp[i]);
		contabp[i].rq = 0;
		contabp[i].mx = 0;
		contabp[i].emx = 0;
		contabp[i].f = 0;
		if (contabp[i].divsiz != NULL) {
			free(contabp[i].divsiz);
			contabp[i].divsiz = NULL;
		}
		if (freeslot > i)
			freeslot = i;
	}
}

void growcontab(void)
{
	nm += MDELTA;
	contabp = (Contab *) grow((char *) contabp , nm, sizeof(Contab));
	if (contabp == NULL) {
		ERROR "Too many (%d) string/macro names", nm WARN;
		done2(02);
	} else {
		memset((char *)(contabp) + (nm - MDELTA) * sizeof(Contab),
						0, MDELTA * sizeof(Contab));
		mrehash();
	}
}


Offset finds(int mn)
{
	int i;
	Offset savip;

	oldmn = findmn(mn);
	newmn = 0;
	apptr = 0;
	if (app && oldmn >= 0 && contabp[oldmn].mx) {
		savip = ip;
		ip = contabp[oldmn].emx;
		oldmn = -1;
		apptr = ip;
		if (!diflg)
			ip = incoff(ip);
		nextb = ip;
		ip = savip;
	} else {
		for (i = freeslot; i < nm; i++) {
			if (contabp[i].rq == 0)
				break;
		}
		if (i == nm)
			growcontab();
		freeslot = i + 1;
		if ((nextb = alloc()) == -1) {
			app = 0;
			if (macerr++ > 1)
				done2(02);
			if (nextb == 0)
				ERROR "Not enough space for string/macro names" WARN;
			edone(04);
			return(offset = 0);
		}
		contabp[i].mx = nextb;
		if (!diflg) {
			newmn = i;
			if (oldmn == -1)
				contabp[i].rq = -1;
		} else {
			contabp[i].rq = mn;
			maddhash(&contabp[i]);
		}
	}
	app = 0;
	return(offset = nextb);
}

int skip(void)
{
	Tchar i;

	while (cbits(i = getch()) == ' ' || ismot(i))
		;
	ch = i;
	return(nlflg);
}


int copyb(void)
{
	int i, j, state;
	Tchar ii;
	int req, k;
	Offset savoff;
	Uchar *p;

	savoff = 0;
	if (skip() || !(j = getrq()))
		j = '.';
	req = j;
	p = unpair(j);
	/* was: k = j >> BYTE; j &= BYTEMASK; */
	j = p[0];
	k = p[1];
	copyf++;
	flushi();
	nlflg = 0;
	state = 1;

/* state 0	eat up
 * state 1	look for .
 * state 2	look for first char of end macro
 * state 3	look for second char of end macro
 */

	while (1) {
		i = cbits(ii = getch());
		if (state == 3) {
			if (i == k)
				break;
			if (!k) {
				ch = ii;
				i = getach();
				ch = ii;
				if (!i)
					break;
			}
			state = 0;
			goto c0;
		}
		if (i == '\n') {
			state = 1;
			nlflg = 0;
			goto c0;
		}
		if (state == 1 && i == '.') {
			state++;
			savoff = offset;
			goto c0;
		}
		if (state == 2 && i == j) {
			state++;
			goto c0;
		}
		state = 0;
c0:
		if (offset)
			wbf(ii);
	}
	if (offset) {
		offset = savoff;
		wbf((Tchar)0);
	}
	copyf--;
	return(req);
}


void copys(void)
{
	Tchar i;

	copyf++;
	if (skip())
		goto c0;
	if (cbits(i = getch()) != '"')
		wbf(i);
	while (cbits(i = getch()) != '\n')
		wbf(i);
c0:
	wbf((Tchar)0);
	copyf--;
}


Offset alloc(void)	/* return free Offset in nextb */
{
	int i, j;

	for (i = bfree; i < nblist; i++)
		if (blist[i].nextoff == 0)
			break;
	if (i == nblist) {
		blist = (Blockp *) realloc((char *) blist, 2 * nblist * sizeof(Blockp));
		if (blist == NULL) {
			ERROR "can't grow blist for string/macro defns" WARN;
			done2(2);
		}
		nblist *= 2;
		for (j = i; j < nblist; j++) {
			blist[j].nextoff = 0;
			blist[j].bp = 0;
		}
	}
	blist[i].nextoff = -1;	/* this block is the end */
	bfree = i + 1;
	if (blist[i].bp == 0)
		blist[i].bp = (Tchar *) calloc(BLK, sizeof(Tchar));
	if (blist[i].bp == NULL) {
		ERROR "can't allocate memory for string/macro definitions" WARN;
		done2(2);
	}
	nextb = (Offset) i * BLK;
	return nextb;
}


void ffree(Offset i)	/* free list of blocks starting at blist(o) */
{			/* (doesn't actually free the blocks, just the pointers) */
	int j;

	for ( ; blist[j = bindex(i)].nextoff != -1; ) {
		if (bfree > j)
			bfree = j;
		i = blist[j].nextoff;
		blist[j].nextoff = 0;
	}
	blist[j].nextoff = 0;
}


void wbf(Tchar i)	/* store i into offset, get ready for next one */
{
	int j, off;

	if (!offset)
		return;
	j = bindex(offset);
	if (i == 0)
		contabp[savslot].emx = offset;
	off = boffset(offset);
	blist[j].bp[off++] = i;
	offset++;
	if (pastend(offset)) {	/* off the end of this block */
		if (blist[j].nextoff == -1) {
			if ((nextb = alloc()) == -1) {
				ERROR "Out of temp file space" WARN;
				done2(01);
			}
			blist[j].nextoff = nextb;
		}
		offset = blist[j].nextoff;
	}
}


Tchar rbf(void)	/* return next char from blist[] block */
{
	Tchar i, j;

	if (ip == RD_OFFSET) {		/* for rdtty */
		if (j = rdtty())
			return(j);
		else
			return(popi());
	}

	i = rbf0(ip);
	if (i == 0) {
		if (!app)
			i = popi();
		return(i);
	}
	ip = incoff(ip);
	return(i);
}


Offset xxxincoff(Offset p)		/* get next blist[] block */
{
	p++;
	if (pastend(p)) {		/* off the end of this block */
		if ((p = blist[bindex(p-1)].nextoff) == -1) {	/* and nothing was allocated after it */
			ERROR "Bad storage allocation" WARN;
			done2(-5);
		}
	}
	return(p);
}


Tchar popi(void)
{
	Stack *p;

	if (frame == stk)
		return(0);
	if (strflg)
		strflg--;
	p = nxf = frame;
	p->nargs = 0;
	frame = p->pframe;
	ip = p->pip;
	pendt = p->ppendt;
	lastpbp = p->lastpbp;
	return(p->pch);
}

/*
 *	test that the end of the allocation is above a certain location
 *	in memory
 */
#define SPACETEST(base, size) \
	if ((char*)base + size >= (char*)stk+STACKSIZE) \
		ERROR "Stacksize overflow in n3" WARN

Offset pushi(Offset newip, int  mname)
{
	Stack *p;

	SPACETEST(nxf, sizeof(Stack));
	p = nxf;
	p->pframe = frame;
	p->pip = ip;
	p->ppendt = pendt;
	p->pch = ch;
	p->lastpbp = lastpbp;
	p->mname = mname;
	lastpbp = pbp;
	pendt = ch = 0;
	frame = nxf;
	if (nxf->nargs == 0)
		nxf += 1;
	else
		nxf = (Stack *)argtop;
	return(ip = newip);
}


void *setbrk(int x)
{
	char *i;

	if ((i = (char *) calloc(x, 1)) == 0) {
		ERROR "Core limit reached" WARN;
		edone(0100);
	}
	return(i);
}


int getsn(void)
{
	int i;

	if ((i = getach()) == 0)
		return(0);
	if (i == '(')
		return(getrq());
	else
		return(i);
}


Offset setstr(void)
{
	int i, j;

	lgf++;
	if ((i = getsn()) == 0 || (j = findmn(i)) == -1 || !contabp[j].mx) {
		lgf--;
		return(0);
	} else {
		SPACETEST(nxf, sizeof(Stack));
		nxf->nargs = 0;
		strflg++;
		lgf--;
		return pushi(contabp[j].mx, i);
	}
}



void collect(void)
{
	int j;
	Tchar i, *strp, *lim, **argpp, **argppend;
	int quote;
	Stack *savnxf;

	copyf++;
	nxf->nargs = 0;
	savnxf = nxf;
	if (skip())
		goto rtn;

	{
		char *memp;
		memp = (char *)savnxf;
		/*
		 *	1 s structure for the macro descriptor
		 *	APERMAC Tchar *'s for pointers into the strings
		 *	space for the Tchar's themselves
		 */
		memp += sizeof(Stack);
		/*
		 *	CPERMAC = the total # of characters for ALL arguments
		 */
#define	CPERMAC	200
#define	APERMAC	9
		memp += APERMAC * sizeof(Tchar *);
		memp += CPERMAC * sizeof(Tchar);
		nxf = (Stack *)memp;
	}
	lim = (Tchar *)nxf;
	argpp = (Tchar **)(savnxf + 1);
	argppend = &argpp[APERMAC];
	SPACETEST(argppend, sizeof(Tchar *));
	strp = (Tchar *)argppend;
	/*
	 *	Zero out all the string pointers before filling them in.
	 */
	for (j = 0; j < APERMAC; j++)
		argpp[j] = 0;
	/* ERROR "savnxf=0x%x,nxf=0x%x,argpp=0x%x,strp=argppend=0x%x, lim=0x%x",
	 * 	savnxf, nxf, argpp, strp, lim WARN;
	 */
	strflg = 0;
	while (argpp != argppend && !skip()) {
		*argpp++ = strp;
		quote = 0;
		if (cbits(i = getch()) == '"')
			quote++;
		else
			ch = i;
		while (1) {
			i = getch();
/* fprintf(stderr, "collect %c %d\n", cbits(i), cbits(i)); */
			if (nlflg || (!quote && argpp != argppend && cbits(i) == ' '))
				break;	/* collects rest into $9 */
			if (   quote
			    && cbits(i) == '"'
			    && cbits(i = getch()) != '"') {
				ch = i;
				break;
			}
			*strp++ = i;
			if (strflg && strp >= lim) {
				/* ERROR "strp=0x%x, lim = 0x%x", strp, lim WARN; */
				ERROR "Macro argument too long" WARN;
				copyf--;
				edone(004);
			}
			SPACETEST(strp, 3 * sizeof(Tchar));
		}
		*strp++ = 0;
	}
	nxf = savnxf;
	nxf->nargs = argpp - (Tchar **)(savnxf + 1);
	argtop = strp;
rtn:
	copyf--;
}


void seta(void)
{
	int i;

	i = cbits(getch()) - '0';
	if (i > 0 && i <= APERMAC && i <= frame->nargs)
		pushback(*(((Tchar **)(frame + 1)) + i - 1));
}


void caseda(void)
{
	app++;
	casedi();
}

void casegd(void)
{
	int i, j;

	skip();
	if ((i = getrq()) == 0)
		return;
	if ((j = findmn(i)) >= 0) {
		if (contabp[j].divsiz != NULL) {
			numtabp[DN].val = contabp[j].divsiz->dix;
			numtabp[DL].val = contabp[j].divsiz->diy;
		}
	}
}

#define FINDDIV(o) if ((o =  findmn(dip->curd)) < 0) \
			ERROR "lost diversion %s", unpair(dip->curd) WARN

void casedi(void)
{
	int i, j, *k;

	lgf++;
	if (skip() || (i = getrq()) == 0) {
		if (dip != d) {
			FINDDIV(savslot);
			wbf((Tchar)0);
		}
		if (dilev > 0) {
			numtabp[DN].val = dip->dnl;
			numtabp[DL].val = dip->maxl;
			FINDDIV(j);
			if ((contabp[j].divsiz = (Divsiz *) malloc(sizeof(Divsiz))) == NULL) {
				ERROR "Cannot alloc diversion size" WARN;
				done2(1);
			} else {
				contabp[j].divsiz->dix = numtabp[DN].val;
				contabp[j].divsiz->diy = numtabp[DL].val;
			}
			dip = &d[--dilev];
			offset = dip->op;
		}
		goto rtn;
	}
	if (++dilev == NDI) {
		--dilev;
		ERROR "Diversions nested too deep" WARN;
		edone(02);
	}
	if (dip != d) {
		FINDDIV(j);
		savslot = j;
		wbf((Tchar)0);
	}
	diflg++;
	dip = &d[dilev];
	dip->op = finds(i);
	dip->curd = i;
	clrmn(oldmn);
	k = (int *) & dip->dnl;
	for (j = 0; j < 10; j++)
		k[j] = 0;	/*not op and curd*/
rtn:
	app = 0;
	diflg = 0;
}


void casedt(void)
{
	lgf++;
	dip->dimac = dip->ditrap = dip->ditf = 0;
	skip();
	dip->ditrap = vnumb((int *)0);
	if (nonumb)
		return;
	skip();
	dip->dimac = getrq();
}

#define LNSIZE 4000
void casetl(void)
{
	int j;
	int w[3];
	Tchar buf[LNSIZE];
	Tchar *tp;
	Tchar i, delim;

 	/*
 	 * bug fix
 	 *
 	 * if .tl is the first thing in the file, the p1
 	 * doesn't come out, also the pagenumber will be 0
 	 *
 	 * tends too confuse the device filter (and the user as well)
 	 */
 	if (dip == d && numtabp[NL].val == -1)
 		newline(1);
	dip->nls = 0;
	skip();
	if (ismot(delim = getch())) {
		ch = delim;
		delim = '\'';
	} else
		delim = cbits(delim);
	tp = buf;
	numtabp[HP].val = 0;
	w[0] = w[1] = w[2] = 0;
	j = 0;
	while (cbits(i = getch()) != '\n') {
		if (cbits(i) == cbits(delim)) {
			if (j < 3)
				w[j] = numtabp[HP].val;
			numtabp[HP].val = 0;
			if (w[j] != 0)
				*tp++ = WORDSP;
			j++;
			*tp++ = 0;
		} else {
			if (cbits(i) == pagech) {
				setn1(numtabp[PN].val, numtabp[findr('%')].fmt,
				      i&SFMASK);
				continue;
			}
			numtabp[HP].val += width(i);
			if (tp < &buf[LNSIZE-10]) {
				if (cbits(i) == ' ' && *tp != WORDSP)
					*tp++ = WORDSP;
				*tp++ = i;
			} else {
				ERROR "Overflow in casetl" WARN;
			}
		}
	}
	if (j<3)
		w[j] = numtabp[HP].val;
	*tp++ = 0;
	*tp++ = 0;
	*tp = 0;
	tp = buf;
	if (NROFF)
		horiz(po);
	while (i = *tp++)
		pchar(i);
	if (w[1] || w[2])
		horiz(j = quant((lt - w[1]) / 2 - w[0], HOR));
	while (i = *tp++)
		pchar(i);
	if (w[2]) {
		horiz(lt - w[0] - w[1] - w[2] - j);
		while (i = *tp++)
			pchar(i);
	}
	newline(0);
	if (dip != d) {
		if (dip->dnl > dip->hnl)
			dip->hnl = dip->dnl;
	} else {
		if (numtabp[NL].val > dip->hnl)
			dip->hnl = numtabp[NL].val;
	}
}


void casepc(void)
{
	pagech = chget(IMP);
}


void casepm(void)
{
	int i, k;
	int xx, cnt, tcnt, kk, tot;
	Offset j;

	kk = cnt = tcnt = 0;
	tot = !skip();
	stackdump();
	for (i = 0; i < nm; i++) {
		if ((xx = contabp[i].rq) == 0 || contabp[i].mx == 0)
			continue;
		tcnt++;
		j = contabp[i].mx;
		for (k = 1; (j = blist[bindex(j)].nextoff) != -1; )
			k++;
		cnt++;
		kk += k;
		if (!tot)
			fprintf(stderr, "%-2.2s %d\n", unpair(xx), k);
	}
	fprintf(stderr, "pm: total %d, macros %d, space %d\n", tcnt, cnt, kk);
}

void stackdump(void)	/* dumps stack of macros in process */
{
	Stack *p;

	if (frame != stk) {
		fprintf(stderr, "stack: ");
		for (p = frame; p != stk; p = p->pframe)
			fprintf(stderr, "%s ", unpair(p->mname));
		fprintf(stderr, "\n");
	}
}
