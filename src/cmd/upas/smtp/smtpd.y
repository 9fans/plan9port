%{
#include "common.h"
#include <ctype.h>
#include "smtpd.h"

#define YYSTYPE yystype
typedef struct quux yystype;
struct quux {
	String	*s;
	int	c;
};
Biobuf *yyfp;
YYSTYPE *bang;
extern Biobuf bin;
extern int debug;

YYSTYPE cat(YYSTYPE*, YYSTYPE*, YYSTYPE*, YYSTYPE*, YYSTYPE*, YYSTYPE*, YYSTYPE*);
int yyparse(void);
int yylex(void);
YYSTYPE anonymous(void);
%}

%term SPACE
%term CNTRL
%term CRLF
%start conversation
%%

conversation	: cmd
		| conversation cmd
		;
cmd		: error
		| 'h' 'e' 'l' 'o' spaces sdomain CRLF
			{ hello($6.s, 0); }
		| 'e' 'h' 'l' 'o' spaces sdomain CRLF
			{ hello($6.s, 1); }
		| 'm' 'a' 'i' 'l' spaces 'f' 'r' 'o' 'm' ':' spath CRLF
			{ sender($11.s); }
		| 'm' 'a' 'i' 'l' spaces 'f' 'r' 'o' 'm' ':' spath spaces 'a' 'u' 't' 'h' '=' sauth CRLF
			{ sender($11.s); }
		| 'r' 'c' 'p' 't' spaces 't' 'o' ':' spath CRLF
			{ receiver($9.s); }
		| 'd' 'a' 't' 'a' CRLF
			{ data(); }
		| 'r' 's' 'e' 't' CRLF
			{ reset(); }
		| 's' 'e' 'n' 'd' spaces 'f' 'r' 'o' 'm' ':' spath CRLF
			{ sender($11.s); }
		| 's' 'o' 'm' 'l' spaces 'f' 'r' 'o' 'm'  ':' spath CRLF
			{ sender($11.s); }
		| 's' 'a' 'm' 'l' spaces 'f' 'r' 'o' 'm' ':' spath CRLF
			{ sender($11.s); }
		| 'v' 'r' 'f' 'y' spaces string CRLF
			{ verify($6.s); }
		| 'e' 'x' 'p' 'n' spaces string CRLF
			{ verify($6.s); }
		| 'h' 'e' 'l' 'p' CRLF
			{ help(0); }
		| 'h' 'e' 'l' 'p' spaces string CRLF
			{ help($6.s); }
		| 'n' 'o' 'o' 'p' CRLF
			{ noop(); }
		| 'q' 'u' 'i' 't' CRLF
			{ quit(); }
		| 't' 'u' 'r' 'n' CRLF
			{ turn(); }
		| 's' 't' 'a' 'r' 't' 't' 'l' 's' CRLF
			{ starttls(); }
		| 'a' 'u' 't' 'h' spaces name spaces string CRLF
			{ auth($6.s, $8.s); }
		| 'a' 'u' 't' 'h' spaces name CRLF
			{ auth($6.s, nil); }
		| CRLF
			{ reply("501 illegal command or bad syntax\r\n"); }
		;
path		: '<' '>'			={ $$ = anonymous(); }
		| '<' mailbox '>'		={ $$ = $2; }
		| '<' a_d_l ':' mailbox '>'	={ $$ = cat(&$2, bang, &$4, 0, 0 ,0, 0); }
		;
spath		: path			={ $$ = $1; }
		| spaces path		={ $$ = $2; }
		;
auth		: path			={ $$ = $1; }
		| mailbox		={ $$ = $1; }
		;
sauth		: auth			={ $$ = $1; }
		| spaces auth		={ $$ = $2; }
		;
		;
a_d_l		: at_domain		={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| at_domain ',' a_d_l	={ $$ = cat(&$1, bang, &$3, 0, 0, 0, 0); }
		;
at_domain	: '@' domain		={ $$ = cat(&$2, 0, 0, 0, 0 ,0, 0); }
		;
sdomain		: domain		={ $$ = $1; }
		| domain spaces		={ $$ = $1; }
		;
domain		: element		={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| element '.'		={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| element '.' domain	={ $$ = cat(&$1, &$2, &$3, 0, 0 ,0, 0); }
		;
element		: name			={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| '#' number		={ $$ = cat(&$1, &$2, 0, 0, 0 ,0, 0); }
		| '[' ']'		={ $$ = cat(&$1, &$2, 0, 0, 0 ,0, 0); }
		| '[' dotnum ']'	={ $$ = cat(&$1, &$2, &$3, 0, 0 ,0, 0); }
		;
mailbox		: local_part		={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| local_part '@' domain	={ $$ = cat(&$3, bang, &$1, 0, 0 ,0, 0); }
		;
local_part	: dot_string		={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| quoted_string		={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		;
name		: let_dig			={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| let_dig ld_str		={ $$ = cat(&$1, &$2, 0, 0, 0 ,0, 0); }
		| let_dig ldh_str ld_str	={ $$ = cat(&$1, &$2, &$3, 0, 0 ,0, 0); }
		;
ld_str		: let_dig
		| let_dig ld_str		={ $$ = cat(&$1, &$2, 0, 0, 0 ,0, 0); }
		;
ldh_str		: hunder
		| ld_str hunder		={ $$ = cat(&$1, &$2, 0, 0, 0 ,0, 0); }
		| ldh_str ld_str hunder	={ $$ = cat(&$1, &$2, &$3, 0, 0 ,0, 0); }
		;
let_dig		: a
		| d
		;
dot_string	: string			={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| string '.' dot_string		={ $$ = cat(&$1, &$2, &$3, 0, 0 ,0, 0); }
		;

string		: char	={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| string char	={ $$ = cat(&$1, &$2, 0, 0, 0 ,0, 0); }
		;

quoted_string	: '"' qtext '"'	={ $$ = cat(&$1, &$2, &$3, 0, 0 ,0, 0); }
		;
qtext		: '\\' x		={ $$ = cat(&$2, 0, 0, 0, 0 ,0, 0); }
		| qtext '\\' x		={ $$ = cat(&$1, &$3, 0, 0, 0 ,0, 0); }
		| q
		| qtext q		={ $$ = cat(&$1, &$2, 0, 0, 0 ,0, 0); }
		;
char		: c
		| '\\' x		={ $$ = $2; }
		;
dotnum		: snum '.' snum '.' snum '.' snum ={ $$ = cat(&$1, &$2, &$3, &$4, &$5, &$6, &$7); }
		;
number		: d		={ $$ = cat(&$1, 0, 0, 0, 0 ,0, 0); }
		| number d	={ $$ = cat(&$1, &$2, 0, 0, 0 ,0, 0); }
		;
snum		: number		={ if(atoi(s_to_c($1.s)) > 255) print("bad snum\n"); } 
		;
spaces		: SPACE		={ $$ = $1; }
		| SPACE	spaces	={ $$ = $1; }
		;
hunder		: '-' | '_'
		;
special1	: CNTRL
		| '(' | ')' | ',' | '.'
		| ':' | ';' | '<' | '>' | '@'
		;
special		: special1 | '\\' | '"'
		;
notspecial	: '!' | '#' | '$' | '%' | '&' | '\''
		| '*' | '+' | '-' | '/'
		| '=' | '?'
		| '[' | ']' | '^' | '_' | '`' | '{' | '|' | '}' | '~'
		;

a		: 'a' | 'b' | 'c' | 'd' | 'e' | 'f' | 'g' | 'h' | 'i'
		| 'j' | 'k' | 'l' | 'm' | 'n' | 'o' | 'p' | 'q' | 'r'
		| 's' | 't' | 'u' | 'v' | 'w' | 'x' | 'y' | 'z'
		;
d		: '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
		;
c		: a | d | notspecial
		;		
q		: a | d | special1 | notspecial | SPACE
		;
x		: a | d | special | notspecial | SPACE
		;
%%

void
parseinit(void)
{
	bang = (YYSTYPE*)malloc(sizeof(YYSTYPE));
	bang->c = '!';
	bang->s = 0;
	yyfp = &bin;
}

int
yylex(void)
{
	int c;

	for(;;){
		c = Bgetc(yyfp);
		if(c == -1)
			return 0;
		if(debug)
			fprint(2, "%c", c);
		yylval.c = c = c & 0x7F;
		if(c == '\n'){
			return CRLF;
		}
		if(c == '\r'){
			c = Bgetc(yyfp);
			if(c != '\n'){
				Bungetc(yyfp);
				c = '\r';
			} else {
				if(debug)
					fprint(2, "%c", c);
				return CRLF;
			}
		}
		if(isalpha(c))
			return tolower(c);
		if(isspace(c))
			return SPACE;
		if(iscntrl(c))
			return CNTRL;
		return c;
	}
}

YYSTYPE
cat(YYSTYPE *y1, YYSTYPE *y2, YYSTYPE *y3, YYSTYPE *y4, YYSTYPE *y5, YYSTYPE *y6, YYSTYPE *y7)
{
	YYSTYPE rv;

	memset(&rv, 0, sizeof rv);
	if(y1->s)
		rv.s = y1->s;
	else {
		rv.s = s_new();
		s_putc(rv.s, y1->c);
		s_terminate(rv.s);
	}
	if(y2){
		if(y2->s){
			s_append(rv.s, s_to_c(y2->s));
			s_free(y2->s);
		} else {
			s_putc(rv.s, y2->c);
			s_terminate(rv.s);
		}
	} else
		return rv;
	if(y3){
		if(y3->s){
			s_append(rv.s, s_to_c(y3->s));
			s_free(y3->s);
		} else {
			s_putc(rv.s, y3->c);
			s_terminate(rv.s);
		}
	} else
		return rv;
	if(y4){
		if(y4->s){
			s_append(rv.s, s_to_c(y4->s));
			s_free(y4->s);
		} else {
			s_putc(rv.s, y4->c);
			s_terminate(rv.s);
		}
	} else
		return rv;
	if(y5){
		if(y5->s){
			s_append(rv.s, s_to_c(y5->s));
			s_free(y5->s);
		} else {
			s_putc(rv.s, y5->c);
			s_terminate(rv.s);
		}
	} else
		return rv;
	if(y6){
		if(y6->s){
			s_append(rv.s, s_to_c(y6->s));
			s_free(y6->s);
		} else {
			s_putc(rv.s, y6->c);
			s_terminate(rv.s);
		}
	} else
		return rv;
	if(y7){
		if(y7->s){
			s_append(rv.s, s_to_c(y7->s));
			s_free(y7->s);
		} else {
			s_putc(rv.s, y7->c);
			s_terminate(rv.s);
		}
	} else
		return rv;
	return rv;
}

void
yyerror(char *x)
{
	USED(x);
}

/*
 *  an anonymous user
 */
YYSTYPE
anonymous(void)
{
	YYSTYPE rv;

	memset(&rv, 0, sizeof rv);
	rv.s = s_copy("/dev/null");
	return rv;
}
