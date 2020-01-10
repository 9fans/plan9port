#include "e.h"
#include "y.tab.h"

void pile(int oct)
{
	int i, nlist, nlist2, mid;
	double bi, h, b, gap, sb;
	extern double Pilegap, Pilebase;
	int type, p1, p2;

	yyval = salloc();
	type = lp[oct];
	p1 = oct + 3;		/* first entry */
	p2 = p1 + lp[oct+1];	/* 1 after last */
	gap = lp[oct+2];
	if (gap != DEFGAP)
		gap = EM(gap/100.0, ps);
	else if (type == COL)
		gap = 0;
	else
		gap = EM(Pilegap, ps);	/* 0.4 m between LCOL, etc. */
	nlist = p2 - p1;
	nlist2 = (nlist+1)/2;
	mid = p1 + nlist2 - 1;
	h = 0;
	for (i = p1; i < p2; i++)
		h += eht[lp[i]];
	eht[yyval] = h + (nlist-1)*gap;
	b = 0;
	for (i = p2-1; i > mid; i--)
		b += eht[lp[i]] + gap;
	ebase[yyval] = (nlist%2) ? b + ebase[lp[mid]]
			: b - EM(Pilebase, ps) - gap;
	if (dbg) {
		printf(".\tS%d <- %d pile of:", (int)yyval, type);
		for (i = p1; i < p2; i++)
			printf(" S%d", lp[i]);
		printf("; h=%g b=%g\n", eht[yyval], ebase[yyval]);
	}
	nrwid(lp[p1], ps, lp[p1]);
	printf(".nr %d \\n(%d\n", (int)yyval, lp[p1]);
	for (i = p1+1; i < p2; i++) {
		nrwid(lp[i], ps, lp[i]);
		printf(".if \\n(%d>\\n(%d .nr %d \\n(%d\n",
			lp[i], (int)yyval, (int)yyval, lp[i]);
	}
	printf(".ds %d \\v'%gm'\\h'%du*\\n(%du'\\\n", (int)yyval, REL(ebase[yyval],ps),
		type==RCOL ? 1 : 0, (int)yyval);
	sb = 0;		/* sum of box hts */
	for (i = p2-1; i >= p1; i--) {
		bi = sb + ebase[lp[i]];
		switch (type) {
		case LCOL:
			printf("\\v'%gm'\\*(%d\\h'-\\n(%du'\\v'%gm'\\\n",
				REL(-bi,ps), lp[i], lp[i], REL(bi,ps));
			break;
		case RCOL:
			printf("\\v'%gm'\\h'-\\n(%du'\\*(%d\\v'%gm'\\\n",
				REL(-bi,ps), lp[i], lp[i], REL(bi,ps));
			break;
		case CCOL:
		case COL:
			printf("\\v'%gm'\\h'\\n(%du-\\n(%du/2u'\\*(%d",
				REL(-bi,ps), (int)yyval, lp[i], lp[i]);
			printf("\\h'-\\n(%du-\\n(%du/2u'\\v'%gm'\\\n",
				(int)yyval, lp[i], REL(bi,ps));
			break;
		}
		sb += eht[lp[i]] + gap;
	}
	printf("\\v'%gm'\\h'%du*\\n(%du'\n", REL(-ebase[yyval],ps),
		type!=RCOL ? 1 : 0, (int)yyval);
	for (i = p1; i < p2; i++)
		sfree(lp[i]);
	lfont[yyval] = rfont[yyval] = 0;
}
