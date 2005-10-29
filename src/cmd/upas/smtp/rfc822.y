%{
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
%}

%term WORD
%term DATE
%term RESENT_DATE
%term RETURN_PATH
%term FROM
%term SENDER
%term REPLY_TO
%term RESENT_FROM
%term RESENT_SENDER
%term RESENT_REPLY_TO
%term SUBJECT
%term TO
%term CC
%term BCC
%term RESENT_TO
%term RESENT_CC
%term RESENT_BCC
%term REMOTE
%term PRECEDENCE
%term MIMEVERSION
%term CONTENTTYPE
%term MESSAGEID
%term RECEIVED
%term MAILER
%term BADTOKEN
%start msg
%%

msg		: fields
		| unixfrom '\n' fields
		;
fields		: '\n'
			{ yydone = 1; }
		| field '\n'
		| field '\n' fields
		;
field		: dates
			{ date = 1; }
		| originator
			{ originator = 1; }
		| destination
			{ destination = 1; }
		| subject
		| optional
		| ignored
		| received
		| precedence
		| error '\n' field
		;
unixfrom	: FROM route_addr unix_date_time REMOTE FROM word
			{ freenode($1); freenode($4); freenode($5);
			  usender = $2; udate = $3; usys = $6;
			}
		;
originator	: REPLY_TO ':' address_list
			{ newfield(link3($1, $2, $3), 1); }
		| RETURN_PATH ':' route_addr
			{ newfield(link3($1, $2, $3), 1); }
		| FROM ':' mailbox_list
			{ newfield(link3($1, $2, $3), 1); }
		| SENDER ':' mailbox
			{ newfield(link3($1, $2, $3), 1); }
		| RESENT_REPLY_TO ':' address_list
			{ newfield(link3($1, $2, $3), 1); }
		| RESENT_SENDER ':' mailbox
			{ newfield(link3($1, $2, $3), 1); }
		| RESENT_FROM ':' mailbox
			{ newfield(link3($1, $2, $3), 1); }
		;
dates 		: DATE ':' date_time
			{ newfield(link3($1, $2, $3), 0); }
		| RESENT_DATE ':' date_time
			{ newfield(link3($1, $2, $3), 0); }
		;
destination	: TO ':'
			{ newfield(link2($1, $2), 0); }
		| TO ':' address_list
			{ newfield(link3($1, $2, $3), 0); }
		| RESENT_TO ':'
			{ newfield(link2($1, $2), 0); }
		| RESENT_TO ':' address_list
			{ newfield(link3($1, $2, $3), 0); }
		| CC ':'
			{ newfield(link2($1, $2), 0); }
		| CC ':' address_list
			{ newfield(link3($1, $2, $3), 0); }
		| RESENT_CC ':'
			{ newfield(link2($1, $2), 0); }
		| RESENT_CC ':' address_list
			{ newfield(link3($1, $2, $3), 0); }
		| BCC ':'
			{ newfield(link2($1, $2), 0); }
		| BCC ':' address_list
			{ newfield(link3($1, $2, $3), 0); }
		| RESENT_BCC ':' 
			{ newfield(link2($1, $2), 0); }
		| RESENT_BCC ':' address_list
			{ newfield(link3($1, $2, $3), 0); }
		;
subject		: SUBJECT ':' things
			{ newfield(link3($1, $2, $3), 0); }
		| SUBJECT ':'
			{ newfield(link2($1, $2), 0); }
		;
received	: RECEIVED ':' things
			{ newfield(link3($1, $2, $3), 0); received++; }
		| RECEIVED ':'
			{ newfield(link2($1, $2), 0); received++; }
		;
precedence	: PRECEDENCE ':' things
			{ newfield(link3($1, $2, $3), 0); }
		| PRECEDENCE ':'
			{ newfield(link2($1, $2), 0); }
		;
ignored		: ignoredhdr ':' things
			{ newfield(link3($1, $2, $3), 0); }
		| ignoredhdr ':'
			{ newfield(link2($1, $2), 0); }
		;
ignoredhdr	: MIMEVERSION | CONTENTTYPE | MESSAGEID { messageid = 1; } | MAILER
		;
optional	: fieldwords ':' things
			{ /* hack to allow same lex for field names and the rest */
			 if(badfieldname($1)){
				freenode($1);
				freenode($2);
				freenode($3);
				return 1;
			 }
			 newfield(link3($1, $2, $3), 0);
			}
		| fieldwords ':'
			{ /* hack to allow same lex for field names and the rest */
			 if(badfieldname($1)){
				freenode($1);
				freenode($2);
				return 1;
			 }
			 newfield(link2($1, $2), 0);
			}
		;
address_list	: address
		| address_list ',' address
			{ $$ = link3($1, $2, $3); }
		;
address		: mailbox
		| group
		;
group		: phrase ':' address_list ';'
			{ $$ = link2($1, link3($2, $3, $4)); }
		| phrase ':' ';'
			{ $$ = link3($1, $2, $3); }
		;
mailbox_list	: mailbox
		| mailbox_list ',' mailbox
			{ $$ = link3($1, $2, $3); }
		;
mailbox		: route_addr
		| phrase brak_addr
			{ $$ = link2($1, $2); }
		| brak_addr
		;
brak_addr	: '<' route_addr '>'
			{ $$ = link3($1, $2, $3); }
		| '<' '>'
			{ $$ = nobody($2); freenode($1); }
		;
route_addr	: route ':' at_addr
			{ $$ = address(concat($1, concat($2, $3))); }
		| addr_spec
		;
route		: '@' domain
			{ $$ = concat($1, $2); }
		| route ',' '@' domain
			{ $$ = concat($1, concat($2, concat($3, $4))); }
		;
addr_spec	: local_part
			{ $$ = address($1); }
		| at_addr
		;
at_addr		: local_part '@' domain
			{ $$ = address(concat($1, concat($2, $3)));}
		| at_addr '@' domain
			{ $$ = address(concat($1, concat($2, $3)));}
		;
local_part	: word
		;
domain		: word
		;
phrase		: word
		| phrase word
			{ $$ = link2($1, $2); }
		;
things		: thing
		| things thing
			{ $$ = link2($1, $2); }
		;
thing		: word | '<' | '>' | '@' | ':' | ';' | ','
		;
date_time	: things
		;
unix_date_time	: word word word unix_time word word
			{ $$ = link3($1, $3, link3($2, $6, link2($4, $5))); }
		;
unix_time	: word
		| unix_time ':' word
			{ $$ = link3($1, $2, $3); }
		;
word		: WORD | DATE | RESENT_DATE | RETURN_PATH | FROM | SENDER
		| REPLY_TO | RESENT_FROM | RESENT_SENDER | RESENT_REPLY_TO
		| TO | CC | BCC | RESENT_TO | RESENT_CC | RESENT_BCC | REMOTE | SUBJECT
		| PRECEDENCE | MIMEVERSION | CONTENTTYPE | MESSAGEID | RECEIVED | MAILER
		;
fieldwords	: fieldword
		| WORD
		| fieldwords fieldword
			{ $$ = link2($1, $2); }
		| fieldwords word
			{ $$ = link2($1, $2); }
		;
fieldword	: '<' | '>' | '@' | ';' | ','
		;
%%

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
