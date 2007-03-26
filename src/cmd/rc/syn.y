%term FOR IN WHILE IF NOT TWIDDLE BANG SUBSHELL SWITCH FN
%term WORD REDIR DUP PIPE SUB
%term SIMPLE ARGLIST WORDS BRACE PAREN PCMD PIPEFD /* not used in syntax */
/* operator priorities -- lowest first */
%left IF WHILE FOR SWITCH ')' NOT
%left ANDAND OROR
%left BANG SUBSHELL
%left PIPE
%left '^'
%right '$' COUNT '"'
%left SUB
%{
#include "rc.h"
#include "fns.h"
%}
%union{
	struct tree *tree;
};
%type<tree> line paren brace body cmdsa cmdsan assign epilog redir
%type<tree> cmd simple first word comword keyword words
%type<tree> NOT FOR IN WHILE IF TWIDDLE BANG SUBSHELL SWITCH FN
%type<tree> WORD REDIR DUP PIPE
%%
rc:				{ return 1;}
|	line '\n'		{return !compile($1);}
line:	cmd
|	cmdsa line		{$$=tree2(';', $1, $2);}
body:	cmd
|	cmdsan body		{$$=tree2(';', $1, $2);}
cmdsa:	cmd ';'
|	cmd '&'			{$$=tree1('&', $1);}
cmdsan:	cmdsa
|	cmd '\n'
brace:	'{' body '}'		{$$=tree1(BRACE, $2);}
paren:	'(' body ')'		{$$=tree1(PCMD, $2);}
assign:	first '=' word		{$$=tree2('=', $1, $3);}
epilog:				{$$=0;}
|	redir epilog		{$$=mung2($1, $1->child[0], $2);}
redir:	REDIR word		{$$=mung1($1, $1->rtype==HERE?heredoc($2):$2);}
|	DUP
cmd:				{$$=0;}
|	brace epilog		{$$=epimung($1, $2);}
|	IF paren {skipnl();} cmd
				{$$=mung2($1, $2, $4);}
|	IF NOT {skipnl();} cmd	{$$=mung1($2, $4);}
|	FOR '(' word IN words ')' {skipnl();} cmd
	/*
	 * if ``words'' is nil, we need a tree element to distinguish between 
	 * for(i in ) and for(i), the former being a loop over the empty set
	 * and the latter being the implicit argument loop.  so if $5 is nil
	 * (the empty set), we represent it as "()".  don't parenthesize non-nil
	 * functions, to avoid growing parentheses every time we reread the
	 * definition.
	 */
				{$$=mung3($1, $3, $5 ? $5 : tree1(PAREN, $5), $8);}
|	FOR '(' word ')' {skipnl();} cmd
				{$$=mung3($1, $3, (struct tree *)0, $6);}
|	WHILE paren {skipnl();} cmd
				{$$=mung2($1, $2, $4);}
|	SWITCH word {skipnl();} brace
				{$$=tree2(SWITCH, $2, $4);}
|	simple			{$$=simplemung($1);}
|	TWIDDLE word words	{$$=mung2($1, $2, $3);}
|	cmd ANDAND cmd		{$$=tree2(ANDAND, $1, $3);}
|	cmd OROR cmd		{$$=tree2(OROR, $1, $3);}
|	cmd PIPE cmd		{$$=mung2($2, $1, $3);}
|	redir cmd  %prec BANG	{$$=mung2($1, $1->child[0], $2);}
|	assign cmd %prec BANG	{$$=mung3($1, $1->child[0], $1->child[1], $2);}
|	BANG cmd		{$$=mung1($1, $2);}
|	SUBSHELL cmd		{$$=mung1($1, $2);}
|	FN words brace		{$$=tree2(FN, $2, $3);}
|	FN words		{$$=tree1(FN, $2);}
simple:	first
|	simple word		{$$=tree2(ARGLIST, $1, $2);}
|	simple redir		{$$=tree2(ARGLIST, $1, $2);}
first:	comword	
|	first '^' word		{$$=tree2('^', $1, $3);}
word:	keyword			{lastword=1; $1->type=WORD;}
|	comword
|	word '^' word		{$$=tree2('^', $1, $3);}
comword: '$' word		{$$=tree1('$', $2);}
|	'$' word SUB words ')'	{$$=tree2(SUB, $2, $4);}
|	'"' word		{$$=tree1('"', $2);}
|	COUNT word		{$$=tree1(COUNT, $2);}
|	WORD
|	'`' brace		{$$=tree1('`', $2);}
|	'(' words ')'		{$$=tree1(PAREN, $2);}
|	REDIR brace		{$$=mung1($1, $2); $$->type=PIPEFD;}
keyword: FOR|IN|WHILE|IF|NOT|TWIDDLE|BANG|SUBSHELL|SWITCH|FN
words:				{$$=(struct tree*)0;}
|	words word		{$$=tree2(WORDS, $1, $2);}
