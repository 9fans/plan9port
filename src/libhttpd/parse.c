#include <u.h>
#include <libc.h>
#include <bin.h>
#include <httpd.h>
#include "escape.h"

typedef struct Hlex	Hlex;
typedef struct MimeHead	MimeHead;

enum
{
	/*
	 * tokens
	 */
	Word	= 1,
	QString
};

#define UlongMax	4294967295UL

struct Hlex
{
	int	tok;
	int	eoh;
	int	eol;			/* end of header line encountered? */
	uchar	*hstart;		/* start of header */
	jmp_buf	jmp;			/* jmp here to parse header */
	char	wordval[HMaxWord];
	HConnect *c;
};

struct MimeHead
{
	char	*name;
	void	(*parse)(Hlex*, char*);
	uchar	seen;
	uchar	ignore;
};

static void	mimeaccept(Hlex*, char*);
static void	mimeacceptchar(Hlex*, char*);
static void	mimeacceptenc(Hlex*, char*);
static void	mimeacceptlang(Hlex*, char*);
static void	mimeagent(Hlex*, char*);
static void	mimeauthorization(Hlex*, char*);
static void	mimeconnection(Hlex*, char*);
static void	mimecontlen(Hlex*, char*);
static void	mimeexpect(Hlex*, char*);
static void	mimefresh(Hlex*, char*);
static void	mimefrom(Hlex*, char*);
static void	mimehost(Hlex*, char*);
static void	mimeifrange(Hlex*, char*);
static void	mimeignore(Hlex*, char*);
static void	mimematch(Hlex*, char*);
static void	mimemodified(Hlex*, char*);
static void	mimenomatch(Hlex*, char*);
static void	mimerange(Hlex*, char*);
static void	mimetransenc(Hlex*, char*);
static void	mimeunmodified(Hlex*, char*);

/*
 * headers seen also include
 * allow  cache-control chargeto
 * content-encoding content-language content-location content-md5 content-range content-type
 * date etag expires forwarded last-modified max-forwards pragma
 * proxy-agent proxy-authorization proxy-connection
 * ua-color ua-cpu ua-os ua-pixels
 * upgrade via x-afs-tokens x-serial-number
 */
static MimeHead	mimehead[] =
{
	{"accept",		mimeaccept},
	{"accept-charset",	mimeacceptchar},
	{"accept-encoding",	mimeacceptenc},
	{"accept-language",	mimeacceptlang},
	{"authorization",	mimeauthorization},
	{"connection",		mimeconnection},
	{"content-length",	mimecontlen},
	{"expect",		mimeexpect},
	{"fresh",		mimefresh},
	{"from",		mimefrom},
	{"host",		mimehost},
	{"if-match",		mimematch},
	{"if-modified-since",	mimemodified},
	{"if-none-match",	mimenomatch},
	{"if-range",		mimeifrange},
	{"if-unmodified-since",	mimeunmodified},
	{"range",		mimerange},
	{"transfer-encoding",	mimetransenc},
	{"user-agent",		mimeagent},
};

char*		hmydomain;
char*		hversion = "HTTP/1.1";

static	void	lexhead(Hlex*);
static	void	parsejump(Hlex*, char*);
static	int	getc(Hlex*);
static	void	ungetc(Hlex*);
static	int	wordcr(Hlex*);
static	int	wordnl(Hlex*);
static	void	word(Hlex*, char*);
static	int	lex1(Hlex*, int);
static	int	lex(Hlex*);
static	int	lexbase64(Hlex*);
static	ulong	digtoul(char *s, char **e);

/*
 * flush an clean up junk from a request
 */
void
hreqcleanup(HConnect *c)
{
	int i;

	hxferenc(&c->hout, 0);
	memset(&c->req, 0, sizeof(c->req));
	memset(&c->head, 0, sizeof(c->head));
	c->hpos = c->header;
	c->hstop = c->header;
	binfree(&c->bin);
	for(i = 0; i < nelem(mimehead); i++){
		mimehead[i].seen = 0;
		mimehead[i].ignore = 0;
	}
}

/*
 * list of tokens
 * if the client is HTTP/1.0,
 * ignore headers which match one of the tokens.
 * restarts parsing if necessary.
 */
static void
mimeconnection(Hlex *h, char *unused)
{
	char *u, *p;
	int reparse, i;

	reparse = 0;
	for(;;){
		while(lex(h) != Word)
			if(h->tok != ',')
				goto breakout;

		if(cistrcmp(h->wordval, "keep-alive") == 0)
			h->c->head.persist = 1;
		else if(cistrcmp(h->wordval, "close") == 0)
			h->c->head.closeit = 1;
		else if(!http11(h->c)){
			for(i = 0; i < nelem(mimehead); i++){
				if(cistrcmp(mimehead[i].name, h->wordval) == 0){
					reparse = mimehead[i].seen && !mimehead[i].ignore;
					mimehead[i].ignore = 1;
					if(cistrcmp(mimehead[i].name, "authorization") == 0){
						h->c->head.authuser = nil;
						h->c->head.authpass = nil;
					}
				}
			}
		}

		if(lex(h) != ',')
			break;
	}

breakout:;
	/*
	 * if need to ignore headers we've already parsed,
	 * reset & start over.  need to save authorization
	 * info because it's written over when parsed.
	 */
	if(reparse){
		u = h->c->head.authuser;
		p = h->c->head.authpass;
		memset(&h->c->head, 0, sizeof(h->c->head));
		h->c->head.authuser = u;
		h->c->head.authpass = p;

		h->c->hpos = h->hstart;
		longjmp(h->jmp, 1);
	}
}

int
hparseheaders(HConnect *c, int timeout)
{
	Hlex h;

	c->head.fresh_thresh = 0;
	c->head.fresh_have = 0;
	c->head.persist = 0;
	if(c->req.vermaj == 0){
		c->head.host = hmydomain;
		return 1;
	}

	memset(&h, 0, sizeof(h));
	h.c = c;
	if(timeout)
		alarm(timeout);
	if(hgethead(c, 1) < 0)
		return -1;
	if(timeout)
		alarm(0);
	h.hstart = c->hpos;

	if(setjmp(h.jmp) == -1)
		return -1;

	h.eol = 0;
	h.eoh = 0;
	h.tok = '\n';
	while(lex(&h) != '\n'){
		if(h.tok == Word && lex(&h) == ':')
			parsejump(&h, hstrdup(c, h.wordval));
		while(h.tok != '\n')
			lex(&h);
		h.eol = h.eoh;
	}

	if(http11(c)){
		/*
		 * according to the http/1.1 spec,
		 * these rules must be followed
		 */
		if(c->head.host == nil){
			hfail(c, HBadReq, nil);
			return -1;
		}
		if(c->req.urihost != nil)
			c->head.host = c->req.urihost;
		/*
		 * also need to check host is actually this one
		 */
	}else if(c->head.host == nil)
		c->head.host = hmydomain;
	return 1;
}

/*
 * mimeparams	: | mimeparams ";" mimepara
 * mimeparam	: token "=" token | token "=" qstring
 */
static HSPairs*
mimeparams(Hlex *h)
{
	HSPairs *p;
	char *s;

	p = nil;
	for(;;){
		if(lex(h) != Word)
			break;
		s = hstrdup(h->c, h->wordval);
		if(lex(h) != Word && h->tok != QString)
			break;
		p = hmkspairs(h->c, s, hstrdup(h->c, h->wordval), p);
	}
	return hrevspairs(p);
}

/*
 * mimehfields	: mimehfield | mimehfields commas mimehfield
 * mimehfield	: token mimeparams
 * commas	: "," | commas ","
 */
static HFields*
mimehfields(Hlex *h)
{
	HFields *f;

	f = nil;
	for(;;){
		while(lex(h) != Word)
			if(h->tok != ',')
				goto breakout;

		f = hmkhfields(h->c, hstrdup(h->c, h->wordval), nil, f);

		if(lex(h) == ';')
			f->params = mimeparams(h);
		if(h->tok != ',')
			break;
	}
breakout:;
	return hrevhfields(f);
}

/*
 * parse a list of acceptable types, encodings, languages, etc.
 */
static HContent*
mimeok(Hlex *h, char *name, int multipart, HContent *head)
{
	char *generic, *specific, *s;
	float v;

	/*
	 * each type is separated by one or more commas
	 */
	while(lex(h) != Word)
		if(h->tok != ',')
			return head;

	generic = hstrdup(h->c, h->wordval);
	lex(h);
	if(h->tok == '/' || multipart){
		/*
		 * at one time, IE5 improperly said '*' for single types
		 */
		if(h->tok != '/')
			return nil;
		if(lex(h) != Word)
			return head;
		specific = hstrdup(h->c, h->wordval);
		if(!multipart && strcmp(specific, "*") != 0)
			return head;
		lex(h);
	}else
		specific = nil;
	head = hmkcontent(h->c, generic, specific, head);

	for(;;){
		switch(h->tok){
		case ';':
			/*
			 * should make a list of these params
			 * for accept, they fall into two classes:
			 *	up to a q=..., they modify the media type.
			 *	afterwards, they acceptance criteria
			 */
			if(lex(h) == Word){
				s = hstrdup(h->c, h->wordval);
				if(lex(h) != '=' || lex(h) != Word && h->tok != QString)
					return head;
				v = strtod(h->wordval, nil);
				if(strcmp(s, "q") == 0)
					head->q = v;
				else if(strcmp(s, "mxb") == 0)
					head->mxb = v;
			}
			break;
		case ',':
			return  mimeok(h, name, multipart, head);
		default:
			return head;
		}
		lex(h);
	}
}

/*
 * parse a list of entity tags
 * 1#entity-tag
 * entity-tag = [weak] opaque-tag
 * weak = "W/"
 * opaque-tag = quoted-string
 */
static HETag*
mimeetag(Hlex *h, HETag *head)
{
	HETag *e;
	int weak;

	for(;;){
		while(lex(h) != Word && h->tok != QString)
			if(h->tok != ',')
				return head;

		weak = 0;
		if(h->tok == Word && strcmp(h->wordval, "*") != 0){
			if(strcmp(h->wordval, "W") != 0)
				return head;
			if(lex(h) != '/' || lex(h) != QString)
				return head;
			weak = 1;
		}

		e = halloc(h->c, sizeof(HETag));
		e->etag = hstrdup(h->c, h->wordval);
		e->weak = weak;
		e->next = head;
		head = e;

		if(lex(h) != ',')
			return head;
	}
}

/*
 * ranges-specifier = byte-ranges-specifier
 * byte-ranges-specifier = "bytes" "=" byte-range-set
 * byte-range-set = 1#(byte-range-spec|suffix-byte-range-spec)
 * byte-range-spec = byte-pos "-" [byte-pos]
 * byte-pos = 1*DIGIT
 * suffix-byte-range-spec = "-" suffix-length
 * suffix-length = 1*DIGIT
 *
 * syntactically invalid range specifiers cause the
 * entire header field to be ignored.
 * it is syntactically incorrect for the second byte pos
 * to be smaller than the first byte pos
 */
static HRange*
mimeranges(Hlex *h, HRange *head)
{
	HRange *r, *rh, *tail;
	char *w;
	ulong start, stop;
	int suf;

	if(lex(h) != Word || strcmp(h->wordval, "bytes") != 0 || lex(h) != '=')
		return head;

	rh = nil;
	tail = nil;
	for(;;){
		while(lex(h) != Word){
			if(h->tok != ','){
				if(h->tok == '\n')
					goto breakout;
				return head;
			}
		}

		w = h->wordval;
		start = 0;
		suf = 1;
		if(w[0] != '-'){
			suf = 0;
			start = digtoul(w, &w);
			if(w[0] != '-')
				return head;
		}
		w++;
		stop = ~0UL;
		if(w[0] != '\0'){
			stop = digtoul(w, &w);
			if(w[0] != '\0')
				return head;
			if(!suf && stop < start)
				return head;
		}

		r = halloc(h->c, sizeof(HRange));
		r->suffix = suf;
		r->start = start;
		r->stop = stop;
		r->next = nil;
		if(rh == nil)
			rh = r;
		else
			tail->next = r;
		tail = r;

		if(lex(h) != ','){
			if(h->tok == '\n')
				break;
			return head;
		}
	}
breakout:;

	if(head == nil)
		return rh;

	for(tail = head; tail->next != nil; tail = tail->next)
		;
	tail->next = rh;
	return head;
}

static void
mimeaccept(Hlex *h, char *name)
{
	h->c->head.oktype = mimeok(h, name, 1, h->c->head.oktype);
}

static void
mimeacceptchar(Hlex *h, char *name)
{
	h->c->head.okchar = mimeok(h, name, 0, h->c->head.okchar);
}

static void
mimeacceptenc(Hlex *h, char *name)
{
	h->c->head.okencode = mimeok(h, name, 0, h->c->head.okencode);
}

static void
mimeacceptlang(Hlex *h, char *name)
{
	h->c->head.oklang = mimeok(h, name, 0, h->c->head.oklang);
}

static void
mimemodified(Hlex *h, char *unused)
{
	lexhead(h);
	h->c->head.ifmodsince = hdate2sec(h->wordval);
}

static void
mimeunmodified(Hlex *h, char *unused)
{
	lexhead(h);
	h->c->head.ifunmodsince = hdate2sec(h->wordval);
}

static void
mimematch(Hlex *h, char *unused)
{
	h->c->head.ifmatch = mimeetag(h, h->c->head.ifmatch);
}

static void
mimenomatch(Hlex *h, char *unused)
{
	h->c->head.ifnomatch = mimeetag(h, h->c->head.ifnomatch);
}

/*
 * argument is either etag or date
 */
static void
mimeifrange(Hlex *h, char *unused)
{
	int c, d, et;

	et = 0;
	c = getc(h);
	while(c == ' ' || c == '\t')
		c = getc(h);
	if(c == '"')
		et = 1;
	else if(c == 'W'){
		d = getc(h);
		if(d == '/')
			et = 1;
		ungetc(h);
	}
	ungetc(h);
	if(et){
		h->c->head.ifrangeetag = mimeetag(h, h->c->head.ifrangeetag);
	}else{
		lexhead(h);
		h->c->head.ifrangedate = hdate2sec(h->wordval);
	}
}

static void
mimerange(Hlex *h, char *unused)
{
	h->c->head.range = mimeranges(h, h->c->head.range);
}

/*
 * note: netscape and ie through versions 4.7 and 4
 * support only basic authorization, so that is all that is supported here
 *
 * "Authorization" ":" "Basic" base64-user-pass
 * where base64-user-pass is the base64 encoding of
 * username ":" password
 */
static void
mimeauthorization(Hlex *h, char *unused)
{
	char *up, *p;
	int n;

	if(lex(h) != Word || cistrcmp(h->wordval, "basic") != 0)
		return;

	n = lexbase64(h);
	if(!n)
		return;

	/*
	 * wipe out source for password, so it won't be logged.
	 * it is replaced by a single =,
	 * which is valid base64, but not ok for an auth reponse.
	 * therefore future parses of the header field will not overwrite
	 * authuser and authpass.
	 */
	memmove(h->c->hpos - (n - 1), h->c->hpos, h->c->hstop - h->c->hpos);
	h->c->hstop -= n - 1;
	*h->c->hstop = '\0';
	h->c->hpos -= n - 1;
	h->c->hpos[-1] = '=';

	up = halloc(h->c, n + 1);
	n = dec64((uchar*)up, n, h->wordval, n);
	up[n] = '\0';
	p = strchr(up, ':');
	if(p != nil){
		*p++ = '\0';
		h->c->head.authuser = hstrdup(h->c, up);
		h->c->head.authpass = hstrdup(h->c, p);
	}
}

static void
mimeagent(Hlex *h, char *unused)
{
	lexhead(h);
	h->c->head.client = hstrdup(h->c, h->wordval);
}

static void
mimefrom(Hlex *h, char *unused)
{
	lexhead(h);
}

static void
mimehost(Hlex *h, char *unused)
{
	char *hd;

	lexhead(h);
	for(hd = h->wordval; *hd == ' ' || *hd == '\t'; hd++)
		;
	h->c->head.host = hlower(hstrdup(h->c, hd));
}

/*
 * if present, implies that a message body follows the headers
 * "content-length" ":" digits
 */
static void
mimecontlen(Hlex *h, char *unused)
{
	char *e;
	ulong v;

	if(lex(h) != Word)
		return;
	e = h->wordval;
	v = digtoul(e, &e);
	if(v == ~0UL || *e != '\0')
		return;
	h->c->head.contlen = v;
}

/*
 * mimexpect	: "expect" ":" expects
 * expects	: | expects "," expect
 * expect	: "100-continue" | token | token "=" token expectparams | token "=" qstring expectparams
 * expectparams	: ";" token | ";" token "=" token | token "=" qstring
 * for now, we merely parse "100-continue" or anything else.
 */
static void
mimeexpect(Hlex *h, char *unused)
{
	if(lex(h) != Word || cistrcmp(h->wordval, "100-continue") != 0 || lex(h) != '\n')
		h->c->head.expectother = 1;
	h->c->head.expectcont = 1;
}

static void
mimetransenc(Hlex *h, char *unused)
{
	h->c->head.transenc = mimehfields(h);
}

static void
mimefresh(Hlex *h, char *unused)
{
	char *s;

	lexhead(h);
	for(s = h->wordval; *s && (*s==' ' || *s=='\t'); s++)
		;
	if(strncmp(s, "pathstat/", 9) == 0)
		h->c->head.fresh_thresh = atoi(s+9);
	else if(strncmp(s, "have/", 5) == 0)
		h->c->head.fresh_have = atoi(s+5);
}

static void
mimeignore(Hlex *h, char *unused)
{
	lexhead(h);
}

static void
parsejump(Hlex *h, char *k)
{
	int l, r, m;

	l = 1;
	r = nelem(mimehead) - 1;
	while(l <= r){
		m = (r + l) >> 1;
		if(cistrcmp(mimehead[m].name, k) <= 0)
			l = m + 1;
		else
			r = m - 1;
	}
	m = l - 1;
	if(cistrcmp(mimehead[m].name, k) == 0 && !mimehead[m].ignore){
		mimehead[m].seen = 1;
		(*mimehead[m].parse)(h, k);
	}else
		mimeignore(h, k);
}

static int
lex(Hlex *h)
{
	return h->tok = lex1(h, 0);
}

static int
lexbase64(Hlex *h)
{
	int c, n;

	n = 0;
	lex1(h, 1);

	while((c = getc(h)) >= 0){
		if(!(c >= 'A' && c <= 'Z'
		|| c >= 'a' && c <= 'z'
		|| c >= '0' && c <= '9'
		|| c == '+' || c == '/')){
			ungetc(h);
			break;
		}

		if(n < HMaxWord-1)
			h->wordval[n++] = c;
	}
	h->wordval[n] = '\0';
	return n;
}

/*
 * rfc 822/rfc 1521 lexical analyzer
 */
static int
lex1(Hlex *h, int skipwhite)
{
	int level, c;

	if(h->eol)
		return '\n';

top:
	c = getc(h);
	switch(c){
	case '(':
		level = 1;
		while((c = getc(h)) >= 0){
			if(c == '\\'){
				c = getc(h);
				if(c < 0)
					return '\n';
				continue;
			}
			if(c == '(')
				level++;
			else if(c == ')' && --level == 0)
				break;
			else if(c == '\n'){
				c = getc(h);
				if(c < 0)
					return '\n';
				if(c == ')' && --level == 0)
					break;
				if(c != ' ' && c != '\t'){
					ungetc(h);
					return '\n';
				}
			}
		}
		goto top;

	case ' ': case '\t':
		goto top;

	case '\r':
		c = getc(h);
		if(c != '\n'){
			ungetc(h);
			goto top;
		}

	case '\n':
		if(h->tok == '\n'){
			h->eol = 1;
			h->eoh = 1;
			return '\n';
		}
		c = getc(h);
		if(c < 0){
			h->eol = 1;
			return '\n';
		}
		if(c != ' ' && c != '\t'){
			ungetc(h);
			h->eol = 1;
			return '\n';
		}
		goto top;

	case ')':
	case '<': case '>':
	case '[': case ']':
	case '@': case '/':
	case ',': case ';': case ':': case '?': case '=':
		if(skipwhite){
			ungetc(h);
			return c;
		}
		return c;

	case '"':
		if(skipwhite){
			ungetc(h);
			return c;
		}
		word(h, "\"");
		getc(h);		/* skip the closing quote */
		return QString;

	default:
		ungetc(h);
		if(skipwhite)
			return c;
		word(h, "\"(){}<>@,;:/[]?=\r\n \t");
		if(h->wordval[0] == '\0'){
			h->c->head.closeit = 1;
			hfail(h->c, HSyntax);
			longjmp(h->jmp, -1);
		}
		return Word;
	}
	/* not reached */
}

/*
 * return the rest of an rfc 822, including \n
 * do not map to lower case
 */
static void
lexhead(Hlex *h)
{
	int c, n;

	n = 0;
	while((c = getc(h)) >= 0){
		if(c == '\r')
			c = wordcr(h);
		else if(c == '\n')
			c = wordnl(h);
		if(c == '\n')
			break;
		if(c == '\\'){
			c = getc(h);
			if(c < 0)
				break;
		}

		if(n < HMaxWord-1)
			h->wordval[n++] = c;
	}
	h->tok = '\n';
	h->eol = 1;
	h->wordval[n] = '\0';
}

static void
word(Hlex *h, char *stop)
{
	int c, n;

	n = 0;
	while((c = getc(h)) >= 0){
		if(c == '\r')
			c = wordcr(h);
		else if(c == '\n')
			c = wordnl(h);
		if(c == '\\'){
			c = getc(h);
			if(c < 0)
				break;
		}else if(c < 32 || strchr(stop, c) != nil){
			ungetc(h);
			break;
		}

		if(n < HMaxWord-1)
			h->wordval[n++] = c;
	}
	h->wordval[n] = '\0';
}

static int
wordcr(Hlex *h)
{
	int c;

	c = getc(h);
	if(c == '\n')
		return wordnl(h);
	ungetc(h);
	return ' ';
}

static int
wordnl(Hlex *h)
{
	int c;

	c = getc(h);
	if(c == ' ' || c == '\t')
		return c;
	ungetc(h);

	return '\n';
}

static int
getc(Hlex *h)
{
	if(h->eoh)
		return -1;
	if(h->c->hpos < h->c->hstop)
		return *h->c->hpos++;
	h->eoh = 1;
	h->eol = 1;
	return -1;
}

static void
ungetc(Hlex *h)
{
	if(h->eoh)
		return;
	h->c->hpos--;
}

static ulong
digtoul(char *s, char **e)
{
	ulong v;
	int c, ovfl;

	v = 0;
	ovfl = 0;
	for(;;){
		c = *s;
		if(c < '0' || c > '9')
			break;
		s++;
		c -= '0';
		if(v > UlongMax/10 || v == UlongMax/10 && c >= UlongMax%10)
			ovfl = 1;
		v = v * 10 + c;
	}

	if(e)
		*e = s;
	if(ovfl)
		return UlongMax;
	return v;
}

int
http11(HConnect *c)
{
	return c->req.vermaj > 1 || c->req.vermaj == 1 && c->req.vermin > 0;
}

char*
hmkmimeboundary(HConnect *c)
{
	char buf[32];
	int i;

	srand((time(0)<<16)|getpid());
	strcpy(buf, "upas-");
	for(i = 5; i < sizeof(buf)-1; i++)
		buf[i] = 'a' + nrand(26);
	buf[i] = 0;
	return hstrdup(c, buf);
}

HSPairs*
hmkspairs(HConnect *c, char *s, char *t, HSPairs *next)
{
	HSPairs *sp;

	sp = halloc(c, sizeof *sp);
	sp->s = s;
	sp->t = t;
	sp->next = next;
	return sp;
}

HSPairs*
hrevspairs(HSPairs *sp)
{
	HSPairs *last, *next;

	last = nil;
	for(; sp != nil; sp = next){
		next = sp->next;
		sp->next = last;
		last = sp;
	}
	return last;
}

HFields*
hmkhfields(HConnect *c, char *s, HSPairs *p, HFields *next)
{
	HFields *hf;

	hf = halloc(c, sizeof *hf);
	hf->s = s;
	hf->params = p;
	hf->next = next;
	return hf;
}

HFields*
hrevhfields(HFields *hf)
{
	HFields *last, *next;

	last = nil;
	for(; hf != nil; hf = next){
		next = hf->next;
		hf->next = last;
		last = hf;
	}
	return last;
}

HContent*
hmkcontent(HConnect *c, char *generic, char *specific, HContent *next)
{
	HContent *ct;

	ct = halloc(c, sizeof(HContent));
	ct->generic = generic;
	ct->specific = specific;
	ct->next = next;
	ct->q = 1;
	ct->mxb = 0;
	return ct;
}
