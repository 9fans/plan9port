#include "rc.h"
#include "io.h"
#include "fns.h"

static tree*	body(int tok, int *ptok);
static tree*	brace(int tok);
static tree*	cmd(int tok, int *ptok);
static tree*	cmd2(int tok, int *ptok);
static tree*	cmd3(int tok, int *ptok);
static tree*	cmds(int tok, int *ptok, int nlok);
static tree*	epilog(int tok, int *ptok);
static int	iswordtok(int tok);
static tree*	line(int tok, int *ptok);
static tree*	paren(int tok);
static tree*	yyredir(int tok, int *ptok);
static tree*	yyword(int tok, int *ptok, int eqok);
static tree*	word1(int tok, int *ptok);
static tree*	words(int tok, int *ptok);

static jmp_buf yyjmp;

static int
dropnl(int tok)
{
	while(tok == ' ' || tok == '\n')
		tok = yylex();
	return tok;
}

static int
dropsp(int tok)
{
	while(tok == ' ')
		tok = yylex();
	return tok;
}

static void
syntax(int tok)
{
	USED(tok);
	yyerror("syntax error");
	longjmp(yyjmp, 1);
}

int
parse(void)
{
	tree *t;
	int tok;

	if(setjmp(yyjmp))
		return 1;

	// rc:				{ return 1;}
	// |	line '\n'		{return !compile($1);}

	tok = dropsp(yylex());
	if(tok == EOF)
		return 1;
	t = line(tok, &tok);
	if(tok != '\n')
		yyerror("missing newline at end of line");
	yylval.tree = t;
	return !compile(t);
}

static tree*
line(int tok, int *ptok)
{
	return cmds(tok, ptok, 0);
}

static tree*
body(int tok, int *ptok)
{
	return cmds(tok, ptok, 1);
}

static tree*
cmds(int tok, int *ptok, int nlok)
{
	tree *t, **last, *t2;

	// line:	cmd
	// |	cmdsa line		{$$=tree2(';', $1, $2);}
	// cmdsa:	cmd ';'
	// |	cmd '&'			{$$=tree1('&', $1);}

	// body:	cmd
	// |	cmdsan body		{$$=tree2(';', $1, $2);}
	// cmdsan:	cmdsa
	// |	cmd '\n'

	t = nil;
	last = nil;
	for(;;) {
		t2 = cmd(tok, &tok);
		if(tok == '&')
			t2 = tree1('&', t2);
		if(t2 != nil) {
			// slot into list t
			if(last == nil) {
				t = t2;
				last = &t;
			} else {
				*last = tree2(';', *last, t2);
				last = &(*last)->child[1];
			}
		}
		if(tok != ';' && tok != '&' && (!nlok || tok != '\n'))
			break;
		tok = yylex();
	}
	*ptok = tok;
	return t;
}

static tree*
brace(int tok)
{
	tree *t;

	// brace:	'{' body '}'		{$$=tree1(BRACE, $2);}

	tok = dropsp(tok);
	if(tok != '{')
		syntax(tok);
	t = body(yylex(), &tok);
	if(tok != '}')
		syntax(tok);
	return tree1(BRACE, t);
}

static tree*
paren(int tok)
{
	tree *t;

	// paren:	'(' body ')'		{$$=tree1(PCMD, $2);}

	tok = dropsp(tok);
	if(tok != '(')
		syntax(tok);
	t = body(yylex(), &tok);
	if(tok != ')')
		syntax(tok);
	return tree1(PCMD, t);
}

static tree*
epilog(int tok, int *ptok)
{
	tree *t, *r;

	// epilog:				{$$=0;}
	// |	redir epilog		{$$=mung2($1, $1->child[0], $2);}

	if(tok != REDIR && tok != DUP) {
		*ptok = tok;
		return nil;
	}

	r = yyredir(tok, &tok);
	t = epilog(tok, &tok);
	*ptok = tok;
	return mung2(r, r->child[0], t);
}

static tree*
yyredir(int tok, int *ptok)
{
	tree *r, *w;

	// redir:	REDIR word		{$$=mung1($1, $1->rtype==HERE?heredoc($2):$2);}
	// |	DUP

	switch(tok) {
	default:
		syntax(tok);
	case DUP:
		r = yylval.tree;
		*ptok = dropsp(yylex());
		break;
	case REDIR:
		r = yylval.tree;
		w = yyword(yylex(), &tok, 1);
		*ptok = dropsp(tok);
		r = mung1(r, r->rtype==HERE?heredoc(w):w);
		break;
	}
	return r;
}

static tree*
cmd(int tok, int *ptok)
{
	int op;
	tree *t1, *t2;

	// |	cmd ANDAND cmd		{$$=tree2(ANDAND, $1, $3);}
	// |	cmd OROR cmd		{$$=tree2(OROR, $1, $3);}

	tok = dropsp(tok);
	t1 = cmd2(tok, &tok);
	while(tok == ANDAND || tok == OROR) {
		op = tok;
		t2 = cmd2(dropnl(yylex()), &tok);
		t1 = tree2(op, t1, t2);
	}
	*ptok = tok;
	return t1;
}

static tree*
cmd2(int tok, int *ptok)
{
	tree *t1, *t2, *t3;

	// |	cmd PIPE cmd		{$$=mung2($2, $1, $3);}
	t1 = cmd3(tok, &tok);
	while(tok == PIPE) {
		t2 = yylval.tree;
		t3 = cmd3(dropnl(yylex()), &tok);
		t1 = mung2(t2, t1, t3);
	}
	*ptok = tok;
	return t1;
}

static tree*
cmd3(int tok, int *ptok)
{
	tree *t1, *t2, *t3, *t4;

	tok = dropsp(tok);
	switch(tok) {
	case ';':
	case '&':
	case '\n':
		*ptok = tok;
		return nil;

	case IF:
		// |	IF paren {skipnl();} cmd	{$$=mung2($1, $2, $4);}
		// |	IF NOT {skipnl();} cmd	{$$=mung1($2, $4);}
		t1 = yylval.tree;
		tok = dropsp(yylex());
		if(tok == NOT) {
			t1 = yylval.tree;
			t2 = cmd(dropnl(yylex()), ptok);
			return mung1(t1, t2);
		}
		t2 = paren(tok);
		t3 = cmd(dropnl(yylex()), ptok);
		return mung2(t1, t2, t3);

	case FOR:
		// |	FOR '(' word IN words ')' {skipnl();} cmd
		//		{$$=mung3($1, $3, $5 ? $5 : tree1(PAREN, $5), $8);}
		// |	FOR '(' word ')' {skipnl();} cmd
		//		{$$=mung3($1, $3, (tree *)0, $6);}
		t1 = yylval.tree;
		tok = dropsp(yylex());
		if(tok != '(')
			syntax(tok);
		t2 = yyword(yylex(), &tok, 1);
		switch(tok) {
		default:
			syntax(tok);
		case ')':
			t3 = nil;
			break;
		case IN:
			t3 = words(yylex(), &tok);
			if(t3 == nil)
				t3 = tree1(PAREN, nil);
			if(tok != ')')
				syntax(tok);
			break;
		}
		t4 = cmd(dropnl(yylex()), ptok);
		return mung3(t1, t2, t3, t4);

	case WHILE:
		// |	WHILE paren {skipnl();} cmd
		//		{$$=mung2($1, $2, $4);}
		t1 = yylval.tree;
		t2 = paren(yylex());
		t3 = cmd(dropnl(yylex()), ptok);
		return mung2(t1, t2, t3);

	case SWITCH:
		// |	SWITCH word {skipnl();} brace
		//		{$$=tree2(SWITCH, $2, $4);}
		t1 = yyword(yylex(), &tok, 1);
		tok = dropnl(tok); // doesn't work in yacc grammar but works here!
		t2 = brace(tok);
		*ptok = dropsp(yylex());
		return tree2(SWITCH, t1, t2);
		// Note: cmd: a && for(x) y && b is a && {for (x) {y && b}}.
		return cmd(tok, ptok);

	case FN:
		// |	FN words brace		{$$=tree2(FN, $2, $3);}
		// |	FN words		{$$=tree1(FN, $2);}
		t1 = words(yylex(), &tok);
		if(tok != '{') {
			*ptok = tok;
			return tree1(FN, t1);
		}
		t2 = brace(tok);
		*ptok = dropsp(yylex());
		return tree2(FN, t1, t2);

	case TWIDDLE:
		// |	TWIDDLE word words	{$$=mung2($1, $2, $3);}
		t1 = yylval.tree;
		t2 = yyword(yylex(), &tok, 1);
		t3 = words(tok, ptok);
		return mung2(t1, t2, t3);

	case BANG:
	case SUBSHELL:
		// |	BANG cmd		{$$=mung1($1, $2);}
		// |	SUBSHELL cmd		{$$=mung1($1, $2);}
		// Note: cmd2: ! x | y is !{x | y} not {!x} | y.
		t1 = yylval.tree;
		return mung1(t1, cmd2(yylex(), ptok));

	case REDIR:
	case DUP:
		// |	redir cmd  %prec BANG	{$$=mung2($1, $1->child[0], $2);}
		// Note: cmd2: {>x echo a | tr a-z A-Z} writes A to x.
		t1 = yyredir(tok, &tok);
		t2 = cmd2(tok, ptok);
		return mung2(t1, t1->child[0], t2);

	case '{':
		// |	brace epilog		{$$=epimung($1, $2);}
		t1 = brace(tok);
		tok = dropsp(yylex());
		t2 = epilog(tok, ptok);
		return epimung(t1, t2);
	}

	if(!iswordtok(tok)) {
		*ptok = tok;
		return nil;
	}

	// cmd: ...
	// |	simple			{$$=simplemung($1);}
	// |	assign cmd %prec BANG	{$$=mung3($1, $1->child[0], $1->child[1], $2);}
	// assign:	first '=' word		{$$=tree2('=', $1, $3);}
	// Note: first is same as word except for disallowing all the leading keywords,
	// but all those keywords have been picked off in the switch above.
	// Except NOT, but disallowing that in yacc was likely a mistake anyway:
	// there's no ambiguity in not=1 or not x y z.
	t1 = yyword(tok, &tok, 0);
	if(tok == '=') {
		// assignment
		// Note: cmd2: {x=1 true | echo $x} echoes 1.
		t1 = tree2('=', t1, yyword(yylex(), &tok, 1));
		t2 = cmd2(tok, ptok);
		return mung3(t1, t1->child[0], t1->child[1], t2);
	}

	// simple:	first
	// |	simple word		{$$=tree2(ARGLIST, $1, $2);}
	// |	simple redir		{$$=tree2(ARGLIST, $1, $2);}
	for(;;) {
		if(tok == REDIR || tok == DUP) {
			t1 = tree2(ARGLIST, t1, yyredir(tok, &tok));
		} else if(iswordtok(tok)) {
			t1 = tree2(ARGLIST, t1, yyword(tok, &tok, 1));
		} else {
			break;
		}
	}
	*ptok = tok;
	return simplemung(t1);
}

static tree*
words(int tok, int *ptok)
{
	tree *t;

	// words:				{$$=(tree*)0;}
	// |	words word		{$$=tree2(WORDS, $1, $2);}

	t = nil;
	tok = dropsp(tok);
	while(iswordtok(tok))
		t = tree2(WORDS, t, yyword(tok, &tok, 1));
	*ptok = tok;
	return t;
}

static tree*
yyword(int tok, int *ptok, int eqok)
{
	tree *t;

	// word:	keyword			{lastword=1; $1->type=WORD;}
	// |	comword
	// |	word '^' word		{$$=tree2('^', $1, $3);}
	// comword: '$' word		{$$=tree1('$', $2);}
	// |	'$' word SUB words ')'	{$$=tree2(SUB, $2, $4);}
	// |	'"' word		{$$=tree1('"', $2);}
	// |	COUNT word		{$$=tree1(COUNT, $2);}
	// |	WORD
	// |	'`' brace		{$$=tree1('`', $2);}
	// |	'(' words ')'		{$$=tree1(PAREN, $2);}
	// |	REDIR brace		{$$=mung1($1, $2); $$->type=PIPEFD;}
	// keyword: FOR|IN|WHILE|IF|NOT|TWIDDLE|BANG|SUBSHELL|SWITCH|FN
	//
	// factored into:
	//
	// word: word1
	// | word '^' word1
	//
	// word1: keyword | comword

	t = word1(tok, &tok);
	if(tok == '=' && !eqok)
		goto out;
	for(;;) {
		if(iswordtok(tok)) {
			// No free carats around parens.
			if(t->type == PAREN || tok == '(')
				syntax(tok);
			t = tree2('^', t, word1(tok, &tok));
			continue;
		}
		tok = dropsp(tok);
		if(tok == '^') {
			t = tree2('^', t, word1(yylex(), &tok));
			continue;
		}
		break;
	}
out:
	*ptok = dropsp(tok);
	return t;
}

static tree*
word1(int tok, int *ptok)
{
	tree *w, *sub, *t;

	tok = dropsp(tok);
	switch(tok) {
	default:
		syntax(tok);

	case WORD:
	case FOR:
	case IN:
	case WHILE:
	case IF:
	case NOT:
	case TWIDDLE:
	case BANG:
	case SUBSHELL:
	case SWITCH:
	case FN:
		// |	WORD
		// keyword: FOR|IN|WHILE|IF|NOT|TWIDDLE|BANG|SUBSHELL|SWITCH|FN
		t = yylval.tree;
		t->type = WORD;
		*ptok = yylex();
		return t;

	case '=':
		*ptok = yylex();
		return token("=", WORD);

	case '$':
		// comword: '$' word1		{$$=tree1('$', $2);}
		// |	'$' word1 SUB words ')'	{$$=tree2(SUB, $2, $4);}
		w = word1(yylex(), &tok);
		if(tok == '(') {
			sub = words(yylex(), &tok);
			if(tok != ')')
				syntax(tok);
			*ptok = yylex();
			return tree2(SUB, w, sub);
		}
		*ptok = tok;
		return tree1('$', w);

	case '"':
		// |	'"' word1		{$$=tree1('"', $2);}
		return tree1('"', word1(yylex(), ptok));

	case COUNT:
		// |	COUNT word1		{$$=tree1(COUNT, $2);}
		return tree1(COUNT, word1(yylex(), ptok));

	case '`':
		// |	'`' brace		{$$=tree1('`', $2);}
		t = tree1('`', brace(yylex()));
		*ptok = yylex();
		return t;

	case '(':
		// |	'(' words ')'		{$$=tree1(PAREN, $2);}
		t = tree1(PAREN, words(yylex(), &tok));
		if(tok != ')')
			syntax(tok);
		*ptok = yylex();
		return t;

	case REDIRW:
		// |	REDIRW brace		{$$=mung1($1, $2); $$->type=PIPEFD;}
		t = yylval.tree;
		t = mung1(t, brace(yylex()));
		t->type = PIPEFD;
		*ptok = yylex();
		return t;
	}
}

static int
iswordtok(int tok)
{
	switch(tok) {
	case FOR:
	case IN:
	case WHILE:
	case IF:
	case NOT:
	case TWIDDLE:
	case BANG:
	case SUBSHELL:
	case SWITCH:
	case FN:
	case '$':
	case '"':
	case COUNT:
	case WORD:
	case '`':
	case '(':
	case REDIRW:
	case '=':
		return 1;
	}
	return 0;
}
