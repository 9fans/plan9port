# include "e.h"

void fromto(int p1, int p2, int p3)
{
	double b, h1, b1, t;
	int subps;

	yyval = salloc();
	lfont[yyval] = rfont[yyval] = 0;
	h1 = eht[yyval] = eht[p1];
	b1 = ebase[p1];
	b = 0;
	subps = ps;
	ps += deltaps;
	nrwid(p1, ps, p1);
	printf(".nr %d \\n(%d\n", (int)yyval, p1);
	if (p2 > 0) {
		nrwid(p2, subps, p2);
		printf(".if \\n(%d>\\n(%d .nr %d \\n(%d\n", p2, (int)yyval, (int)yyval, p2);
		eht[yyval] += eht[p2];
		b = eht[p2];
	}
	if (p3 > 0) {
		nrwid(p3, subps, p3);
		printf(".if \\n(%d>\\n(%d .nr %d \\n(%d\n", p3, (int)yyval, (int)yyval, p3);
		eht[yyval] += eht[p3];
	}
	printf(".ds %d ", (int)yyval);	/* bottom of middle box */
	if (p2 > 0) {
		t = eht[p2]-ebase[p2]+b1;
		printf("\\v'%gm'\\h'\\n(%du-\\n(%du/2u'%s\\*(%d%s",
			REL(t,ps), (int)yyval, p2, DPS(ps,subps), p2, DPS(subps,ps));
		printf("\\h'-\\n(%du-\\n(%du/2u'\\v'%gm'\\\n",
			(int)yyval, p2, REL(-t,ps));
	}
	printf("\\h'\\n(%du-\\n(%du/2u'\\*(%d\\h'\\n(%du-\\n(%du/2u'\\\n",
		(int)yyval, p1, p1, (int)yyval, p1);
	if (p3  >0) {
		t = h1-b1+ebase[p3];
		printf("\\v'%gm'\\h'-\\n(%du-\\n(%du/2u'%s\\*(%d%s\\h'\\n(%du-\\n(%du/2u'\\v'%gm'\\\n",
			REL(-t,ps), (int)yyval, p3, DPS(ps,subps), p3, DPS(subps,ps), (int)yyval, p3, REL(t,ps));
	}
	printf("\n");
	ebase[yyval] = b + b1;
	dprintf(".\tS%d <- %d from %d to %d; h=%g b=%g\n",
		(int)yyval, p1, p2, p3, eht[yyval], ebase[yyval]);
	sfree(p1);
	if (p2 > 0)
		sfree(p2);
	if (p3 > 0)
		sfree(p3);
}
