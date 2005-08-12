#define	IF	1
#define	ELSE	2
#define	CASE	3
#define TYPE	4
#define DO	5
#define STRUCT	6
#define OTHER	7

#define ALWAYS	01
#define	NEVER	02
#define	SOMETIMES	04

#define YES	1
#define NO	0

#define	KEYWORD	1
#define	DATADEF	2
#define	SINIT	3

#define CLEVEL	200
#define IFLEVEL	100
#define DOLEVEL	100
#define OPLENGTH	100
#define LINE	2048
#define LINELENG	2048
#define MAXTABS	8
#define TABLENG	8
#define TEMP	20480

#define OUT	outs(clev->tabs); Bputc(output, '\n');opflag = lbegin = 1; count = 0
#define OUTK	OUT; keyflag = 0;
#define BUMP	clev->tabs++; clev->pdepth++
#define UNBUMP	clev->tabs -= clev->pdepth; clev->pdepth = 0
#define eatspace()	while((cc=getch()) == ' ' || cc == '\t'); unget(cc)
#define eatallsp()	while((cc=getch()) == ' ' || cc == '\t' || cc == '\n'); unget(cc)

struct indent {		/* one for each level of { } */
	int tabs;
	int pdepth;
	int iflev;
	int ifc[IFLEVEL];
	int spdepth[IFLEVEL];
} ind[CLEVEL];
struct indent *clev = ind;
struct keyw {
	char	*name;
	char	punc;
	char	type;
} key[] = {
	"switch", ' ', OTHER,
	"do", ' ', DO,
	"while", ' ', OTHER,
	"if", ' ', IF,
	"for", ' ', OTHER,
	"else", ' ', ELSE,
	"case", ' ', CASE,
	"default", ' ', CASE,
	"char", '\t', TYPE,
	"int", '\t', TYPE,
	"short", '\t', TYPE,
	"long", '\t', TYPE,
	"unsigned", '\t', TYPE,
	"float", '\t', TYPE,
	"double", '\t', TYPE,
	"struct", ' ', STRUCT,
	"union", ' ', STRUCT,
	"enum", ' ', STRUCT,
	"extern", ' ', TYPE,
	"register", ' ', TYPE,
	"static", ' ', TYPE,
	"typedef", ' ', TYPE,
	0, 0, 0
};
struct op {
	char	*name;
	char	blanks;
	char	setop;
} op[] = {
	"+=", 	ALWAYS,  YES,
	"-=", 	ALWAYS,  YES,
	"*=", 	ALWAYS,  YES,
	"/=", 	ALWAYS,  YES,
	"%=", 	ALWAYS,  YES,
	">>=", 	ALWAYS,  YES,
	"<<=", 	ALWAYS,  YES,
	"&=", 	ALWAYS,  YES,
	"^=", 	ALWAYS,  YES,
	"|=", 	ALWAYS,  YES,
	">>", 	ALWAYS,  YES,
	"<<", 	ALWAYS,  YES,
	"<=", 	ALWAYS,  YES,
	">=", 	ALWAYS,  YES,
	"==", 	ALWAYS,  YES,
	"!=", 	ALWAYS,  YES,
	"=", 	ALWAYS,  YES,
	"&&", 	ALWAYS, YES,
	"||", 	ALWAYS, YES,
	"++", 	NEVER, NO,
	"--", 	NEVER, NO,
	"->", 	NEVER, NO,
	"<", 	ALWAYS, YES,
	">", 	ALWAYS, YES,
	"+", 	ALWAYS, YES,
	"/", 	ALWAYS, YES,
	"%", 	ALWAYS, YES,
	"^", 	ALWAYS, YES,
	"|", 	ALWAYS, YES,
	"!", 	NEVER, YES,
	"~", 	NEVER, YES,
	"*", 	SOMETIMES, YES,
	"&", 	SOMETIMES, YES,
	"-", 	SOMETIMES, YES,
	"?",	ALWAYS,YES,
	":",	ALWAYS,YES,
	0, 	0,0
};
Biobuf *input;
Biobuf *output;
int	strict = 0;
int	join	= 0;
int	opflag = 1;
int	keyflag = 0;
int	paren	 = 0;
int	split	 = 0;
int	folded	= 0;
int	dolevel	=0;
int	dotabs[DOLEVEL];
int	docurly[DOLEVEL];
int	dopdepth[DOLEVEL];
int	structlev = 0;
int	question	 = 0;
char	string[LINE];
char	*lastlook;
char	*p = string;
char temp[TEMP];
char *tp;
int err = 0;
char *lastplace = temp;
char *tptr = temp;
int maxleng	= LINELENG;
int maxtabs	= MAXTABS;
int count	= 0;
char next = '\0';
int	inswitch	=0;
int	lbegin	 = 1;
int lineno	= 0;

void work(void);
void gotif(void);
void gotelse(void);
int checkif(char *);
void gotdo(void);
void resetdo(void);
void gottype(struct keyw *lptr);
void gotstruct(void);
void gotop(int);
void keep(struct op *);
int getnl(void);
void ptabs(int);
void outs(int);
void putch(char, int);
struct keyw *lookup(char *, char *);
int comment(int);
void putspace(char, int);
int getch(void);
void unget(char);
char *getnext(int);
void copy(char *);
void clearif(struct indent *);
char puttmp(char, int);
void error(char *);
int cpp_comment(int);
