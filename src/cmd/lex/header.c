# include "ldefs.h"

extern int nine;

void
phead1(void)
{
	Bprint(&fout,"typedef unsigned char Uchar;\n");
	if (nine) {
		Bprint(&fout,"# include <u.h>\n");
		Bprint(&fout,"# include <libc.h>\n");
	}
	Bprint(&fout,"# include <stdio.h>\n");
	Bprint(&fout, "# define U(x) x\n");
	Bprint(&fout, "# define NLSTATE yyprevious=YYNEWLINE\n");
	Bprint(&fout,"# define BEGIN yybgin = yysvec + 1 +\n");
	Bprint(&fout,"# define INITIAL 0\n");
	Bprint(&fout,"# define YYLERR yysvec\n");
	Bprint(&fout,"# define YYSTATE (yyestate-yysvec-1)\n");
	Bprint(&fout,"# define YYOPTIM 1\n");
# ifdef DEBUG
	Bprint(&fout,"# define LEXDEBUG 1\n");
# endif
	Bprint(&fout,"# define YYLMAX 200\n");
	Bprint(&fout,
"# define unput(c) {yytchar= (c);if(yytchar=='\\n')yylineno--;*yysptr++=yytchar;}\n");
	Bprint(&fout,"# define yymore() (yymorfg=1)\n");
	Bprint(&fout,"# define ECHO fprintf(yyout, \"%%s\",yytext)\n");
	Bprint(&fout,"# define REJECT { nstr = yyreject(); goto yyfussy;}\n");
	Bprint(&fout,"int yyleng; extern char yytext[];\n");
	Bprint(&fout,"int yymorfg;\n");
	Bprint(&fout,"extern Uchar *yysptr, yysbuf[];\n");
	Bprint(&fout,"int yytchar;\n");
/*	Bprint(&fout,"FILE *yyin = stdin, *yyout = stdout;\n"); */
	Bprint(&fout,"FILE *yyin, *yyout;\n");
	Bprint(&fout,"extern int yylineno;\n");
	Bprint(&fout,"struct yysvf { \n");
	Bprint(&fout,"\tstruct yywork *yystoff;\n");
	Bprint(&fout,"\tstruct yysvf *yyother;\n");
	Bprint(&fout,"\tint *yystops;};\n");
	Bprint(&fout,"struct yysvf *yyestate;\n");
	Bprint(&fout,"extern struct yysvf yysvec[], *yybgin;\n");
	Bprint(&fout,"int yylook(void), yywrap(void), yyback(int *, int);\n");
	if(nine) {
		Bprint(&fout,
				"int infd, outfd;\n"
				"\n"
				"void\n"
				"output(char c)\n"
				"{\n"
				"	int rv;\n"
				"	if ((rv = write(outfd, &c, 1)) < 0)\n"
				"		sysfatal(\"output: %%r\");\n"
				"	if (rv == 0)\n"
				"		sysfatal(\"output: EOF?\");\n"
				"}\n"
				"\n"
				"int\n"
				"input(void)\n"
				"{\n"
				"	if(yysptr > yysbuf)\n"
				"		yytchar = U(*--yysptr);\n"
				"	else {\n"
				"		int rv;\n"
				"		if ((rv = read(infd, &yytchar, 1)) < 0)\n"
				"			sysfatal(\"input: %%r\");\n"
				"		if (rv == 0)\n"
				"			return 0;\n"
				"	}\n"
				"	if (yytchar == '\\n')\n"
				"		yylineno++;\n"
				"	return yytchar;\n"
				"}\n");
	}
	else {
		Bprint(&fout,"# define output(c) putc(c,yyout)\n");
		Bprint(&fout, "%s%d%s\n",
 		 "# define input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==",
		'\n',
 		"?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)");
	}
}

void
phead2(void)
{
	Bprint(&fout,"while((nstr = yylook()) >= 0){\n");
	Bprint(&fout,"goto yyfussy; yyfussy: switch(nstr){\n");
	Bprint(&fout,"case 0:\n");
	Bprint(&fout,"if(yywrap()) return(0); break;\n");
}

void
ptail(void)
{
	if(!pflag){
		Bprint(&fout,"case -1:\nbreak;\n");		/* for reject */
		Bprint(&fout,"default:\n");
		Bprint(&fout,"fprintf(yyout,\"bad switch yylook %%d\",nstr);\n");
		Bprint(&fout,"}} return(0); }\n");
		Bprint(&fout,"/* end of yylex */\n");
	}
	pflag = 1;
}

void
statistics(void)
{
	fprint(errorf,"%d/%d nodes(%%e), %d/%d positions(%%p), %d/%d (%%n), %ld transitions\n",
		tptr, treesize, (int)(nxtpos-positions), maxpos, stnum+1, nstates, rcount);
	fprint(errorf, ", %d/%d packed char classes(%%k)", (int)(pcptr-pchar), pchlen);
	fprint(errorf,", %d/%d packed transitions(%%a)",nptr, ntrans);
	fprint(errorf, ", %d/%d output slots(%%o)", yytop, outsize);
	fprint(errorf,"\n");
}
