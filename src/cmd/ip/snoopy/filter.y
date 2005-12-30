%{
#include <u.h>
#include <libc.h>
#include <ctype.h>
#include "dat.h"

char	*yylp;		/* next character to be lex'd */
char	*yybuffer;
char	*yyend;		/* end of buffer to be parsed */
%}

%term LOR
%term LAND
%term WORD
%right '!'
%left '|'
%left '&'
%left LOR
%left LAND
%start filter
%%

filter		: expr
			{ filter = $$; }
		;
expr		: WORD
			{ $$ = $1; }
		| WORD '=' WORD
			{ $2->l = $1; $2->r = $3; $$ = $2; }
		| WORD '(' expr ')'
			{ $1->l = $3; free($2); free($4); $$ = $1; }
		| '(' expr ')'
			{ free($1); free($3); $$ = $2; }
		| expr LOR expr
			{ $2->l = $1; $2->r = $3; $$ = $2; }
		| expr LAND expr
			{ $2->l = $1; $2->r = $3; $$ = $2; }
		| '!' expr
			{ $1->l = $2; $$ = $1; }
		;
%%

/*
 *  Initialize the parsing.  Done once for each header field.
 */
void
yyinit(char *p)
{
	yylp = p;
}

int
yylex(void)
{
	char *p;
	int c;

	if(yylp == nil || *yylp == 0)
		return 0;
	while(isspace(*yylp))
		yylp++;

	yylval = newfilter();

	p = strpbrk(yylp, "!|&()= ");
	if(p == 0){
		yylval->op = WORD;
		yylval->s = strdup(yylp);
		if(yylval->s == nil)
			sysfatal("parsing filter: %r");
		yylp = nil;
		return WORD;
	}
	c = *p;
	if(p != yylp){
		yylval->op = WORD;
		*p = 0;
		yylval->s = strdup(yylp);
		if(yylval->s == nil)
			sysfatal("parsing filter: %r");
		*p = c;
		yylp = p;
		return WORD;
	}

	yylp++;
	if(*yylp == c)
		switch(c){
		case '&':
			c = LAND;
			yylp++;
			break;
		case '|':
			c = LOR;
			yylp++;
			break;
		}
	yylval->op = c;
	return c;
}

void
yyerror(char *e)
{
	USED(e);
	sysfatal("error parsing filter");
}
