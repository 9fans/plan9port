#define	CONTIG	57346
#define	QTEXT	57347
#define	SPACE	57348
#define	THIN	57349
#define	TAB	57350
#define	MATRIX	57351
#define	LCOL	57352
#define	CCOL	57353
#define	RCOL	57354
#define	COL	57355
#define	ABOVE	57356
#define	MARK	57357
#define	LINEUP	57358
#define	SUM	57359
#define	INT	57360
#define	PROD	57361
#define	UNION	57362
#define	INTER	57363
#define	DEFINE	57364
#define	TDEFINE	57365
#define	NDEFINE	57366
#define	DELIM	57367
#define	GSIZE	57368
#define	GFONT	57369
#define	INCLUDE	57370
#define	IFDEF	57371
#define	DOTEQ	57372
#define	DOTEN	57373
#define	FROM	57374
#define	TO	57375
#define	OVER	57376
#define	SQRT	57377
#define	SUP	57378
#define	SUB	57379
#define	SIZE	57380
#define	FONT	57381
#define	ROMAN	57382
#define	ITALIC	57383
#define	BOLD	57384
#define	FAT	57385
#define	UP	57386
#define	DOWN	57387
#define	BACK	57388
#define	FWD	57389
#define	LEFT	57390
#define	RIGHT	57391
#define	DOT	57392
#define	DOTDOT	57393
#define	HAT	57394
#define	TILDE	57395
#define	BAR	57396
#define	LOWBAR	57397
#define	HIGHBAR	57398
#define	UNDER	57399
#define	VEC	57400
#define	DYAD	57401
#define	UTILDE	57402

#line	17	"/usr/local/plan9/src/cmd/eqn/eqn.y"
#include "e.h"

int	yylex(void);
extern	int	yyerrflag;
#ifndef	YYMAXDEPTH
#define	YYMAXDEPTH	150
#endif
#ifndef	YYSTYPE
#define	YYSTYPE	int
#endif
YYSTYPE	yylval;
YYSTYPE	yyval;
#define YYEOFCODE 1
#define YYERRCODE 2

#line	140	"/usr/local/plan9/src/cmd/eqn/eqn.y"

short	yyexca[] =
{-1, 0,
	1, 3,
	-2, 0,
-1, 1,
	1, -1,
	-2, 0,
};
#define	YYNPROD	90
#define	YYPRIVATE 57344
#define	YYLAST	469
short	yyact[] =
{
   4, 103, 119,  45,  27, 118, 104,   2, 102,  41,
  42,  43,  44,  65,  80,  81,  79,  66,  67,  68,
  69,  70,  50,  49,  74,  75,  76,  77, 105,  73,
  40,  80,  81,  80,  81, 114,  61,  64,  54,  62,
  57,  58,  59,  60,  55,  56,  63,  78,  91,  92,
  82,  26,  83,  85,  86,  87,  88,  90,  51,  52,
  48, 124,  50,  49, 117,  25,  45, 117,  72,  71,
  80,  81, 113,  24,  45,  23,  61,  64,  54,  62,
  57,  58,  59,  60,  55,  56,  63,  53,  89, 100,
  84,  22,  96,  95, 106, 107, 108, 109,  99, 110,
 111,  41,  42,  43,  44,  45,  98, 115,  21,  94,
  93,  18, 130, 123,  17, 116, 121,  46, 112, 125,
 127, 128,   1, 129, 126,   0,   0,  45,   8,   7,
   9,  10,  11,  28,  41,  42,  43,  44,   0,  16,
  47,  12,  34,  13,  14,  15,  61,  64,  54,  62,
  57,  58,  59,  60,  55,  56,  63,   0,   0,  20,
   0,   0,  29,  33,  30,  31,  32,  19,  37,  39,
  38,  36,  35,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   6,  97,   8,   7,   9,
  10,  11,  28,  41,  42,  43,  44,   0,  16,  47,
  12,  34,  13,  14,  15,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,  20,   0,
   0,  29,  33,  30,  31,  32,  19,  37,  39,  38,
  36,  35, 101,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   3,   6,   8,   7,   9,  10,  11,
  28,  41,  42,  43,  44,   0,  16,   5,  12,  34,
  13,  14,  15,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,  20,   0,   0,  29,
  33,  30,  31,  32,  19,  37,  39,  38,  36,  35,
   0,   0,   8,   7,   9,  10,  11,  28,  41,  42,
  43,  44,   6,  16,  47,  12,  34,  13,  14,  15,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,  20,   0,   0,  29,  33,  30,  31,
  32,  19,  37,  39,  38,  36,  35,   0,   0,   8,
   7,   9,  10,  11,  28,  41,  42,  43,  44,   6,
  16,   5,  12,  34,  13,  14,  15,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  20,   0,   0,  29,  33,  30,  31,  32,  19,  37,
  39,  38,  36,  35,   8,   7,   9,  10,  11,  28,
  41,  42,  43,  44,   0,  16,   6,  12,  34,  13,
  14,  15,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,  20,   0,   0,  29,  33,
  30,  31,  32,  19,  37,  39,  38,  36,  35,  51,
 122,  48,   0,  50,  49,   0,   0,   0,   0,   0,
   0,   6,   0,   0, 120,  49,   0,  61,  64,  54,
  62,  57,  58,  59,  60,  55,  56,  63,  61,  64,
  54,  62,  57,  58,  59,  60,  55,  56,  63
};
short	yypact[] =
{
 241,-1000, 288,-1000,  26,-1000, 335,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000, 380, 380, 380, 380,
 380,  32, 335, 380, 380, 380, 380,-1000,-1000,  66,
-1000,-1000,-1000,  66,-1000,  29,  66,  66,  66,  66,
  27,-1000,-1000,-1000,-1000,  26,-1000, 380, 380,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000, 124,  26,  96,  96,  96,
 -14,-1000,-1000, 183,  96,  96,  96,  96, -53,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000, 335,
-1000,  26, -14, 380, 380, 380, 380,-1000, 380, 380,
-1000,  10,  91,  53, 288, -56, 408, -14, 397,  26,
 408, -14,-1000,-1000,  -1,-1000,-1000, 335, 335,-1000,
 380,-1000, 380,-1000,-1000,-1000, 288,  50, -14,  26,
-1000
};
short	yypgo[] =
{
   0, 122,   6,   0, 117,   2, 116, 114, 111, 110,
 109, 108, 106,  98,  93,  92,  91,  89,  87,  75,
  73,  65,  51,   4,  47,  35,  16,  30,   1,  28
};
short	yyr1[] =
{
   0,   1,   1,   1,   2,   2,   2,   2,   4,   5,
   5,   6,   6,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   3,   3,   3,   3,   9,
   3,  10,   3,  12,   3,  13,   3,   3,  14,   3,
  15,   3,   3,   3,   3,   3,   3,   3,   3,   3,
  24,   3,  11,  19,  20,  21,  22,  18,  18,  18,
  18,  18,  18,  18,  18,  18,  18,  18,  16,  16,
  17,  17,  25,  25,  23,  29,  23,  27,  27,  27,
  27,  28,  28,   7,   8,   8,   8,   8,  26,  26
};
short	yyr2[] =
{
   0,   1,   1,   0,   1,   2,   2,   1,   2,   2,
   0,   2,   0,   3,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   3,   2,   2,   2,   2,   2,   0,
   5,   0,   4,   0,   5,   0,   4,   1,   0,   5,
   0,   4,   3,   2,   2,   2,   2,   2,   2,   1,
   0,   5,   1,   2,   2,   2,   2,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,
   2,   2,   1,   2,   4,   0,   6,   1,   1,   1,
   1,   1,   3,   2,   1,   1,   1,   2,   1,   1
};
short	yychk[] =
{
-1000,  -1,  -2,   2,  -3,  16,  61,   5,   4,   6,
   7,   8,  17,  19,  20,  21,  15,  -7,  -8,  43,
  35, -11, -16, -19, -20, -21, -22, -23,   9,  38,
  40,  41,  42,  39,  18,  48,  47,  44,  46,  45,
 -27,  10,  11,  12,  13,  -3,  -4,  16,  34,  37,
  36,  32,  33, -18,  52,  58,  59,  54,  55,  56,
  57,  50,  53,  60,  51,  -2,  -3,  -3,  -3,  -3,
  -3,  37,  36,  -2,  -3,  -3,  -3,  -3, -24, -26,
   4,   5, -26, -26,  61, -26, -26, -26, -26,  61,
 -26,  -3,  -3,  -9, -10, -14, -15,  62, -12, -13,
 -17,  49,  61, -28,  -2, -29,  -3,  -3,  -3,  -3,
  -3,  -3, -26,  62, -25, -23,  62,  14,  61,  -5,
  36,  -6,  33,  -5,  62, -23,  -2, -28,  -3,  -3,
  62
};
short	yydef[] =
{
  -2,  -2,   1,   2,   4,   7,   0,  14,  15,  16,
  17,  18,  19,  20,  21,  22,   0,   0,   0,   0,
   0,  37,   0,   0,   0,   0,   0,  49,  50,   0,
  84,  85,  86,   0,  52,   0,   0,   0,   0,   0,
   0,  77,  78,  79,  80,   5,   6,   0,   0,  29,
  31,  38,  40,  44,  57,  58,  59,  60,  61,  62,
  63,  64,  65,  66,  67,   0,  24,  25,  26,  27,
  28,  33,  35,  43,  45,  46,  47,  48,   0,  83,
  88,  89,  87,  68,  69,  53,  54,  55,  56,   0,
  75,   8,  23,   0,   0,   0,   0,  13,   0,   0,
  42,   0,   0,   0,  81,   0,  10,  32,  12,  41,
  10,  36,  70,  71,   0,  72,  74,   0,   0,  30,
   0,  39,   0,  34,  51,  73,  82,   0,   9,  11,
  76
};
short	yytok1[] =
{
   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,  61,   0,  62
};
short	yytok2[] =
{
   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,
  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,
  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,
  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
  52,  53,  54,  55,  56,  57,  58,  59,  60
};
long	yytok3[] =
{
   0
};
#define YYFLAG 		-1000
#define YYERROR		goto yyerrlab
#define YYACCEPT	return(0)
#define YYABORT		return(1)
#define	yyclearin	yychar = -1
#define	yyerrok		yyerrflag = 0

#ifdef	yydebug
#include	"y.debug"
#else
#define	yydebug		0
char*	yytoknames[1];		/* for debugging */
char*	yystates[1];		/* for debugging */
#endif

/*	parser for yacc output	*/

int	yynerrs = 0;		/* number of errors */
int	yyerrflag = 0;		/* error recovery flag */

char*
yytokname(int yyc)
{
	static char x[10];

	if(yyc > 0 && yyc <= sizeof(yytoknames)/sizeof(yytoknames[0]))
	if(yytoknames[yyc-1])
		return yytoknames[yyc-1];
	sprintf(x, "<%d>", yyc);
	return x;
}

char*
yystatname(int yys)
{
	static char x[10];

	if(yys >= 0 && yys < sizeof(yystates)/sizeof(yystates[0]))
	if(yystates[yys])
		return yystates[yys];
	sprintf(x, "<%d>\n", yys);
	return x;
}

long
yylex1(void)
{
	long yychar;
	long *t3p;
	int c;

	yychar = yylex();
	if(yychar <= 0) {
		c = yytok1[0];
		goto out;
	}
	if(yychar < sizeof(yytok1)/sizeof(yytok1[0])) {
		c = yytok1[yychar];
		goto out;
	}
	if(yychar >= YYPRIVATE)
		if(yychar < YYPRIVATE+sizeof(yytok2)/sizeof(yytok2[0])) {
			c = yytok2[yychar-YYPRIVATE];
			goto out;
		}
	for(t3p=yytok3;; t3p+=2) {
		c = t3p[0];
		if(c == yychar) {
			c = t3p[1];
			goto out;
		}
		if(c == 0)
			break;
	}
	c = 0;

out:
	if(c == 0)
		c = yytok2[1];	/* unknown char */
	if(yydebug >= 3)
		printf("lex %.4lX %s\n", yychar, yytokname(c));
	return c;
}

int
yyparse(void)
{
	struct
	{
		YYSTYPE	yyv;
		int	yys;
	} yys[YYMAXDEPTH], *yyp, *yypt;
	short *yyxi;
	int yyj, yym, yystate, yyn, yyg;
	YYSTYPE save1, save2;
	int save3, save4;
	long yychar;

	save1 = yylval;
	save2 = yyval;
	save3 = yynerrs;
	save4 = yyerrflag;

	yystate = 0;
	yychar = -1;
	yynerrs = 0;
	yyerrflag = 0;
	yyp = &yys[-1];
	goto yystack;

ret0:
	yyn = 0;
	goto ret;

ret1:
	yyn = 1;
	goto ret;

ret:
	yylval = save1;
	yyval = save2;
	yynerrs = save3;
	yyerrflag = save4;
	return yyn;

yystack:
	/* put a state and value onto the stack */
	if(yydebug >= 4)
		printf("char %s in %s", yytokname(yychar), yystatname(yystate));

	yyp++;
	if(yyp >= &yys[YYMAXDEPTH]) {
		yyerror("yacc stack overflow");
		goto ret1;
	}
	yyp->yys = yystate;
	yyp->yyv = yyval;

yynewstate:
	yyn = yypact[yystate];
	if(yyn <= YYFLAG)
		goto yydefault; /* simple state */
	if(yychar < 0)
		yychar = yylex1();
	yyn += yychar;
	if(yyn < 0 || yyn >= YYLAST)
		goto yydefault;
	yyn = yyact[yyn];
	if(yychk[yyn] == yychar) { /* valid shift */
		yychar = -1;
		yyval = yylval;
		yystate = yyn;
		if(yyerrflag > 0)
			yyerrflag--;
		goto yystack;
	}

yydefault:
	/* default state action */
	yyn = yydef[yystate];
	if(yyn == -2) {
		if(yychar < 0)
			yychar = yylex1();

		/* look through exception table */
		for(yyxi=yyexca;; yyxi+=2)
			if(yyxi[0] == -1 && yyxi[1] == yystate)
				break;
		for(yyxi += 2;; yyxi += 2) {
			yyn = yyxi[0];
			if(yyn < 0 || yyn == yychar)
				break;
		}
		yyn = yyxi[1];
		if(yyn < 0)
			goto ret0;
	}
	if(yyn == 0) {
		/* error ... attempt to resume parsing */
		switch(yyerrflag) {
		case 0:   /* brand new error */
			yyerror("syntax error");
			if(yydebug >= 1) {
				printf("%s", yystatname(yystate));
				printf("saw %s\n", yytokname(yychar));
			}
yyerrlab:
			yynerrs++;

		case 1:
		case 2: /* incompletely recovered error ... try again */
			yyerrflag = 3;

			/* find a state where "error" is a legal shift action */
			while(yyp >= yys) {
				yyn = yypact[yyp->yys] + YYERRCODE;
				if(yyn >= 0 && yyn < YYLAST) {
					yystate = yyact[yyn];  /* simulate a shift of "error" */
					if(yychk[yystate] == YYERRCODE)
						goto yystack;
				}

				/* the current yyp has no shift onn "error", pop stack */
				if(yydebug >= 2)
					printf("error recovery pops state %d, uncovers %d\n",
						yyp->yys, (yyp-1)->yys );
				yyp--;
			}
			/* there is no state on the stack with an error shift ... abort */
			goto ret1;

		case 3:  /* no shift yet; clobber input char */
			if(yydebug >= YYEOFCODE)
				printf("error recovery discards %s\n", yytokname(yychar));
			if(yychar == YYEOFCODE)
				goto ret1;
			yychar = -1;
			goto yynewstate;   /* try again in the same state */
		}
	}

	/* reduction by production yyn */
	if(yydebug >= 2)
		printf("reduce %d in:\n\t%s", yyn, yystatname(yystate));

	yypt = yyp;
	yyp -= yyr2[yyn];
	yyval = (yyp+1)->yyv;
	yym = yyn;

	/* consult goto table to find next state */
	yyn = yyr1[yyn];
	yyg = yypgo[yyn];
	yyj = yyg + yyp->yys + 1;

	if(yyj >= YYLAST || yychk[yystate=yyact[yyj]] != -yyn)
		yystate = yyact[yyg];
	switch(yym) {
		
case 1:
#line	24	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ putout(yypt[-0].yyv); } break;
case 2:
#line	25	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ ERROR "syntax error" WARNING; } break;
case 3:
#line	26	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ eqnreg = 0; } break;
case 5:
#line	30	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ eqnbox(yypt[-1].yyv, yypt[-0].yyv, 0); } break;
case 6:
#line	31	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ eqnbox(yypt[-1].yyv, yypt[-0].yyv, 1); } break;
case 7:
#line	32	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ lineup(0); } break;
case 8:
#line	35	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = yypt[-0].yyv; lineup(1); } break;
case 9:
#line	38	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = yypt[-0].yyv; } break;
case 10:
#line	39	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = 0; } break;
case 11:
#line	42	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = yypt[-0].yyv; } break;
case 12:
#line	43	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = 0; } break;
case 13:
#line	46	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = yypt[-1].yyv; } break;
case 14:
#line	47	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ text(QTEXT, (char *) yypt[-0].yyv); } break;
case 15:
#line	48	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ text(CONTIG, (char *) yypt[-0].yyv); } break;
case 16:
#line	49	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ text(SPACE, (char *) 0); } break;
case 17:
#line	50	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ text(THIN, (char *) 0); } break;
case 18:
#line	51	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ text(TAB, (char *) 0); } break;
case 19:
#line	52	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ funny(SUM); } break;
case 20:
#line	53	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ funny(PROD); } break;
case 21:
#line	54	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ funny(UNION); } break;
case 22:
#line	55	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ funny(INTER); } break;
case 23:
#line	56	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ boverb(yypt[-2].yyv, yypt[-0].yyv); } break;
case 24:
#line	57	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ mark(yypt[-0].yyv); } break;
case 25:
#line	58	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ size(yypt[-1].yyv, yypt[-0].yyv); } break;
case 26:
#line	59	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ font(yypt[-1].yyv, yypt[-0].yyv); } break;
case 27:
#line	60	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ fatbox(yypt[-0].yyv); } break;
case 28:
#line	61	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ sqrt(yypt[-0].yyv); } break;
case 29:
#line	62	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ps -= deltaps;} break;
case 30:
#line	62	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ subsup(yypt[-4].yyv, yypt[-1].yyv, yypt[-0].yyv); } break;
case 31:
#line	63	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ps -= deltaps;} break;
case 32:
#line	63	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ subsup(yypt[-3].yyv, 0, yypt[-0].yyv); } break;
case 33:
#line	64	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ps -= deltaps;} break;
case 34:
#line	64	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ integral(yypt[-4].yyv, yypt[-1].yyv, yypt[-0].yyv); } break;
case 35:
#line	65	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ps -= deltaps;} break;
case 36:
#line	65	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ integral(yypt[-3].yyv, 0, yypt[-0].yyv); } break;
case 37:
#line	66	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ integral(yypt[-0].yyv, 0, 0); } break;
case 38:
#line	67	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ps -= deltaps;} break;
case 39:
#line	67	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ fromto(yypt[-4].yyv, yypt[-1].yyv, yypt[-0].yyv); } break;
case 40:
#line	68	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ps -= deltaps;} break;
case 41:
#line	68	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ fromto(yypt[-3].yyv, 0, yypt[-0].yyv); } break;
case 42:
#line	69	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ paren(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv); } break;
case 43:
#line	70	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ paren(yypt[-1].yyv, yypt[-0].yyv, 0); } break;
case 44:
#line	71	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ diacrit(yypt[-1].yyv, yypt[-0].yyv); } break;
case 45:
#line	72	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ move(FWD, yypt[-1].yyv, yypt[-0].yyv); } break;
case 46:
#line	73	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ move(UP, yypt[-1].yyv, yypt[-0].yyv); } break;
case 47:
#line	74	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ move(BACK, yypt[-1].yyv, yypt[-0].yyv); } break;
case 48:
#line	75	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ move(DOWN, yypt[-1].yyv, yypt[-0].yyv); } break;
case 49:
#line	76	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ pile(yypt[-0].yyv); ct = yypt[-0].yyv; } break;
case 50:
#line	77	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{yyval=ct;} break;
case 51:
#line	77	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ matrix(yypt[-3].yyv); ct = yypt[-3].yyv; } break;
case 52:
#line	80	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ setintegral(); } break;
case 53:
#line	83	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = atoi((char *) yypt[-1].yyv); } break;
case 54:
#line	84	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = atoi((char *) yypt[-1].yyv); } break;
case 55:
#line	85	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = atoi((char *) yypt[-1].yyv); } break;
case 56:
#line	86	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = atoi((char *) yypt[-1].yyv); } break;
case 57:
#line	88	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = HAT; } break;
case 58:
#line	89	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = VEC; } break;
case 59:
#line	90	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = DYAD; } break;
case 60:
#line	91	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = BAR; } break;
case 61:
#line	92	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = LOWBAR; } break;
case 62:
#line	93	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = HIGHBAR; } break;
case 63:
#line	94	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = UNDER; } break;
case 64:
#line	95	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = DOT; } break;
case 65:
#line	96	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = TILDE; } break;
case 66:
#line	97	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = UTILDE; } break;
case 67:
#line	98	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = DOTDOT; } break;
case 68:
#line	101	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = ((char *)yypt[-0].yyv)[0]; } break;
case 69:
#line	102	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = '{'; } break;
case 70:
#line	105	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = ((char *)yypt[-0].yyv)[0]; } break;
case 71:
#line	106	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = '}'; } break;
case 74:
#line	113	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ column(yypt[-3].yyv, DEFGAP); } break;
case 75:
#line	114	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{yyval=atoi((char*)yypt[-0].yyv);} break;
case 76:
#line	114	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ column(yypt[-5].yyv, yypt[-3].yyv); } break;
case 77:
#line	117	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = startcol(LCOL); } break;
case 78:
#line	118	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = startcol(CCOL); } break;
case 79:
#line	119	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = startcol(RCOL); } break;
case 80:
#line	120	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = startcol(COL); } break;
case 81:
#line	123	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ lp[ct++] = yypt[-0].yyv; } break;
case 82:
#line	124	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ lp[ct++] = yypt[-0].yyv; } break;
case 83:
#line	127	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ yyval = ps; setsize((char *) yypt[-0].yyv); } break;
case 84:
#line	130	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ static char R[]="R"; setfont(R); } break;
case 85:
#line	131	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ static char I[]="I"; setfont(I); } break;
case 86:
#line	132	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ static char B[]="B"; setfont(B); } break;
case 87:
#line	133	"/usr/local/plan9/src/cmd/eqn/eqn.y"
{ setfont((char *)yypt[-0].yyv); } break;
	}
	goto yystack;  /* stack new state and value */
}
