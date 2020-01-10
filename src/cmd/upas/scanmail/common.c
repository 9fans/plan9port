#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include "spam.h"

enum {
	Quanta	= 8192,
	Minbody = 6000,
	HdrMax	= 15
};

typedef struct keyword Keyword;
typedef struct word Word;

struct word{
	char	*string;
	int	n;
};

struct	keyword{
	char	*string;
	int	value;
};

Word	htmlcmds[] =
{
	"html",		4,
	"!doctype html", 13,
	0,

};

Word	hrefs[] =
{
	"a href=",	7,
	"a title=",	8,
	"a target=",	9,
	"base href=",	10,
	"img src=",	8,
	"img border=",	11,
	"form action=", 12,
	"!--",		3,
	0,

};

/*
 *	RFC822 header keywords to look for for fractured header.
 *	all lengths must be less than HdrMax defined above.
 */
Word	hdrwords[] =
{
	"cc:",			3,
	"bcc:", 		4,
	"to:",			3,
	0,			0,

};

Keyword	keywords[] =
{
	"header",	HoldHeader,
	"line",		SaveLine,
	"hold",		Hold,
	"dump",		Dump,
	"loff",		Lineoff,
	0,		Nactions
};

Patterns patterns[] = {
[Dump]		{ "DUMP:", 0, 0 },
[HoldHeader]	{ "HEADER:", 0, 0 },
[Hold]		{ "HOLD:", 0, 0 },
[SaveLine]	{ "LINE:", 0, 0 },
[Lineoff]	{ "LINEOFF:", 0, 0 },
[Nactions]	{ 0, 0, 0 }
};

static char*	endofhdr(char*, char*);
static	int	escape(char**);
static	int	extract(char*);
static	int	findkey(char*);
static	int	hash(int);
static	int	isword(Word*, char*, int);
static	void	parsealt(Biobuf*, char*, Spat**);

/*
 *	The canonicalizer: convert input to canonical representation
 */
char*
readmsg(Biobuf *bp, int *hsize, int *bufsize)
{
	char *p, *buf;
	int n, offset, eoh, bsize, delta;

	buf = 0;
	offset = 0;
	if(bufsize)
		*bufsize = 0;
	if(hsize)
		*hsize = 0;
	for(;;) {
		buf = Realloc(buf, offset+Quanta+1);
		n = Bread(bp, buf+offset, Quanta);
		if(n < 0){
			free(buf);
			return 0;
		}
		p = buf+offset;			/* start of this chunk */
		offset += n;			/* end of this chunk */
		buf[offset] = 0;
		if(n == 0){
			if(offset == 0)
				return 0;
			break;
		}

		if(hsize == 0)			/* don't process header */
			break;
		if(p != buf && p[-1] == '\n')	/* check for EOH across buffer split */
			p--;
		p = endofhdr(p, buf+offset);
		if(p)
			break;
		if(offset >= Maxread)		/* gargantuan header - just punt*/
		{
			if(hsize)
				*hsize = offset;
			if(bufsize)
				*bufsize = offset;
			return buf;
		}
	}
	eoh = p-buf;				/* End of header */
	bsize = offset - eoh;			/* amount of body already read */

		/* Read at least Minbody bytes of the body */
	if (bsize < Minbody){
		delta = Minbody-bsize;
		buf = Realloc(buf, offset+delta+1);
		n = Bread(bp, buf+offset, delta);
		if(n > 0) {
			offset += n;
			buf[offset] = 0;
		}
	}
	if(hsize)
		*hsize = eoh;
	if(bufsize)
		*bufsize = offset;
	return buf;
}

static	int
isword(Word *wp, char *text, int len)
{
	for(;wp->string; wp++)
		if(len >= wp->n && strncmp(text, wp->string, wp->n) == 0)
			return 1;
	return 0;
}

static char*
endofhdr(char *raw, char *end)
{
	int i;
	char *p, *q;
	char buf[HdrMax];

	/*
 	 * can't use strchr to search for newlines because
	 * there may be embedded NULL's.
	 */
	for(p = raw; p < end; p++){
		if(*p != '\n' || p[1] != '\n')
			continue;
		p++;
		for(i = 0, q = p+1; i < sizeof(buf) && *q; q++){
			buf[i++] = tolower(*q);
			if(*q == ':' || *q == '\n')
				break;
		}
		if(!isword(hdrwords, buf, i))
			return p+1;
	}
	return 0;
}

static	int
htmlmatch(Word *wp, char *text, char *end, int *n)
{
	char *cp;
	int i, c, lastc;
	char buf[MaxHtml];

	/*
	 * extract a string up to '>'
	 */

	i = lastc = 0;
	cp = text;
	while (cp < end && i < sizeof(buf)-1){
		c = *cp++;
		if(c == '=')
			c = escape(&cp);
		switch(c){
		case 0:
		case '\r':
			continue;
		case '>':
			goto out;
		case '\n':
		case ' ':
		case '\t':
			if(lastc == ' ')
				continue;
			c = ' ';
			break;
		default:
			c = tolower(c);
			break;
		}
		buf[i++] = lastc = c;
	}
out:
	buf[i] = 0;
	if(n)
		*n = cp-text;
	return isword(wp, buf, i);
}

static int
escape(char **msg)
{
	int c;
	char *p;

	p = *msg;
	c = *p;
	if(c == '\n'){
		p++;
		c = *p++;
	} else
	if(c == '2'){
		c = tolower(p[1]);
		if(c == 'e'){
			p += 2;
			c = '.';
		}else
		if(c == 'f'){
			p += 2;
			c = '/';
		}else
		if(c == '0'){
			p += 2;
			c = ' ';
		}
		else c = '=';
	} else {
		if(c == '3' && tolower(p[1]) == 'd')
			p += 2;
		c = '=';
	}
	*msg = p;
	return c;
}

static int
htmlchk(char **msg, char *end)
{
	int n;
	char *p;

	static int ishtml;

	p = *msg;
	if(ishtml == 0){
		ishtml = htmlmatch(htmlcmds, p, end, &n);

		/* If not an HTML keyword, check if it's
		 * an HTML comment (<!comment>).  if so,
		 * skip over it; otherwise copy it in.
		 */
		if(ishtml == 0 && *p != '!')	/* not comment */
			return '<';		/* copy it */

	} else if(htmlmatch(hrefs, p, end, &n))	/* if special HTML string  */
		return '<';			/* copy it */

	/*
	 * this is an uninteresting HTML command; skip over it.
	 */
	p += n;
	*msg = p+1;
	return *p;
}

/*
 * decode a base 64 encode body
 */
void
conv64(char *msg, char *end, char *buf, int bufsize)
{
	int len, i;
	char *cp;

	len = end - msg;
	i = (len*3)/4+1;	/* room for max chars + null */
	cp = Malloc(i);
	len = dec64((uchar*)cp, i, msg, len);
	convert(cp, cp+len, buf, bufsize, 1);
	free(cp);
}

int
convert(char *msg, char *end, char *buf, int bufsize, int isbody)
{

	char *p;
	int c, lastc, base64;

	lastc = 0;
	base64 = 0;
	while(msg < end && bufsize > 0){
		c = *msg++;

		/*
		 * In the body only, try to strip most HTML and
		 * replace certain MIME escape sequences with the character
		 */
		if(isbody) {
			do{
				p = msg;
				if(c == '<')
					c = htmlchk(&msg, end);
				if(c == '=')
					c = escape(&msg);
			} while(p != msg && p < end);
		}
		switch(c){
		case 0:
		case '\r':
			continue;
		case '\t':
		case ' ':
		case '\n':
			if(lastc == ' ')
				continue;
			c = ' ';
			break;
		case 'C':	/* check for MIME base 64 encoding in header */
		case 'c':
			if(isbody == 0)
			if(msg < end-32 && *msg == 'o' && msg[1] == 'n')
			if(cistrncmp(msg+2, "tent-transfer-encoding: base64", 30) == 0)
				base64 = 1;
			c = 'c';
			break;
		default:
			c = tolower(c);
			break;
		}
		*buf++ = c;
		lastc = c;
		bufsize--;
	}
	*buf = 0;
	return base64;
}

/*
 *	The pattern parser: build data structures from the pattern file
 */

static int
hash(int c)
{
	return c & 127;
}

static	int
findkey(char *val)
{
	Keyword *kp;

	for(kp = keywords; kp->string; kp++)
		if(strcmp(val, kp->string) == 0)
				break;
	return kp->value;
}

#define	whitespace(c)	((c) == ' ' || (c) == '\t')

void
parsepats(Biobuf *bp)
{
	Pattern *p, *new;
	char *cp, *qp;
	int type, action, n, h;
	Spat *spat;

	for(;;){
		cp = Brdline(bp, '\n');
		if(cp == 0)
			break;
		cp[Blinelen(bp)-1] = 0;
		while(*cp == ' ' || *cp == '\t')
			cp++;
		if(*cp == '#' || *cp == 0)
			continue;
		type = regexp;
		if(*cp == '*'){
			type = string;
			cp++;
		}
		qp = strchr(cp, ':');
		if(qp == 0)
			continue;
		*qp = 0;
		if(debug)
			fprint(2, "action = %s\n", cp);
		action = findkey(cp);
		if(action >= Nactions)
			continue;
		cp = qp+1;
		n = extract(cp);
		if(n <= 0 || *cp == 0)
			continue;

		qp = strstr(cp, "~~");
		if(qp){
			*qp = 0;
			n = strlen(cp);
		}
		if(debug)
			fprint(2, " Pattern: `%s'\n", cp);

			/* Hook regexps into a chain */
		if(type == regexp) {
			new = Malloc(sizeof(Pattern));
			new->action = action;
			new->pat = regcomp(cp);
			if(new->pat == 0){
				free(new);
				continue;
			}
			new->type = regexp;
			new->alt = 0;
			new->next = 0;

			if(qp)
				parsealt(bp, qp+2, &new->alt);

			new->next = patterns[action].regexps;
			patterns[action].regexps = new;
			continue;

		}
			/* not a Regexp - hook strings into Pattern hash chain */
		spat = Malloc(sizeof(*spat));
		spat->next = 0;
		spat->alt = 0;
		spat->len = n;
		spat->string = Malloc(n+1);
		spat->c1 = cp[1];
		strcpy(spat->string, cp);

		if(qp)
			parsealt(bp, qp+2, &spat->alt);

		p = patterns[action].strings;
		if(p == 0) {
			p = Malloc(sizeof(Pattern));
			memset(p, 0, sizeof(*p));
			p->action = action;
			p->type = string;
			patterns[action].strings = p;
		}
		h = hash(*spat->string);
		spat->next = p->spat[h];
		p->spat[h] = spat;
	}
}

static void
parsealt(Biobuf *bp, char *cp, Spat** head)
{
	char *p;
	Spat *alt;

	while(cp){
		if(*cp == 0){		/*escaped newline*/
			do{
				cp = Brdline(bp, '\n');
				if(cp == 0)
					return;
				cp[Blinelen(bp)-1] = 0;
			} while(extract(cp) <= 0 || *cp == 0);
		}

		p = cp;
		cp = strstr(p, "~~");
		if(cp){
			*cp = 0;
			cp += 2;
		}
		if(strlen(p)){
			alt = Malloc(sizeof(*alt));
			alt->string = strdup(p);
			alt->next = *head;
			*head = alt;
		}
	}
}

static int
extract(char *cp)
{
	int c;
	char *p, *q, *r;

	p = q = r = cp;
	while(whitespace(*p))
		p++;
	while(c = *p++){
		if (c == '#')
			break;
		if(c == '"'){
			while(*p && *p != '"'){
				if(*p == '\\' && p[1] == '"')
					p++;
				if('A' <= *p && *p <= 'Z')
					*q++ = *p++ + ('a'-'A');
				else
					*q++ = *p++;
			}
			if(*p)
				p++;
			r = q;		/* never back up over a quoted string */
		} else {
			if('A' <= c && c <= 'Z')
				c += ('a'-'A');
			*q++ = c;
		}
	}
	while(q > r && whitespace(q[-1]))
		q--;
	*q = 0;
	return q-cp;
}

/*
 *	The matching engine: compare canonical input to pattern structures
 */

static Spat*
isalt(char *message, Spat *alt)
{
	while(alt) {
		if(*cmd)
		if(message != cmd && strstr(cmd, alt->string))
			break;
		if(message != header+1 && strstr(header+1, alt->string))
			break;
		if(strstr(message, alt->string))
			break;
		alt = alt->next;
	}
	return alt;
}

int
matchpat(Pattern *p, char *message, Resub *m)
{
	Spat *spat;
	char *s;
	int c, c1;

	if(p->type == string){
		c1 = *message;
		for(s=message; c=c1; s++){
			c1 = s[1];
			for(spat=p->spat[hash(c)]; spat; spat=spat->next){
				if(c1 == spat->c1)
				if(memcmp(s, spat->string, spat->len) == 0)
				if(!isalt(message, spat->alt)){
					m->s.sp = s;
					m->e.ep = s + spat->len;
					return 1;
				}
			}
		}
		return 0;
	}
	m->s.sp = m->e.ep = 0;
	if(regexec(p->pat, message, m, 1) == 0)
		return 0;
	if(isalt(message, p->alt))
		return 0;
	return 1;
}


void
xprint(int fd, char *type, Resub *m)
{
	char *p, *q;
	int i;

	if(m->s.sp == 0 || m->e.ep == 0)
		return;

		/* back up approx 30 characters to whitespace */
	for(p = m->s.sp, i = 0; *p && i < 30; i++, p--)
			;
	while(*p && *p != ' ')
		p--;
	p++;

		/* grab about 30 more chars beyond the end of the match */
	for(q = m->e.ep, i = 0; *q && i < 30; i++, q++)
			;
	while(*q && *q != ' ')
		q++;

	fprint(fd, "%s %.*s~%.*s~%.*s\n", type, (int)(m->s.sp-p), p, (int)(m->e.ep-m->s.sp), m->s.sp, (int)(q-m->e.ep), m->e.ep);
}

enum {
	INVAL=	255
};

static uchar t64d[256] = {
/*00 */	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*10*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*20*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL,    62, INVAL, INVAL, INVAL,    63,
/*30*/	   52,	  53,	 54,	55,    56,    57,    58,    59,
	   60,	  61, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*40*/	INVAL,    0,      1,     2,     3,     4,     5,     6,
	    7,    8,      9,    10,    11,    12,    13,    14,
/*50*/	   15,   16,     17,    18,    19,    20,    21,    22,
	   23,   24,     25, INVAL, INVAL, INVAL, INVAL, INVAL,
/*60*/	INVAL,   26,     27,    28,    29,    30,    31,    32,
	   33,   34,     35,    36,    37,    38,    39,    40,
/*70*/	   41,   42,     43,    44,    45,    46,    47,    48,
	   49,   50,     51, INVAL, INVAL, INVAL, INVAL, INVAL,
/*80*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*90*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*A0*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*B0*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*C0*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*D0*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*E0*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
/*F0*/	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL,
	INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL, INVAL
};
