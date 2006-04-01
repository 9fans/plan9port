
#line	2	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
#include "common.h"
#include "smtp.h"
#include <ctype.h>

char	*yylp;		/* next character to be lex'd */
int	yydone;		/* tell yylex to give up */
char	*yybuffer;	/* first parsed character */
char	*yyend;		/* end of buffer to be parsed */
Node	*root;
Field	*firstfield;
Field	*lastfield;
Node	*usender;
Node	*usys;
Node	*udate;
char	*startfield, *endfield;
int	originator;
int	destination;
int	date;
int	received;
int	messageid;
extern	int	yyerrflag;
#ifndef	YYMAXDEPTH
#define	YYMAXDEPTH	150
#endif
#ifndef	YYSTYPE
#define	YYSTYPE	int
#endif
YYSTYPE	yylval;
YYSTYPE	yyval;
#define	WORD	57346
#define	DATE	57347
#define	RESENT_DATE	57348
#define	RETURN_PATH	57349
#define	FROM	57350
#define	SENDER	57351
#define	REPLY_TO	57352
#define	RESENT_FROM	57353
#define	RESENT_SENDER	57354
#define	RESENT_REPLY_TO	57355
#define	SUBJECT	57356
#define	TO	57357
#define	CC	57358
#define	BCC	57359
#define	RESENT_TO	57360
#define	RESENT_CC	57361
#define	RESENT_BCC	57362
#define	REMOTE	57363
#define	PRECEDENCE	57364
#define	MIMEVERSION	57365
#define	CONTENTTYPE	57366
#define	MESSAGEID	57367
#define	RECEIVED	57368
#define	MAILER	57369
#define	BADTOKEN	57370
#define YYEOFCODE 1
#define YYERRCODE 2

#line	246	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"


/*
 *  Initialize the parsing.  Done once for each header field.
 */
void
yyinit(char *p, int len)
{
	yybuffer = p;
	yylp = p;
	yyend = p + len;
	firstfield = lastfield = 0;
	received = 0;
}

/*
 *  keywords identifying header fields we care about
 */
typedef struct Keyword	Keyword;
struct Keyword {
	char	*rep;
	int	val;
};

/* field names that we need to recognize */
Keyword key[] = {
	{ "date", DATE },
	{ "resent-date", RESENT_DATE },
	{ "return_path", RETURN_PATH },
	{ "from", FROM },
	{ "sender", SENDER },
	{ "reply-to", REPLY_TO },
	{ "resent-from", RESENT_FROM },
	{ "resent-sender", RESENT_SENDER },
	{ "resent-reply-to", RESENT_REPLY_TO },
	{ "to", TO },
	{ "cc", CC },
	{ "bcc", BCC },
	{ "resent-to", RESENT_TO },
	{ "resent-cc", RESENT_CC },
	{ "resent-bcc", RESENT_BCC },
	{ "remote", REMOTE },
	{ "subject", SUBJECT },
	{ "precedence", PRECEDENCE },
	{ "mime-version", MIMEVERSION },
	{ "content-type", CONTENTTYPE },
	{ "message-id", MESSAGEID },
	{ "received", RECEIVED },
	{ "mailer", MAILER },
	{ "who-the-hell-cares", WORD }
};

/*
 *  Lexical analysis for an rfc822 header field.  Continuation lines
 *  are handled in yywhite() when skipping over white space.
 *
 */
int
yylex(void)
{
	String *t;
	int quoting;
	int escaping;
	char *start;
	Keyword *kp;
	int c, d;

/*	print("lexing\n"); /**/
	if(yylp >= yyend)
		return 0;
	if(yydone)
		return 0;

	quoting = escaping = 0;
	start = yylp;
	yylval = malloc(sizeof(Node));
	yylval->white = yylval->s = 0;
	yylval->next = 0;
	yylval->addr = 0;
	yylval->start = yylp;
	for(t = 0; yylp < yyend; yylp++){
		c = *yylp & 0xff;

		/* dump nulls, they can't be in header */
		if(c == 0)
			continue;

		if(escaping) {
			escaping = 0;
		} else if(quoting) {
			switch(c){
			case '\\':
				escaping = 1;
				break;
			case '\n':
				d = (*(yylp+1))&0xff;
				if(d != ' ' && d != '\t'){
					quoting = 0;
					yylp--;
					continue;
				}
				break;
			case '"':
				quoting = 0;
				break;
			}
		} else {
			switch(c){
			case '\\':
				escaping = 1;
				break;
			case '(':
			case ' ':
			case '\t':
			case '\r':
				goto out;
			case '\n':
				if(yylp == start){
					yylp++;
/*					print("lex(c %c)\n", c); /**/
					yylval->end = yylp;
					return yylval->c = c;
				}
				goto out;
			case '@':
			case '>':
			case '<':
			case ':':
			case ',':
			case ';':
				if(yylp == start){
					yylp++;
					yylval->white = yywhite();
/*					print("lex(c %c)\n", c); /**/
					yylval->end = yylp;
					return yylval->c = c;
				}
				goto out;
			case '"':
				quoting = 1;
				break;
			default:
				break;
			}
		}
		if(t == 0)
			t = s_new();
		s_putc(t, c);
	}
out:
	yylval->white = yywhite();
	if(t) {
		s_terminate(t);
	} else				/* message begins with white-space! */
		return yylval->c = '\n';
	yylval->s = t;
	for(kp = key; kp->val != WORD; kp++)
		if(cistrcmp(s_to_c(t), kp->rep)==0)
			break;
/*	print("lex(%d) %s\n", kp->val-WORD, s_to_c(t)); /**/
	yylval->end = yylp;
	return yylval->c = kp->val;
}

void
yyerror(char *x)
{
	USED(x);

	/*fprint(2, "parse err: %s\n", x);/**/
}

/*
 *  parse white space and comments
 */
String *
yywhite(void)
{
	String *w;
	int clevel;
	int c;
	int escaping;

	escaping = clevel = 0;
	for(w = 0; yylp < yyend; yylp++){
		c = *yylp & 0xff;

		/* dump nulls, they can't be in header */
		if(c == 0)
			continue;

		if(escaping){
			escaping = 0;
		} else if(clevel) {
			switch(c){
			case '\n':
				/*
				 *  look for multiline fields
				 */
				if(*(yylp+1)==' ' || *(yylp+1)=='\t')
					break;
				else
					goto out;
			case '\\':
				escaping = 1;
				break;
			case '(':
				clevel++;
				break;
			case ')':
				clevel--;
				break;
			}
		} else {
			switch(c){
			case '\\':
				escaping = 1;
				break;
			case '(':
				clevel++;
				break;
			case ' ':
			case '\t':
			case '\r':
				break;
			case '\n':
				/*
				 *  look for multiline fields
				 */
				if(*(yylp+1)==' ' || *(yylp+1)=='\t')
					break;
				else
					goto out;
			default:
				goto out;
			}
		}
		if(w == 0)
			w = s_new();
		s_putc(w, c);
	}
out:
	if(w)
		s_terminate(w);
	return w;
}

/*
 *  link two parsed entries together
 */
Node*
link2(Node *p1, Node *p2)
{
	Node *p;

	for(p = p1; p->next; p = p->next)
		;
	p->next = p2;
	return p1;
}

/*
 *  link three parsed entries together
 */
Node*
link3(Node *p1, Node *p2, Node *p3)
{
	Node *p;

	for(p = p2; p->next; p = p->next)
		;
	p->next = p3;

	for(p = p1; p->next; p = p->next)
		;
	p->next = p2;

	return p1;
}

/*
 *  make a:b, move all white space after both
 */
Node*
colon(Node *p1, Node *p2)
{
	if(p1->white){
		if(p2->white)
			s_append(p1->white, s_to_c(p2->white));
	} else {
		p1->white = p2->white;
		p2->white = 0;
	}

	s_append(p1->s, ":");
	if(p2->s)
		s_append(p1->s, s_to_c(p2->s));

	if(p1->end < p2->end)
		p1->end = p2->end;
	freenode(p2);
	return p1;
}

/*
 *  concatenate two fields, move all white space after both
 */
Node*
concat(Node *p1, Node *p2)
{
	char buf[2];

	if(p1->white){
		if(p2->white)
			s_append(p1->white, s_to_c(p2->white));
	} else {
		p1->white = p2->white;
		p2->white = 0;
	}

	if(p1->s == nil){
		buf[0] = p1->c;
		buf[1] = 0;
		p1->s = s_new();
		s_append(p1->s, buf);
	}

	if(p2->s)
		s_append(p1->s, s_to_c(p2->s));
	else {
		buf[0] = p2->c;
		buf[1] = 0;
		s_append(p1->s, buf);
	}

	if(p1->end < p2->end)
		p1->end = p2->end;
	freenode(p2);
	return p1;
}

/*
 *  look for disallowed chars in the field name
 */
int
badfieldname(Node *p)
{
	for(; p; p = p->next){
		/* field name can't contain white space */
		if(p->white && p->next)
			return 1;
	}
	return 0;
}

/*
 *  mark as an address
 */
Node *
address(Node *p)
{
	p->addr = 1;
	return p;
}

/*
 *  case independent string compare
 */
int
cistrcmp(char *s1, char *s2)
{
	int c1, c2;

	for(; *s1; s1++, s2++){
		c1 = isupper(*s1) ? tolower(*s1) : *s1;
		c2 = isupper(*s2) ? tolower(*s2) : *s2;
		if (c1 != c2)
			return -1;
	}
	return *s2;
}

/*
 *  free a node
 */
void
freenode(Node *p)
{
	Node *tp;

	while(p){
		tp = p->next;
		if(p->s)
			s_free(p->s);
		if(p->white)
			s_free(p->white);
		free(p);
		p = tp;
	}
}


/*
 *  an anonymous user
 */
Node*
nobody(Node *p)
{
	if(p->s)
		s_free(p->s);
	p->s = s_copy("pOsTmAsTeR");
	p->addr = 1;
	return p;
}

/*
 *  add anything that was dropped because of a parse error
 */
void
missing(Node *p)
{
	Node *np;
	char *start, *end;
	Field *f;
	String *s;

	start = yybuffer;
	if(lastfield != nil){
		for(np = lastfield->node; np; np = np->next)
			start = np->end+1;
	}

	end = p->start-1;

	if(end <= start)
		return;

	if(strncmp(start, "From ", 5) == 0)
		return;

	np = malloc(sizeof(Node));
	np->start = start;
	np->end = end;
	np->white = nil;
	s = s_copy("BadHeader: ");
	np->s = s_nappend(s, start, end-start);
	np->next = nil;

	f = malloc(sizeof(Field));
	f->next = 0;
	f->node = np;
	f->source = 0;
	if(firstfield)
		lastfield->next = f;
	else
		firstfield = f;
	lastfield = f;
}

/*
 *  create a new field
 */
void
newfield(Node *p, int source)
{
	Field *f;

	missing(p);

	f = malloc(sizeof(Field));
	f->next = 0;
	f->node = p;
	f->source = source;
	if(firstfield)
		lastfield->next = f;
	else
		firstfield = f;
	lastfield = f;
	endfield = startfield;
	startfield = yylp;
}

/*
 *  fee a list of fields
 */
void
freefield(Field *f)
{
	Field *tf;

	while(f){
		tf = f->next;
		freenode(f->node);
		free(f);
		f = tf;
	}
}

/*
 *  add some white space to a node
 */
Node*
whiten(Node *p)
{
	Node *tp;

	for(tp = p; tp->next; tp = tp->next)
		;
	if(tp->white == 0)
		tp->white = s_copy(" ");
	return p;
}

void
yycleanup(void)
{
	Field *f, *fnext;
	Node *np, *next;

	for(f = firstfield; f; f = fnext){
		for(np = f->node; np; np = next){
			if(np->s)
				s_free(np->s);
			if(np->white)
				s_free(np->white);
			next = np->next;
			free(np);
		}
		fnext = f->next;
		free(f);
	}
	firstfield = lastfield = 0;
}
static	const	short	yyexca[] =
{-1, 1,
	1, -1,
	-2, 0,
-1, 47,
	1, 4,
	-2, 0,
-1, 112,
	29, 72,
	31, 72,
	32, 72,
	35, 72,
	-2, 74
};
#define	YYNPROD	122
#define	YYPRIVATE 57344
#define	YYLAST	608
static	const	short	yyact[] =
{
 112, 133, 136,  53, 121, 111, 134,  55, 109, 118,
 119, 116, 162, 171,  35,  48, 166,  54,   5, 166,
 179, 114, 115, 155,  49, 101, 100,  99,  95,  94,
  93,  92,  98,  91, 132,  90, 123,  89, 122,  88,
  87,  86,  85,  84,  83,  82,  97,  81,  80, 106,
  47,  46, 110, 117, 153, 168, 108,   2,  56,  57,
  58,  59,  60,  61,  62,  63,  64,  65,  73,  66,
  67,  68,  69,  70,  71,  72,  74,  75,  76,  77,
  78,  79, 124, 124,  49,  55, 177, 131, 110,  52,
 110, 110, 138, 137, 140, 141, 124, 124,  51, 120,
 124, 124, 124,  50, 102, 104, 135, 154,  31,  32,
 107, 157, 105,  14,  55,  55, 156,  13, 161, 117,
 117, 139, 158, 124, 142, 143, 144, 145, 146, 147,
 163, 164, 160,  12, 148, 149,  11, 157, 150, 151,
 152,  10, 156,   9,   8,   7,   3,   1,   0, 124,
 124, 124, 124, 124,   0, 169,   0,   0, 110, 165,
   0,   0, 170, 117,   0,   0,   0,   0, 173, 176,
 178,   0,   0,   0, 172,   0,   0,   0, 180,   0,
   0, 182, 183,   0,   0, 165, 165, 165, 165, 165,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0, 174,  56,  57,  58,  59,  60,  61,  62,
  63,  64,  65,  73,  66,  67,  68,  69,  70,  71,
  72,  74,  75,  76,  77,  78,  79,   0,   0, 128,
 130, 129, 125, 126, 127,  15,   0,  36,  16,  17,
  19, 103,  20,  18,  23,  22,  21,  30,  24,  26,
  28,  25,  27,  29,   0,  34,  37,  38,  39,  33,
  40,   0,   4,   0,  45,  44,  41,  42,  43,  56,
  57,  58,  59,  60,  61,  62,  63,  64,  65,  73,
  66,  67,  68,  69,  70,  71,  72,  74,  75,  76,
  77,  78,  79,   0,   0,  96,  45,  44,  41,  42,
  43,  15,   0,  36,  16,  17,  19,   6,  20,  18,
  23,  22,  21,  30,  24,  26,  28,  25,  27,  29,
   0,  34,  37,  38,  39,  33,  40,   0,   4,   0,
  45,  44,  41,  42,  43,  15,   0,  36,  16,  17,
  19, 103,  20,  18,  23,  22,  21,  30,  24,  26,
  28,  25,  27,  29,   0,  34,  37,  38,  39,  33,
  40,   0,   0,   0,  45,  44,  41,  42,  43,  56,
  57,  58,  59,  60,  61,  62,  63,  64,  65,  73,
  66,  67,  68,  69,  70,  71,  72,  74,  75,  76,
  77,  78,  79,   0,   0,   0,   0, 175, 113,   0,
  52,  56,  57,  58,  59,  60,  61,  62,  63,  64,
  65,  73,  66,  67,  68,  69,  70,  71,  72,  74,
  75,  76,  77,  78,  79,   0,   0,   0,   0,   0,
 113,   0,  52,  56,  57,  58,  59,  60,  61,  62,
  63,  64,  65,  73,  66,  67,  68,  69,  70,  71,
  72,  74,  75,  76,  77,  78,  79,   0,   0,   0,
   0,   0,   0, 159,  52,  56,  57,  58,  59,  60,
  61,  62,  63,  64,  65,  73,  66,  67,  68,  69,
  70,  71,  72,  74,  75,  76,  77,  78,  79,   0,
   0,   0,   0,   0,   0,   0,  52,  56,  57,  58,
  59,  60,  61,  62,  63,  64,  65,  73,  66,  67,
  68,  69,  70,  71,  72,  74,  75,  76,  77,  78,
  79,   0,   0, 167,   0,   0, 113,  56,  57,  58,
  59,  60,  61,  62,  63,  64,  65,  73,  66,  67,
  68,  69,  70,  71,  72,  74,  75,  76,  77,  78,
  79,   0,   0,   0,   0,   0, 113,  56,  57,  58,
  59,  60,  61,  62,  63,  64,  65,  73,  66,  67,
  68,  69,  70,  71,  72,  74,  75,  76,  77,  78,
  79,   0,   0, 181,  56,  57,  58,  59,  60,  61,
  62,  63,  64,  65,  73,  66,  67,  68,  69,  70,
  71,  72,  74,  75,  76,  77,  78,  79
};
static	const	short	yypact[] =
{
 299,-1000,-1000,  22,-1000,  21,  54,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,  19,  17,  15,  14,  13,
  12,  11,  10,   9,   7,   5,   3,   1,   0,  -1,
  -2, 265,  -3,  -4,  -5,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000, 233, 233, 580, 397,
  -9,-1000, 580, -26, -25,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
 333, 199, 199, 397, 461, 397, 397, 397, 397, 397,
 397, 397, 397, 397, 397, 199, 199,-1000,-1000, 199,
 199, 199,-1000,  -6,-1000,  33, 580,  -8,-1000,-1000,
 523,-1000,-1000, 429, 580, -23,-1000,-1000, 580, 580,
-1000,-1000, 199,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000, -15,-1000,-1000,-1000, 493,-1000,-1000, -15,
-1000,-1000, -15, -15, -15, -15, -15, -15, 199, 199,
 199, 199, 199,  47, 580, 397,-1000,-1000, -21,-1000,
 -25, -26, 580,-1000,-1000,-1000, 397, 365, 580, 580,
-1000,-1000,-1000,-1000, -12,-1000,-1000, 553,-1000,-1000,
 580, 580,-1000,-1000
};
static	const	short	yypgo[] =
{
   0, 147,  57, 146,  18, 145, 144, 143, 141, 136,
 133, 117, 113,   8, 112,   0,  34, 110,   6,   4,
  38, 109, 108,   1, 106,   2,   5, 103,  17,  98,
  11,   3,  36,  86,  14
};
static	const	short	yyr1[] =
{
   0,   1,   1,   2,   2,   2,   4,   4,   4,   4,
   4,   4,   4,   4,   4,   3,   6,   6,   6,   6,
   6,   6,   6,   5,   5,   7,   7,   7,   7,   7,
   7,   7,   7,   7,   7,   7,   7,   8,   8,  11,
  11,  12,  12,  10,  10,  21,  21,  21,  21,   9,
   9,  16,  16,  23,  23,  24,  24,  17,  17,  18,
  18,  18,  26,  26,  13,  13,  27,  27,  29,  29,
  28,  28,  31,  30,  25,  25,  20,  20,  32,  32,
  32,  32,  32,  32,  32,  19,  14,  33,  33,  15,
  15,  15,  15,  15,  15,  15,  15,  15,  15,  15,
  15,  15,  15,  15,  15,  15,  15,  15,  15,  15,
  15,  15,  15,  22,  22,  22,  22,  34,  34,  34,
  34,  34
};
static	const	short	yyr2[] =
{
   0,   1,   3,   1,   2,   3,   1,   1,   1,   1,
   1,   1,   1,   1,   3,   6,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   2,   3,   2,   3,   2,
   3,   2,   3,   2,   3,   2,   3,   3,   2,   3,
   2,   3,   2,   3,   2,   1,   1,   1,   1,   3,
   2,   1,   3,   1,   1,   4,   3,   1,   3,   1,
   2,   1,   3,   2,   3,   1,   2,   4,   1,   1,
   3,   3,   1,   1,   1,   2,   1,   2,   1,   1,
   1,   1,   1,   1,   1,   1,   6,   1,   3,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1,   2,   2,   1,   1,   1,
   1,   1
};
static	const	short	yychk[] =
{
-1000,  -1,  -2,  -3,  29,  -4,   8,  -5,  -6,  -7,
  -8,  -9, -10, -11, -12,   2,   5,   6,  10,   7,
   9,  13,  12,  11,  15,  18,  16,  19,  17,  20,
  14, -22, -21,  26,  22, -34,   4,  23,  24,  25,
  27,  33,  34,  35,  32,  31,  29,  29, -13,  30,
 -27, -29,  35, -31, -28, -15,   4,   5,   6,   7,
   8,   9,  10,  11,  12,  13,  15,  16,  17,  18,
  19,  20,  21,  14,  22,  23,  24,  25,  26,  27,
  29,  30,  30,  30,  30,  30,  30,  30,  30,  30,
  30,  30,  30,  30,  30,  30,  30, -34, -15,  30,
  30,  30,  -2,   8,  -2, -14, -15, -17, -18, -13,
 -25, -26, -15,  33,  30,  31, -30, -15,  35,  35,
  -4, -19, -20, -32, -15,  33,  34,  35,  30,  32,
  31, -19, -16, -23, -18, -24, -25, -13, -18, -16,
 -18, -18, -16, -16, -16, -16, -16, -16, -20, -20,
 -20, -20, -20,  21, -15,  31, -26, -15, -13,  34,
 -28, -31,  35, -30, -30, -32,  31,  30,   8, -15,
 -18,  34, -30, -23, -16,  32, -15, -33, -15,  32,
 -15,  30, -15, -15
};
static	const	short	yydef[] =
{
   0,  -2,   1,   0,   3,   0,   0,   6,   7,   8,
   9,  10,  11,  12,  13,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0, 113, 114,  45,  46,  47,
  48, 117, 118, 119, 120, 121,   0,  -2,   0,   0,
   0,  65,   0,  68,  69,  72,  89,  90,  91,  92,
  93,  94,  95,  96,  97,  98,  99, 100, 101, 102,
 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
   0,   0,   0,   0,   0,   0,   0,   0,   0,  25,
  27,  29,  31,  33,  35,  38,  50, 115, 116,  44,
  40,  42,   2,   0,   5,   0,   0,  18,  57,  59,
   0,  61,  -2,   0,   0,   0,  66,  73,   0,   0,
  14,  23,  85,  76,  78,  79,  80,  81,  82,  83,
  84,  24,  16,  51,  53,  54,   0,  17,  19,  20,
  21,  22,  26,  28,  30,  32,  34,  36,  37,  49,
  43,  39,  41,   0,   0,   0,  60,  75,   0,  63,
  64,   0,   0,  70,  71,  77,   0,   0,   0,   0,
  58,  62,  67,  52,   0,  56,  15,   0,  87,  55,
   0,   0,  86,  88
};
static	const	short	yytok1[] =
{
   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  29,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,  31,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,  30,  32,
  33,   0,  34,   0,  35
};
static	const	short	yytok2[] =
{
   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,
  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,
  22,  23,  24,  25,  26,  27,  28
};
static	const	long	yytok3[] =
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
static	const	char*	yytoknames[1];		/* for debugging */
static	const	char*	yystates[1];		/* for debugging */
#endif

/*	parser for yacc output	*/
#ifdef YYARG
#define	yynerrs		yyarg->yynerrs
#define	yyerrflag	yyarg->yyerrflag
#define yyval		yyarg->yyval
#define yylval		yyarg->yylval
#else
int	yynerrs = 0;		/* number of errors */
int	yyerrflag = 0;		/* error recovery flag */
#endif

extern	int	fprint(int, char*, ...);
extern	int	sprint(char*, char*, ...);

static const char*
yytokname(int yyc)
{
	static char x[10];

	if(yyc > 0 && yyc <= sizeof(yytoknames)/sizeof(yytoknames[0]))
	if(yytoknames[yyc-1])
		return yytoknames[yyc-1];
	sprint(x, "<%d>", yyc);
	return x;
}

static const char*
yystatname(int yys)
{
	static char x[10];

	if(yys >= 0 && yys < sizeof(yystates)/sizeof(yystates[0]))
	if(yystates[yys])
		return yystates[yys];
	sprint(x, "<%d>\n", yys);
	return x;
}

static long
#ifdef YYARG
yylex1(struct Yyarg *yyarg)
#else
yylex1(void)
#endif
{
	long yychar;
	const long *t3p;
	int c;

#ifdef YYARG	
	yychar = yylex(yyarg);
#else
	yychar = yylex();
#endif
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
		fprint(2, "lex %.4lux %s\n", yychar, yytokname(c));
	return c;
}

int
#ifdef YYARG
yyparse(struct Yyarg *yyarg)
#else
yyparse(void)
#endif
{
	struct
	{
		YYSTYPE	yyv;
		int	yys;
	} yys[YYMAXDEPTH], *yyp, *yypt;
	const short *yyxi;
	int yyj, yym, yystate, yyn, yyg;
	long yychar;
#ifndef YYARG
	YYSTYPE save1, save2;
	int save3, save4;

	save1 = yylval;
	save2 = yyval;
	save3 = yynerrs;
	save4 = yyerrflag;
#endif

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
#ifndef YYARG
	yylval = save1;
	yyval = save2;
	yynerrs = save3;
	yyerrflag = save4;
#endif
	return yyn;

yystack:
	/* put a state and value onto the stack */
	if(yydebug >= 4)
		fprint(2, "char %s in %s", yytokname(yychar), yystatname(yystate));

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
#ifdef YYARG
		yychar = yylex1(yyarg);
#else
		yychar = yylex1();
#endif
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
#ifdef YYARG
		yychar = yylex1(yyarg);
#else
		yychar = yylex1();
#endif

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
				fprint(2, "%s", yystatname(yystate));
				fprint(2, "saw %s\n", yytokname(yychar));
			}
			goto yyerrlab;
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
					fprint(2, "error recovery pops state %d, uncovers %d\n",
						yyp->yys, (yyp-1)->yys );
				yyp--;
			}
			/* there is no state on the stack with an error shift ... abort */
			goto ret1;

		case 3:  /* no shift yet; clobber input char */
			if(yydebug >= 2)
				fprint(2, "error recovery discards %s\n", yytokname(yychar));
			if(yychar == YYEOFCODE)
				goto ret1;
			yychar = -1;
			goto yynewstate;   /* try again in the same state */
		}
	}

	/* reduction by production yyn */
	if(yydebug >= 2)
		fprint(2, "reduce %d in:\n\t%s", yyn, yystatname(yystate));

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
		
case 3:
#line	56	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yydone = 1; } break;
case 6:
#line	61	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ date = 1; } break;
case 7:
#line	63	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ originator = 1; } break;
case 8:
#line	65	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ destination = 1; } break;
case 15:
#line	74	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ freenode(yypt[-5].yyv); freenode(yypt[-2].yyv); freenode(yypt[-1].yyv);
			  usender = yypt[-4].yyv; udate = yypt[-3].yyv; usys = yypt[-0].yyv;
			} break;
case 16:
#line	79	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 1); } break;
case 17:
#line	81	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 1); } break;
case 18:
#line	83	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 1); } break;
case 19:
#line	85	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 1); } break;
case 20:
#line	87	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 1); } break;
case 21:
#line	89	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 1); } break;
case 22:
#line	91	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 1); } break;
case 23:
#line	94	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 24:
#line	96	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 25:
#line	99	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 26:
#line	101	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 27:
#line	103	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 28:
#line	105	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 29:
#line	107	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 30:
#line	109	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 31:
#line	111	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 32:
#line	113	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 33:
#line	115	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 34:
#line	117	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 35:
#line	119	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 36:
#line	121	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 37:
#line	124	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 38:
#line	126	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 39:
#line	129	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); received++; } break;
case 40:
#line	131	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); received++; } break;
case 41:
#line	134	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 42:
#line	136	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 43:
#line	139	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 44:
#line	141	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0); } break;
case 47:
#line	143	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ messageid = 1; } break;
case 49:
#line	146	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ /* hack to allow same lex for field names and the rest */
			 if(badfieldname(yypt[-2].yyv)){
				freenode(yypt[-2].yyv);
				freenode(yypt[-1].yyv);
				freenode(yypt[-0].yyv);
				return 1;
			 }
			 newfield(link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv), 0);
			} break;
case 50:
#line	156	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ /* hack to allow same lex for field names and the rest */
			 if(badfieldname(yypt[-1].yyv)){
				freenode(yypt[-1].yyv);
				freenode(yypt[-0].yyv);
				return 1;
			 }
			 newfield(link2(yypt[-1].yyv, yypt[-0].yyv), 0);
			} break;
case 52:
#line	167	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv); } break;
case 55:
#line	173	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link2(yypt[-3].yyv, link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv)); } break;
case 56:
#line	175	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv); } break;
case 58:
#line	179	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv); } break;
case 60:
#line	183	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link2(yypt[-1].yyv, yypt[-0].yyv); } break;
case 62:
#line	187	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv); } break;
case 63:
#line	189	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = nobody(yypt[-0].yyv); freenode(yypt[-1].yyv); } break;
case 64:
#line	192	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = address(concat(yypt[-2].yyv, concat(yypt[-1].yyv, yypt[-0].yyv))); } break;
case 66:
#line	196	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = concat(yypt[-1].yyv, yypt[-0].yyv); } break;
case 67:
#line	198	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = concat(yypt[-3].yyv, concat(yypt[-2].yyv, concat(yypt[-1].yyv, yypt[-0].yyv))); } break;
case 68:
#line	201	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = address(yypt[-0].yyv); } break;
case 70:
#line	205	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = address(concat(yypt[-2].yyv, concat(yypt[-1].yyv, yypt[-0].yyv)));} break;
case 71:
#line	207	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = address(concat(yypt[-2].yyv, concat(yypt[-1].yyv, yypt[-0].yyv)));} break;
case 75:
#line	215	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link2(yypt[-1].yyv, yypt[-0].yyv); } break;
case 77:
#line	219	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link2(yypt[-1].yyv, yypt[-0].yyv); } break;
case 86:
#line	226	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link3(yypt[-5].yyv, yypt[-3].yyv, link3(yypt[-4].yyv, yypt[-0].yyv, link2(yypt[-2].yyv, yypt[-1].yyv))); } break;
case 88:
#line	230	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link3(yypt[-2].yyv, yypt[-1].yyv, yypt[-0].yyv); } break;
case 115:
#line	240	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link2(yypt[-1].yyv, yypt[-0].yyv); } break;
case 116:
#line	242	"/usr/local/plan9/src/cmd/upas/smtp/rfc822.y"
{ yyval = link2(yypt[-1].yyv, yypt[-0].yyv); } break;
	}
	goto yystack;  /* stack new state and value */
}
