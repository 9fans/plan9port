%{
	#include	<u.h>
	#include	<libc.h>
	#include	<bio.h>

	#define	bsp_max	5000

	Biobuf	*in;
	Biobuf	bstdin;
	Biobuf	bstdout;
	char	cary[1000];
	char*	cp = { cary };
	char	string[1000];
	char*	str = { string };
	int	crs = 128;
	int	rcrs = 128;	/* reset crs */
	int	bindx = 0;
	int	lev = 0;
	int	ln;
	char*	ttp;
	char*	ss = "";
	int	bstack[10] = { 0 };
	char*	numb[15] =
	{
		" 0", " 1", " 2", " 3", " 4", " 5",
		" 6", " 7", " 8", " 9", " 10", " 11",
		" 12", " 13", " 14"
	};
	char*	pre;
	char*	post;

	long	peekc = -1;
	int	sargc;
	int	ifile;
	char**	sargv;

	char	*funtab[] =
	{
		"<1>","<2>","<3>","<4>","<5>",
		"<6>","<7>","<8>","<9>","<10>",
		"<11>","<12>","<13>","<14>","<15>",
		"<16>","<17>","<18>","<19>","<20>",
		"<21>","<22>","<23>","<24>","<25>",
		"<26>"
	};
	char	*atab[] =
	{
		"<221>","<222>","<223>","<224>","<225>",
		"<226>","<227>","<228>","<229>","<230>",
		"<231>","<232>","<233>","<234>","<235>",
		"<236>","<237>","<238>","<239>","<240>",
		"<241>","<242>","<243>","<244>","<245>",
		"<246>"
	};
	char*	letr[26] =
	{
		"a","b","c","d","e","f","g","h","i","j",
		"k","l","m","n","o","p","q","r","s","t",
		"u","v","w","x","y","z"
	};
	char*	dot = { "." };
	char*	bspace[bsp_max];
	char**	bsp_nxt = bspace;
	int	bdebug = 0;
	int	lflag;
	int	cflag;
	int	sflag;

	char*	bundle(int, ...);
	void	conout(char*, char*);
	int	cpeek(int, int, int);
	int	getch(void);
	char*	geta(char*);
	char*	getf(char*);
	void	getout(void);
	void	output(char*);
	void	pp(char*);
	void	routput(char*);
	void	tp(char*);
	void	yyerror(char*, ...);
	int	yyparse(void);

	typedef	void*	pointer;
	#pragma	varargck	type	"lx"	pointer

%}
%union
{
	char*	cptr;
	int	cc;
}

%type	<cptr>	pstat stat stat1 def slist dlets e ase nase
%type	<cptr>	slist re fprefix cargs eora cons constant lora
%type	<cptr>	crs

%token	<cptr>	LETTER EQOP _AUTO DOT
%token	<cc>	DIGIT SQRT LENGTH _IF FFF EQ
%token	<cc>	_PRINT _WHILE _FOR NE LE GE INCR DECR
%token	<cc>	_RETURN _BREAK _DEFINE BASE OBASE SCALE
%token	<cc>	QSTR ERROR

%right	'=' EQOP
%left	'+' '-'
%left	'*' '/' '%'
%right	'^'
%left	UMINUS

%%
start:
	start stuff
|	stuff

stuff:
	pstat tail
	{
		output($1);
	}
|	def dargs ')' '{' dlist slist '}'
	{
		ttp = bundle(6, pre, $6, post , "0", numb[lev], "Q");
		conout(ttp, (char*)$1);
		rcrs = crs;
		output("");
		lev = bindx = 0;
	}

dlist:
	tail
|	dlist _AUTO dlets tail

stat:
	stat1
|	nase
	{
		if(sflag)
			bundle(2, $1, "s.");
	}

pstat:
	stat1
	{
		if(sflag)
			bundle(2, $1, "0");
	}
|	nase
	{
		if(!sflag)
			bundle(2, $1, "ps.");
	}

stat1:
	{
		bundle(1, "");
	}
|	ase
	{
		bundle(2, $1, "s.");
	}
|	SCALE '=' e
	{
		bundle(2, $3, "k");
	}
|	SCALE EQOP e
	{
		bundle(4, "K", $3, $2, "k");
	}
|	BASE '=' e
	{
		bundle(2, $3, "i");
	}
|	BASE EQOP e
	{
		bundle(4, "I", $3, $2, "i");
	}
|	OBASE '=' e
	{
		bundle(2, $3, "o");
	}
|	OBASE EQOP e
	{
		bundle(4, "O", $3, $2, "o");
	}
|	QSTR
	{
		bundle(3, "[", $1, "]P");
	}
|	_BREAK
	{
		bundle(2, numb[lev-bstack[bindx-1]], "Q");
	}
|	_PRINT e
	{
		bundle(2, $2, "ps.");
	}
|	_RETURN e
	{
		bundle(4, $2, post, numb[lev], "Q");
	}
|	_RETURN
	{
		bundle(4, "0", post, numb[lev], "Q");
	}
|	'{' slist '}'
	{
		$$ = $2;
	}
|	FFF
	{
		bundle(1, "fY");
	}
|	_IF crs BLEV '(' re ')' stat
	{
		conout($7, $2);
		bundle(3, $5, $2, " ");
	}
|	_WHILE crs '(' re ')' stat BLEV
	{
		bundle(3, $6, $4, $2);
		conout($$, $2);
		bundle(3, $4, $2, " ");
	}
|	fprefix crs re ';' e ')' stat BLEV
	{
		bundle(5, $7, $5, "s.", $3, $2);
		conout($$, $2);
		bundle(5, $1, "s.", $3, $2, " ");
	}
|	'~' LETTER '=' e
	{
		bundle(3, $4, "S", $2);
	}

fprefix:
	_FOR '(' e ';'
	{
		$$ = $3;
	}

BLEV:
	=
	{
		--bindx;
	}

slist:
	stat
|	slist tail stat
	{
		bundle(2, $1, $3);
	}

tail:
	'\n'
	{
		ln++;
	}
|	';'

re:
	e EQ e
	{
		$$ = bundle(3, $1, $3, "=");
	}
|	e '<' e
	{
		bundle(3, $1, $3, ">");
	}
|	e '>' e
	{
		bundle(3, $1, $3, "<");
	}
|	e NE e
	{
		bundle(3, $1, $3, "!=");
	}
|	e GE e
	{
		bundle(3, $1, $3, "!>");
	}
|	e LE e
	{
		bundle(3, $1, $3, "!<");
	}
|	e
	{
		bundle(2, $1, " 0!=");
	}

nase:
	'(' e ')'
	{
		$$ = $2;
	}
|	cons
	{
		bundle(3, " ", $1, " ");
	}
|	DOT cons
	{
		bundle(3, " .", $2, " ");
	}
|	cons DOT cons
	{
		bundle(5, " ", $1, ".", $3, " ");
	}
|	cons DOT
	{
		bundle(4, " ", $1, ".", " ");
	}
|	DOT
	{
		$<cptr>$ = "l.";
	}
|	LETTER '[' e ']'
	{
		bundle(3, $3, ";", geta($1));
	}
|	LETTER INCR
	{
		bundle(4, "l", $1, "d1+s", $1);
	}
|	INCR LETTER
	{
		bundle(4, "l", $2, "1+ds", $2);
	}
|	DECR LETTER
	{
		bundle(4, "l", $2, "1-ds", $2);
	}
|	LETTER DECR
	{
		bundle(4, "l", $1, "d1-s", $1);
	}
|	LETTER '[' e ']' INCR
	{
		bundle(7, $3, ";", geta($1), "d1+" ,$3, ":" ,geta($1));
	}
|	INCR LETTER '[' e ']'
	{
		bundle(7, $4, ";", geta($2), "1+d", $4, ":", geta($2));
	}
|	LETTER '[' e ']' DECR
	{
		bundle(7, $3, ";", geta($1), "d1-", $3, ":", geta($1));
	}
|	DECR LETTER '[' e ']'
	{
		bundle(7, $4, ";", geta($2), "1-d", $4, ":" ,geta($2));
	}
|	SCALE INCR
	{
		bundle(1, "Kd1+k");
	}
|	INCR SCALE
	{
		bundle(1, "K1+dk");
	}
|	SCALE DECR
	{
		bundle(1, "Kd1-k");
	}
|	DECR SCALE
	{
		bundle(1, "K1-dk");
	}
|	BASE INCR
	{
		bundle(1, "Id1+i");
	}
|	INCR BASE
	{
		bundle(1, "I1+di");
	}
|	BASE DECR
	{
		bundle(1, "Id1-i");
	}
|	DECR BASE
	{
		bundle(1, "I1-di");
	}
|	OBASE INCR
	{
		bundle(1, "Od1+o");
	}
|	INCR OBASE
	{
		bundle(1, "O1+do");
	}
|	OBASE DECR
	{
		bundle(1, "Od1-o");
	}
|	DECR OBASE
	{
		bundle(1, "O1-do");
	}
|	LETTER '(' cargs ')'
	{
		bundle(4, $3, "l", getf($1), "x");
	}
|	LETTER '(' ')'
	{
		bundle(3, "l", getf($1), "x");
	}
|	LETTER = {
		bundle(2, "l", $1);
	}
|	LENGTH '(' e ')'
	{
		bundle(2, $3, "Z");
	}
|	SCALE '(' e ')'
	{
		bundle(2, $3, "X");
	}
|	'?'
	{
		bundle(1, "?");
	}
|	SQRT '(' e ')'
	{
		bundle(2, $3, "v");
	}
|	'~' LETTER
	{
		bundle(2, "L", $2);
	}
|	SCALE
	{
		bundle(1, "K");
	}
|	BASE
	{
		bundle(1, "I");
	}
|	OBASE
	{
		bundle(1, "O");
	}
|	'-' e
	{
		bundle(3, " 0", $2, "-");
	}
|	e '+' e
	{
		bundle(3, $1, $3, "+");
	}
|	e '-' e
	{
		bundle(3, $1, $3, "-");
	}
|	e '*' e
	{
		bundle(3, $1, $3, "*");
	}
|	e '/' e
	{
		bundle(3, $1, $3, "/");
	}
|	e '%' e
	{
		bundle(3, $1, $3, "%%");
	}
|	e '^' e
	{
		bundle(3, $1, $3, "^");
	}

ase:
	LETTER '=' e
	{
		bundle(3, $3, "ds", $1);
	}
|	LETTER '[' e ']' '=' e
	{
		bundle(5, $6, "d", $3, ":", geta($1));
	}
|	LETTER EQOP e
	{
		bundle(6, "l", $1, $3, $2, "ds", $1);
	}
|	LETTER '[' e ']' EQOP e
	{
		bundle(9, $3, ";", geta($1), $6, $5, "d", $3, ":", geta($1));
	}

e:
	ase
|	nase

cargs:
	eora
|	cargs ',' eora
	{
		bundle(2, $1, $3);
	}

eora:
	e
|	LETTER '[' ']'
	{
		bundle(2, "l", geta($1));
	}

cons:
	constant
	{
		*cp++ = 0;
	}

constant:
	'_'
	{
		$<cptr>$ = cp;
		*cp++ = '_';
	}
|	DIGIT
	{
		$<cptr>$ = cp;
		*cp++ = $1;
	}
|	constant DIGIT
	{
		*cp++ = $2;
	}

crs:
	=
	{
		$$ = cp;
		*cp++ = '<';
		*cp++ = crs/100+'0';
		*cp++ = (crs%100)/10+'0';
		*cp++ = crs%10+'0';
		*cp++ = '>';
		*cp++ = '\0';
		if(crs++ >= 220) {
			yyerror("program too big");
			getout();
		}
		bstack[bindx++] = lev++;
	}

def:
	_DEFINE LETTER '('
	{
		$$ = getf($2);
		pre = (char*)"";
		post = (char*)"";
		lev = 1;
		bindx = 0;
		bstack[bindx] = 0;
	}

dargs:
|	lora
	{
		pp((char*)$1);
	}
|	dargs ',' lora
	{
		pp((char*)$3);
	}

dlets:
	lora
	{
		tp((char*)$1);
	}
|	dlets ',' lora
	{
		tp((char*)$3);
	}

lora:
	LETTER
	{
		$<cptr>$=$1;
	}
|	LETTER '[' ']'
	{
		$$ = geta($1);
	}

%%

int
yylex(void)
{
	int c, ch;

restart:
	c = getch();
	peekc = -1;
	while(c == ' ' || c == '\t')
		c = getch();
	if(c == '\\') {
		getch();
		goto restart;
	}
	if(c >= 'a' && c <= 'z') {
		/* look ahead to look for reserved words */
		peekc = getch();
		if(peekc >= 'a' && peekc <= 'z') { /* must be reserved word */
			if(c=='p' && peekc=='r') {
				c = _PRINT;
				goto skip;
			}
			if(c=='i' && peekc=='f') {
				c = _IF;
				goto skip;
			}
			if(c=='w' && peekc=='h') {
				c = _WHILE;
				goto skip;
			}
			if(c=='f' && peekc=='o') {
				c = _FOR;
				goto skip;
			}
			if(c=='s' && peekc=='q') {
				c = SQRT;
				goto skip;
			}
			if(c=='r' && peekc=='e') {
				c = _RETURN;
				goto skip;
			}
			if(c=='b' && peekc=='r') {
				c = _BREAK;
				goto skip;
			}
			if(c=='d' && peekc=='e') {
				c = _DEFINE;
				goto skip;
			}
			if(c=='s' && peekc=='c') {
				c = SCALE;
				goto skip;
			}
			if(c=='b' && peekc=='a') {
				c = BASE;
				goto skip;
			}
			if(c=='i' && peekc=='b') {
				c = BASE;
				goto skip;
			}
			if(c=='o' && peekc=='b') {
				c = OBASE;
				goto skip;
			}
			if(c=='d' && peekc=='i') {
				c = FFF;
				goto skip;
			}
			if(c=='a' && peekc=='u') {
				c = _AUTO;
				goto skip;
			}
			if(c=='l' && peekc=='e') {
				c = LENGTH;
				goto skip;
			}
			if(c=='q' && peekc=='u')
				getout();
			/* could not be found */
			return ERROR;

		skip:	/* skip over rest of word */
			peekc = -1;
			for(;;) {
				ch = getch();
				if(ch < 'a' || ch > 'z')
					break;
			}
			peekc = ch;
			return c;
		}

		/* usual case; just one single letter */
		yylval.cptr = letr[c-'a'];
		return LETTER;
	}
	if((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
		yylval.cc = c;
		return DIGIT;
	}
	switch(c) {
	case '.':
		return DOT;
	case '*':
		yylval.cptr = "*";
		return cpeek('=', EQOP, c);
	case '%':
		yylval.cptr = "%%";
		return cpeek('=', EQOP, c);
	case '^':
		yylval.cptr = "^";
		return cpeek('=', EQOP, c);
	case '+':
		ch = cpeek('=', EQOP, c);
		if(ch == EQOP) {
			yylval.cptr = "+";
			return ch;
		}
		return cpeek('+', INCR, c);
	case '-':
		ch = cpeek('=', EQOP, c);
		if(ch == EQOP) {
			yylval.cptr = "-";
			return ch;
		}
		return cpeek('-', DECR, c);
	case '=':
		return cpeek('=', EQ, '=');
	case '<':
		return cpeek('=', LE, '<');
	case '>':
		return cpeek('=', GE, '>');
	case '!':
		return cpeek('=', NE, '!');
	case '/':
		ch = cpeek('=', EQOP, c);
		if(ch == EQOP) {
			yylval.cptr = "/";
			return ch;
		}
		if(peekc == '*') {
			peekc = -1;
			for(;;) {
				ch = getch();
				if(ch == '*') {
					peekc = getch();
					if(peekc == '/') {
						peekc = -1;
						goto restart;
					}
				}
			}
		}
		return c;
	case '"':
		yylval.cptr = str;
		while((c=getch()) != '"'){
			*str++ = c;
			if(str >= &string[999]){
				yyerror("string space exceeded");
				getout();
			}
		}
		*str++ = 0;
		return QSTR;
	default:
		return c;
	}
}

int
cpeek(int c, int yes, int no)
{

	peekc = getch();
	if(peekc == c) {
		peekc = -1;
		return yes;
	}
	return no;
}

int
getch(void)
{
	long ch;

loop:
	ch = peekc;
	if(ch < 0){
		if(in == 0)
			ch = -1;
		else
			ch = Bgetc(in);
	}
	peekc = -1;
	if(ch >= 0)
		return ch;

	ifile++;
	if(ifile >= sargc) {
		if(ifile >= sargc+1)
			getout();
		in = &bstdin;
		Binit(in, 0, OREAD);
		ln = 0;
		goto loop;
	}
	if(in)
		Bterm(in);
	if((in = Bopen(sargv[ifile], OREAD)) != 0){
		ln = 0;
		ss = sargv[ifile];
		goto loop;
	}
	fprint(2, "open %s: %r\n", sargv[ifile]);
	yyerror("cannot open input file");
	return 0;		/* shut up ken */
}

char*
bundle(int a, ...)
{
	int i;
	char **q;
	va_list arg;
	
	i = a;
	va_start(arg, a);
	q = bsp_nxt;
	if(bdebug)
		fprint(2, "bundle %d elements at %lx\n", i, q);
	while(i-- > 0) {
		if(bsp_nxt >= &bspace[bsp_max])
			yyerror("bundling space exceeded");
		*bsp_nxt++ = va_arg(arg, char*);
	}
	*bsp_nxt++ = 0;
	va_end(arg);
	yyval.cptr = (char*)q;
	return (char*)q;
}

void
routput(char *p)
{
	char **pp;
	
	if(bdebug)
		fprint(2, "routput(%lx)\n", p);
	if((char**)p >= &bspace[0] && (char**)p < &bspace[bsp_max]) {
		/* part of a bundle */
		pp = (char**)p;
		while(*pp != 0)
			routput(*pp++);
	} else
		Bprint(&bstdout, p);	/* character string */
}

void
output(char *p)
{
	routput(p);
	bsp_nxt = &bspace[0];
	Bprint(&bstdout, "\n");
	Bflush(&bstdout);
	cp = cary;
	crs = rcrs;
}

void
conout(char *p, char *s)
{
	Bprint(&bstdout, "[");
	routput(p);
	Bprint(&bstdout, "]s%s\n", s);
	Bflush(&bstdout);
	lev--;
}

void
yyerror(char *s, ...)
{
	if(ifile > sargc)
		ss = "teletype";
	Bprint(&bstdout, "c[%s:%d, %s]pc\n", s, ln+1, ss);
	Bflush(&bstdout);
	cp = cary;
	crs = rcrs;
	bindx = 0;
	lev = 0;
	bsp_nxt = &bspace[0];
}

void
pp(char *s)
{
	/* puts the relevant stuff on pre and post for the letter s */
	bundle(3, "S", s, pre);
	pre = yyval.cptr;
	bundle(4, post, "L", s, "s.");
	post = yyval.cptr;
}

void
tp(char *s)
{
	/* same as pp, but for temps */
	bundle(3, "0S", s, pre);
	pre = yyval.cptr;
	bundle(4, post, "L", s, "s.");
	post = yyval.cptr;
}

void
yyinit(int argc, char **argv)
{
	Binit(&bstdout, 1, OWRITE);
	sargv = argv;
	sargc = argc;
	if(sargc == 0) {
		in = &bstdin;
		Binit(in, 0, OREAD);
	} else if((in = Bopen(sargv[0], OREAD)) == 0)
		yyerror("cannot open input file");
	ifile = 0;
	ln = 0;
	ss = sargv[0];
}

void
getout(void)
{
	Bprint(&bstdout, "q");
	Bflush(&bstdout);
	exits(0);
}

char*
getf(char *p)
{
	return funtab[*p - 'a'];
}

char*
geta(char *p)
{
	return atab[*p - 'a'];
}

void
main(int argc, char **argv)
{
	int p[2];

	ARGBEGIN{
	case 'd':
		bdebug++;
		break;
	case 'c':
		cflag++;
		break;
	case 'l':
		lflag++;
		break;
	case 's':
		sflag++;
		break;
	default:
		fprint(2, "Usage: bc [-l] [-c] [file ...]\n");
		exits("usage");
	}ARGEND
	
	if(lflag) {
		argc++;
		argv--;
		*argv = unsharp("#9/lib/bclib");
	}
	if(cflag) {
		yyinit(argc, argv);
		for(;;)
			yyparse();
		exits(0);
	}
	pipe(p);
	if(fork() == 0) {
		dup(p[1], 1);
		close(p[0]);
		close(p[1]);
		yyinit(argc, argv);
		for(;;)
			yyparse();
	}
	dup(p[0], 0);
	close(p[0]);
	close(p[1]);
	execl(unsharp("#9/bin/dc"), "dc", nil);
}
