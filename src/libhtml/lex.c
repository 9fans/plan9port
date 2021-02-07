#include <u.h>
#include <libc.h>
#include <draw.h>
#include <ctype.h>
#include <html.h>
#include "impl.h"

typedef struct TokenSource TokenSource;
struct TokenSource
{
	int			i;		/* index of next byte to use */
	uchar*		data;		/* all the data */
	int			edata;	/* data[0:edata] is valid */
	int			chset;	/* one of US_Ascii, etc. */
	int			mtype;	/* TextHtml or TextPlain */
};

enum {
	EOF = -2,
	EOB = -1
};

#define ISNAMCHAR(c)	((c)<256 && (isalpha(c) || isdigit(c) || (c) == '-' || (c) == '.'))

#define SMALLBUFSIZE 240
#define BIGBUFSIZE 2000

/* HTML 4.0 tag names. */
/* Keep sorted, and in correspondence with enum in iparse.h. */
Rune **tagnames;
char *_tagnames[] = {
	" ",
	"!",
	"a",
	"abbr",
	"acronym",
	"address",
	"applet",
	"area",
	"b",
	"base",
	"basefont",
	"bdo",
	"big",
	"blink",
	"blockquote",
	"body",
	"bq",
	"br",
	"button",
	"caption",
	"center",
	"cite",
	"code",
	"col",
	"colgroup",
	"dd",
	"del",
	"dfn",
	"dir",
	"div",
	"dl",
	"dt",
	"em",
	"fieldset",
	"font",
	"form",
	"frame",
	"frameset",
	"h1",
	"h2",
	"h3",
	"h4",
	"h5",
	"h6",
	"head",
	"hr",
	"html",
	"i",
	"iframe",
	"img",
	"input",
	"ins",
	"isindex",
	"kbd",
	"label",
	"legend",
	"li",
	"link",
	"map",
	"menu",
	"meta",
	"nobr",
	"noframes",
	"noscript",
	"object",
	"ol",
	"optgroup",
	"option",
	"p",
	"param",
	"pre",
	"q",
	"s",
	"samp",
	"script",
	"select",
	"small",
	"span",
	"strike",
	"strong",
	"style",
	"sub",
	"sup",
	"table",
	"tbody",
	"td",
	"textarea",
	"tfoot",
	"th",
	"thead",
	"title",
	"tr",
	"tt",
	"u",
	"ul",
	"var"
};

/* HTML 4.0 attribute names. */
/* Keep sorted, and in correspondence with enum in i.h. */
Rune **attrnames;
char* _attrnames[] = {
	"abbr",
	"accept-charset",
	"access-key",
	"action",
	"align",
	"alink",
	"alt",
	"archive",
	"axis",
	"background",
	"bgcolor",
	"border",
	"cellpadding",
	"cellspacing",
	"char",
	"charoff",
	"charset",
	"checked",
	"cite",
	"class",
	"classid",
	"clear",
	"code",
	"codebase",
	"codetype",
	"color",
	"cols",
	"colspan",
	"compact",
	"content",
	"coords",
	"data",
	"datetime",
	"declare",
	"defer",
	"dir",
	"disabled",
	"enctype",
	"face",
	"for",
	"frame",
	"frameborder",
	"headers",
	"height",
	"href",
	"hreflang",
	"hspace",
	"http-equiv",
	"id",
	"ismap",
	"label",
	"lang",
	"link",
	"longdesc",
	"marginheight",
	"marginwidth",
	"maxlength",
	"media",
	"method",
	"multiple",
	"name",
	"nohref",
	"noresize",
	"noshade",
	"nowrap",
	"object",
	"onblur",
	"onchange",
	"onclick",
	"ondblclick",
	"onfocus",
	"onkeypress",
	"onkeyup",
	"onload",
	"onmousedown",
	"onmousemove",
	"onmouseout",
	"onmouseover",
	"onmouseup",
	"onreset",
	"onselect",
	"onsubmit",
	"onunload",
	"profile",
	"prompt",
	"readonly",
	"rel",
	"rev",
	"rows",
	"rowspan",
	"rules",
	"scheme",
	"scope",
	"scrolling",
	"selected",
	"shape",
	"size",
	"span",
	"src",
	"standby",
	"start",
	"style",
	"summary",
	"tabindex",
	"target",
	"text",
	"title",
	"type",
	"usemap",
	"valign",
	"value",
	"valuetype",
	"version",
	"vlink",
	"vspace",
	"width"
};


/* Character entity to unicode character number map. */
/* Keep sorted by name. */
StringInt *chartab;
AsciiInt _chartab[] = {
	{"AElig", 198},
	{"Aacute", 193},
	{"Acirc", 194},
	{"Agrave", 192},
	{"Aring", 197},
	{"Atilde", 195},
	{"Auml", 196},
	{"Ccedil", 199},
	{"ETH", 208},
	{"Eacute", 201},
	{"Ecirc", 202},
	{"Egrave", 200},
	{"Euml", 203},
	{"Iacute", 205},
	{"Icirc", 206},
	{"Igrave", 204},
	{"Iuml", 207},
	{"Ntilde", 209},
	{"Oacute", 211},
	{"Ocirc", 212},
	{"Ograve", 210},
	{"Oslash", 216},
	{"Otilde", 213},
	{"Ouml", 214},
	{"THORN", 222},
	{"Uacute", 218},
	{"Ucirc", 219},
	{"Ugrave", 217},
	{"Uuml", 220},
	{"Yacute", 221},
	{"aacute", 225},
	{"acirc", 226},
	{"acute", 180},
	{"aelig", 230},
	{"agrave", 224},
	{"alpha", 945},
	{"amp", 38},
	{"aring", 229},
	{"atilde", 227},
	{"auml", 228},
	{"beta", 946},
	{"brvbar", 166},
	{"ccedil", 231},
	{"cdots", 8943},
	{"cedil", 184},
	{"cent", 162},
	{"chi", 967},
	{"copy", 169},
	{"curren", 164},
	{"ddots", 8945},
	{"deg", 176},
	{"delta", 948},
	{"divide", 247},
	{"eacute", 233},
	{"ecirc", 234},
	{"egrave", 232},
	{"emdash", 8212},	/* non-standard but commonly used */
	{"emsp", 8195},
	{"endash", 8211},	/* non-standard but commonly used */
	{"ensp", 8194},
	{"epsilon", 949},
	{"eta", 951},
	{"eth", 240},
	{"euml", 235},
	{"frac12", 189},
	{"frac14", 188},
	{"frac34", 190},
	{"gamma", 947},
	{"gt", 62},
	{"iacute", 237},
	{"icirc", 238},
	{"iexcl", 161},
	{"igrave", 236},
	{"iota", 953},
	{"iquest", 191},
	{"iuml", 239},
	{"kappa", 954},
	{"lambda", 955},
	{"laquo", 171},
	{"ldquo", 8220},
	{"ldots", 8230},
	{"lsquo", 8216},
	{"lt", 60},
	{"macr", 175},
	{"mdash", 8212},
	{"micro", 181},
	{"middot", 183},
	{"mu", 956},
	{"nbsp", 160},
	{"ndash", 8211},
	{"not", 172},
	{"ntilde", 241},
	{"nu", 957},
	{"oacute", 243},
	{"ocirc", 244},
	{"ograve", 242},
	{"omega", 969},
	{"omicron", 959},
	{"ordf", 170},
	{"ordm", 186},
	{"oslash", 248},
	{"otilde", 245},
	{"ouml", 246},
	{"para", 182},
	{"phi", 966},
	{"pi", 960},
	{"plusmn", 177},
	{"pound", 163},
	{"psi", 968},
	{"quad", 8193},
	{"quot", 34},
	{"raquo", 187},
	{"rdquo", 8221},
	{"reg", 174},
	{"rho", 961},
	{"rsquo", 8217},
	{"sect", 167},
	{"shy", 173},
	{"sigma", 963},
	{"sp", 8194},
	{"sup1", 185},
	{"sup2", 178},
	{"sup3", 179},
	{"szlig", 223},
	{"tau", 964},
	{"theta", 952},
	{"thinsp", 8201},
	{"thorn", 254},
	{"times", 215},
	{"trade", 8482},
	{"uacute", 250},
	{"ucirc", 251},
	{"ugrave", 249},
	{"uml", 168},
	{"upsilon", 965},
	{"uuml", 252},
	{"varepsilon", 8712},
	{"varphi", 981},
	{"varpi", 982},
	{"varrho", 1009},
	{"vdots", 8942},
	{"vsigma", 962},
	{"vtheta", 977},
	{"xi", 958},
	{"yacute", 253},
	{"yen", 165},
	{"yuml", 255},
	{"zeta", 950}
};
#define NCHARTAB (sizeof(_chartab)/sizeof(_chartab[0]))

/* Characters Winstart..Winend are those that Windows */
/* uses interpolated into the Latin1 set. */
/* They aren't supposed to appear in HTML, but they do.... */
enum {
	Winstart = 127,
	Winend = 159
};

static int	winchars[]= { 8226,	/* 8226 is a bullet */
	8226, 8226, 8218, 402, 8222, 8230, 8224, 8225,
	710, 8240, 352, 8249, 338, 8226, 8226, 8226,
	8226, 8216, 8217, 8220, 8221, 8226, 8211, 8212,
	732, 8482, 353, 8250, 339, 8226, 8226, 376};

static StringInt*	tagtable;		/* initialized from tagnames */
static StringInt*	attrtable;		/* initialized from attrnames */

static void		lexinit(void);
static int		getplaindata(TokenSource* ts, Token* a, int* pai);
static int		getdata(TokenSource* ts, int firstc, int starti, Token* a, int* pai);
static int		getscriptdata(TokenSource* ts, int firstc, int starti, Token* a, int* pai);
static int		gettag(TokenSource* ts, int starti, Token* a, int* pai);
static Rune*		buftostr(Rune* s, Rune* buf, int j);
static int		comment(TokenSource* ts);
static int		findstr(TokenSource* ts, Rune* s);
static int		ampersand(TokenSource* ts);
/*static int		lowerc(int c); */
static int		getchar(TokenSource* ts);
static void		ungetchar(TokenSource* ts, int c);
static void		backup(TokenSource* ts, int savei);
/*static void		freeinsidetoken(Token* t); */
static void		freeattrs(Attr* ahead);
static Attr*		newattr(int attid, Rune* value, Attr* link);
static int		Tconv(Fmt* f);

int	dbglex = 0;
static int lexinited = 0;

static void
lexinit(void)
{
	chartab = _cvtstringinttab(_chartab, nelem(_chartab));
	tagnames = _cvtstringtab(_tagnames, nelem(_tagnames));
	tagtable = _makestrinttab(tagnames, Numtags);
	attrnames = _cvtstringtab(_attrnames, nelem(_attrnames));
	attrtable = _makestrinttab(attrnames, Numattrs);
	fmtinstall('T', Tconv);
	lexinited = 1;
}

static TokenSource*
newtokensource(uchar* data, int edata, int chset, int mtype)
{
	TokenSource*	ans;

	assert(chset == US_Ascii || chset == ISO_8859_1 ||
			chset == UTF_8 || chset == Unicode);
	ans = (TokenSource*)emalloc(sizeof(TokenSource));
	ans->i = 0;
	ans->data = data;
	ans->edata = edata;
	ans->chset = chset;
	ans->mtype = mtype;
	return ans;
}

enum {
	ToksChunk = 500
};

/* Call this to get the tokens. */
/*  The number of returned tokens is returned in *plen. */
Token*
_gettoks(uchar* data, int datalen, int chset, int mtype, int* plen)
{
	TokenSource*	ts;
	Token*		a;
	int	alen;
	int	ai;
	int	starti;
	int	c;
	int	tag;

	if(!lexinited)
		lexinit();
	ts = newtokensource(data, datalen, chset, mtype);
	alen = ToksChunk;
	a = (Token*)emalloc(alen * sizeof(Token));
	ai = 0;
	if(dbglex)
		fprint(2, "_gettoks starts, ts.i=%d, ts.edata=%d\n", ts->i, ts->edata);
	if(ts->mtype == TextHtml){
		for(;;){
			if(ai == alen){
				a = (Token*)erealloc(a, (alen+ToksChunk)*sizeof(Token));
				alen += ToksChunk;
			}
			starti = ts->i;
			c = getchar(ts);
			if(c < 0)
				break;
			if(c == '<'){
				tag = gettag(ts, starti, a, &ai);
				if(tag == Tscript){
					/* special rules for getting Data after.... */
					starti = ts->i;
					c = getchar(ts);
					tag = getscriptdata(ts, c, starti, a, &ai);
				}
			}
			else
				tag = getdata(ts, c, starti, a, &ai);
			if(tag == -1)
				break;
			else if(dbglex > 1 && tag != Comment)
				fprint(2, "lex: got token %T\n", &a[ai-1]);
		}
	}
	else {
		/* plain text (non-html) tokens */
		for(;;){
			if(ai == alen){
				a = (Token*)erealloc(a, (alen+ToksChunk)*sizeof(Token));
				alen += ToksChunk;
			}
			tag = getplaindata(ts, a, &ai);
			if(tag == -1)
				break;
			if(dbglex > 1)
				fprint(2, "lex: got token %T\n", &a[ai]);
		}
	}
	if(dbglex)
		fprint(2, "lex: returning %d tokens\n", ai);
	*plen = ai;
	free(ts);
	if(ai == 0) {
		free(a);
		return nil;
	}
	return a;
}

/* For case where source isn't HTML. */
/* Just make data tokens, one per line (or partial line, */
/* at end of buffer), ignoring non-whitespace control */
/* characters and dumping \r's. */
/* If find non-empty token, fill in a[*pai], bump *pai, and return Data. */
/* Otherwise return -1; */
static int
getplaindata(TokenSource* ts, Token* a, int* pai)
{
	Rune*	s;
	int	j;
	int	starti;
	int	c;
	Token*	tok;
	Rune	buf[BIGBUFSIZE];

	s = nil;
	j = 0;
	starti = ts->i;
	for(c = getchar(ts); c >= 0; c = getchar(ts)){
		if(c < ' '){
			if(isspace(c)){
				if(c == '\r'){
					/* ignore it unless no following '\n', */
					/* in which case treat it like '\n' */
					c = getchar(ts);
					if(c != '\n'){
						if(c >= 0)
							ungetchar(ts, c);
						c = '\n';
					}
				}
			}
			else
				c = 0;
		}
		if(c != 0){
			buf[j++] = c;
			if(j == BIGBUFSIZE-1){
				s = buftostr(s, buf, j);
				j = 0;
			}
		}
		if(c == '\n')
			break;
	}
	s = buftostr(s, buf, j);
	if(s == nil)
		return -1;
	tok = &a[(*pai)++];
	tok->tag = Data;
	tok->text = s;
	tok->attr = nil;
	tok->starti = starti;
	return Data;
}

/* Return concatenation of s and buf[0:j] */
/* Frees s. */
static Rune*
buftostr(Rune* s, Rune* buf, int j)
{
	Rune *tmp;
	buf[j] = 0;
	if(s == nil)
		tmp = _Strndup(buf, j);
	else
		tmp = _Strdup2(s, buf);
	free(s);
	return tmp;
}

/* Gather data up to next start-of-tag or end-of-buffer. */
/* Translate entity references (&amp;). */
/* Ignore non-whitespace control characters and get rid of \r's. */
/* If find non-empty token, fill in a[*pai], bump *pai, and return Data. */
/* Otherwise return -1; */
static int
getdata(TokenSource* ts, int firstc, int starti, Token* a, int* pai)
{
	Rune*	s;
	int	j;
	int	c;
	Token*	tok;
	Rune	buf[BIGBUFSIZE];

	s = nil;
	j = 0;
	c = firstc;
	while(c >= 0){
		if(c == '&'){
			c = ampersand(ts);
			if(c < 0)
				break;
		}
		else if(c < ' '){
			if(isspace(c)){
				if(c == '\r'){
					/* ignore it unless no following '\n', */
					/* in which case treat it like '\n' */
					c = getchar(ts);
					if(c != '\n'){
						if(c >= 0)
							ungetchar(ts, c);
						c = '\n';
					}
				}
			}
			else {
				if(warn)
					fprint(2, "warning: non-whitespace control character %d ignored\n", c);
				c = 0;
			}
		}
		else if(c == '<'){
			ungetchar(ts, c);
			break;
		}
		if(c != 0){
			buf[j++] = c;
			if(j == BIGBUFSIZE-1){
				s = buftostr(s, buf, j);
				j = 0;
			}
		}
		c = getchar(ts);
	}
	s = buftostr(s, buf, j);
	if(s == nil)
		return -1;
	tok = &a[(*pai)++];
	tok->tag = Data;
	tok->text = s;
	tok->attr = nil;
	tok->starti = starti;
	return Data;
}

/* The rules for lexing scripts are different (ugh). */
/* Gather up everything until see a </SCRIPT>. */
static int
getscriptdata(TokenSource* ts, int firstc, int starti, Token* a, int* pai)
{
	Rune*	s;
	int	j;
	int	tstarti;
	int	savei;
	int	c;
	int	tag;
	int	done;
	Token*	tok;
	Rune	buf[BIGBUFSIZE];

	s = nil;
	j = 0;
	tstarti = starti;
	c = firstc;
	done = 0;
	while(c >= 0){
		if(c == '<'){
			/* other browsers ignore stuff to end of line after <! */
			savei = ts->i;
			c = getchar(ts);
			if(c == '!'){
				while(c >= 0 && c != '\n' && c != '\r')
					c = getchar(ts);
				if(c == '\r')
					c = getchar(ts);
				if(c == '\n')
					c = getchar(ts);
			}
			else if(c >= 0){
				backup(ts, savei);
				tag = gettag(ts, tstarti, a, pai);
				if(tag == -1)
					break;
				if(tag != Comment)
					(*pai)--;
				backup(ts, tstarti);
				if(tag == Tscript + RBRA){
					done = 1;
					break;
				}
				/* here tag was not </SCRIPT>, so take as regular data */
				c = getchar(ts);
			}
		}
		if(c < 0)
			break;
		if(c != 0){
			buf[j++] = c;
			if(j == BIGBUFSIZE-1){
				s = buftostr(s, buf, j);
				j = 0;
			}
		}
		tstarti = ts->i;
		c = getchar(ts);
	}
	if(done || ts->i == ts->edata){
		s = buftostr(s, buf, j);
		tok = &a[(*pai)++];
		tok->tag = Data;
		tok->text = s;
		tok->attr = nil;
		tok->starti = starti;
		return Data;
	}
	backup(ts, starti);
	return -1;
}

/* We've just seen a '<'.  Gather up stuff to closing '>' (if buffer */
/* ends before then, return -1). */
/* If it's a tag, look up the name, gather the attributes, and return */
/* the appropriate token. */
/* Else it's either just plain data or some kind of ignorable stuff: */
/* return Data or Comment as appropriate. */
/* If it's not a Comment, put it in a[*pai] and bump *pai. */
static int
gettag(TokenSource* ts, int starti, Token* a, int* pai)
{
	int	rbra;
	int	ans;
	Attr*	al;
	int	nexti;
	int	c;
	int	ti;
	int	afnd;
	int	attid;
	int	quote;
	Rune*	val;
	int	nv;
	int	i;
	int	tag;
	Token*	tok;
	Rune	buf[BIGBUFSIZE];

	rbra = 0;
	nexti = ts->i;
	tok = &a[*pai];
	tok->tag = Notfound;
	tok->text = nil;
	tok->attr = nil;
	tok->starti = starti;
	c = getchar(ts);
	if(c == '/'){
		rbra = RBRA;
		c = getchar(ts);
	}
	if(c < 0)
		goto eob_done;
	if(c >= 256 || !isalpha(c)){
		/* not a tag */
		if(c == '!'){
			ans = comment(ts);
			if(ans != -1)
				return ans;
			goto eob_done;
		}
		else {
			backup(ts, nexti);
			tok->tag = Data;
			tok->text = _Strdup(L(Llt));
			(*pai)++;
			return Data;
		}
	}
	/* c starts a tagname */
	buf[0] = c;
	i = 1;
	for(;;){
		c = getchar(ts);
		if(c < 0)
			goto eob_done;
		if(!ISNAMCHAR(c))
			break;
		/* if name is bigger than buf it won't be found anyway... */
		if(i < BIGBUFSIZE)
			buf[i++] = c;
	}
	if(_lookup(tagtable, Numtags, buf, i, &tag))
		tok->tag = tag + rbra;
	else
		tok->text = _Strndup(buf, i);	/* for warning print, in build */

	/* attribute gathering loop */
	al = nil;
	for(;;){
		/* look for "ws name" or "ws name ws = ws val"  (ws=whitespace) */
		/* skip whitespace */
attrloop_continue:
		while(c < 256 && isspace(c)){
			c = getchar(ts);
			if(c < 0)
				goto eob_done;
		}
		if(c == '>')
			goto attrloop_done;
		if(c == '<'){
			if(warn)
				fprint(2, "warning: unclosed tag\n");
			ungetchar(ts, c);
			goto attrloop_done;
		}
		if(c >= 256 || !isalpha(c)){
			if(warn)
				fprint(2, "warning: expected attribute name\n");
			/* skipt to next attribute name */
			for(;;){
				c = getchar(ts);
				if(c < 0)
					goto eob_done;
				if(c < 256 && isalpha(c))
					goto attrloop_continue;
				if(c == '<'){
					if(warn)
						fprint(2, "warning: unclosed tag\n");
					ungetchar(ts, 60);
					goto attrloop_done;
				}
				if(c == '>')
					goto attrloop_done;
			}
		}
		/* gather attribute name */
		buf[0] = c;
		i = 1;
		for(;;){
			c = getchar(ts);
			if(c < 0)
				goto eob_done;
			if(!ISNAMCHAR(c))
				break;
			if(i < BIGBUFSIZE-1)
				buf[i++] = c;
		}
		afnd = _lookup(attrtable, Numattrs, buf, i, &attid);
		if(warn && !afnd){
			buf[i] = 0;
			fprint(2, "warning: unknown attribute name %S\n", buf);
		}
		/* skip whitespace */
		while(c < 256 && isspace(c)){
			c = getchar(ts);
			if(c < 0)
				goto eob_done;
		}
		if(c != '='){
			if(afnd)
				al = newattr(attid, nil, al);
			goto attrloop_continue;
		}
		/*# c is '=' here;  skip whitespace */
		for(;;){
			c = getchar(ts);
			if(c < 0)
				goto eob_done;
			if(c >= 256 || !isspace(c))
				break;
		}
		quote = 0;
		if(c == '\'' || c == '"'){
			quote = c;
			c = getchar(ts);
			if(c < 0)
				goto eob_done;
		}
		val = nil;
		nv = 0;
		for(;;){
valloop_continue:
			if(c < 0)
				goto eob_done;
			if(c == '>'){
				if(quote){
					/* c might be part of string (though not good style) */
					/* but if line ends before close quote, assume */
					/* there was an unmatched quote */
					ti = ts->i;
					for(;;){
						c = getchar(ts);
						if(c < 0)
							goto eob_done;
						if(c == quote){
							backup(ts, ti);
							buf[nv++] = '>';
							if(nv == BIGBUFSIZE-1){
								val = buftostr(val, buf, nv);
								nv = 0;
							}
							c = getchar(ts);
							goto valloop_continue;
						}
						if(c == '\n'){
							if(warn)
								fprint(2, "warning: apparent unmatched quote\n");
							backup(ts, ti);
							c = '>';
							goto valloop_done;
						}
					}
				}
				else
					goto valloop_done;
			}
			if(quote){
				if(c == quote){
					c = getchar(ts);
					if(c < 0)
						goto eob_done;
					goto valloop_done;
				}
				if(c == '\r'){
					c = getchar(ts);
					goto valloop_continue;
				}
				if(c == '\t' || c == '\n')
					c = ' ';
			}
			else {
				if(c < 256 && isspace(c))
					goto valloop_done;
			}
			if(c == '&'){
				c = ampersand(ts);
				if(c == -1)
					goto eob_done;
			}
			buf[nv++] = c;
			if(nv == BIGBUFSIZE-1){
				val = buftostr(val, buf, nv);
				nv = 0;
			}
			c = getchar(ts);
		}
valloop_done:
		if(afnd){
			val = buftostr(val, buf, nv);
			al = newattr(attid, val, al);
		}
	}

attrloop_done:
	tok->attr = al;
	(*pai)++;
	return tok->tag;

eob_done:
	if(warn)
		fprint(2, "warning: incomplete tag at end of page\n");
	backup(ts, nexti);
	tok->tag = Data;
	tok->text = _Strdup(L(Llt));
	return Data;
}

/* We've just read a '<!' at position starti, */
/* so this may be a comment or other ignored section, or it may */
/* be just a literal string if there is no close before end of file */
/* (other browsers do that). */
/* The accepted practice seems to be (note: contrary to SGML spec!): */
/* If see <!--, look for --> to close, or if none, > to close. */
/* If see <!(not --), look for > to close. */
/* If no close before end of file, leave original characters in as literal data. */
/* */
/* If we see ignorable stuff, return Comment. */
/* Else return nil (caller should back up and try again when more data arrives, */
/* unless at end of file, in which case caller should just make '<' a data token). */
static int
comment(TokenSource* ts)
{
	int	nexti;
	int	havecomment;
	int	c;

	nexti = ts->i;
	havecomment = 0;
	c = getchar(ts);
	if(c == '-'){
		c = getchar(ts);
		if(c == '-'){
			if(findstr(ts, L(Larrow)))
				havecomment = 1;
			else
				backup(ts, nexti);
		}
	}
	if(!havecomment){
		if(c == '>')
			havecomment = 1;
		else if(c >= 0){
			if(findstr(ts, L(Lgt)))
				havecomment = 1;
		}
	}
	if(havecomment)
		return Comment;
	return -1;
}

/* Look for string s in token source. */
/* If found, return 1, with buffer at next char after s, */
/* else return 0 (caller should back up). */
static int
findstr(TokenSource* ts, Rune* s)
{
	int	c0;
	int	n;
	int	nexti;
	int	i;
	int	c;

	c0 = s[0];
	n = runestrlen(s);
	for(;;){
		c = getchar(ts);
		if(c < 0)
			break;
		if(c == c0){
			if(n == 1)
				return 1;
			nexti = ts->i;
			for(i = 1; i < n; i++){
				c = getchar(ts);
				if(c < 0)
					goto mainloop_done;
				if(c != s[i])
					break;
			}
			if(i == n)
				return 1;
			backup(ts, nexti);
		}
	}
mainloop_done:
	return 0;
}

static int
xdigit(int c)
{
	if('0' <= c && c <= '9')
		return c-'0';
	if('a' <= c && c <= 'f')
		return c-'a'+10;
	if('A' <= c && c <= 'F')
		return c-'A'+10;
	return -1;
}

/* We've just read an '&'; look for an entity reference */
/* name, and if found, return translated char. */
/* if there is a complete entity name but it isn't known, */
/* try prefixes (gets around some buggy HTML out there), */
/* and if that fails, back up to just past the '&' and return '&'. */
/* If the entity can't be completed in the current buffer, back up */
/* to the '&' and return -1. */
static int
ampersand(TokenSource* ts)
{
	int	savei;
	int	c;
	int	fnd;
	int	ans;
	int	v;
	int	i;
	int	k;
	Rune	buf[SMALLBUFSIZE];

	savei = ts->i;
	c = getchar(ts);
	fnd = 0;
	ans = -1;
	if(c == '#'){
		c = getchar(ts);
		v = 0;
		if(c == 'x'){
			c = getchar(ts);
			while((i=xdigit(c)) != -1){
				v = v*16 + i;
				c = getchar(ts);
			}
		}else{
			while('0' <= c && c <= '9'){
				v = v*10 + c - '0';
				c = getchar(ts);
			}
		}
		if(c >= 0){
			if(!(c == ';' || c == '\n' || c == '\r'))
				ungetchar(ts, c);
			c = v;
			if(c == 160)
				c = 160;
			if(c >= Winstart && c <= Winend){
				c = winchars[c - Winstart];
			}
			ans = c;
			fnd = 1;
		}
	}
	else if(c < 256 && isalpha(c)){
		buf[0] = c;
		k = 1;
		for(;;){
			c = getchar(ts);
			if(c < 0)
				break;
			if(ISNAMCHAR(c)){
				if(k < SMALLBUFSIZE-1)
					buf[k++] = c;
			}
			else {
				if(!(c == ';' || c == '\n' || c == '\r'))
					ungetchar(ts, c);
				break;
			}
		}
		if(c >= 0){
			fnd = _lookup(chartab, NCHARTAB, buf, k, &ans);
			if(!fnd){
				/* Try prefixes of s */
				if(c == ';' || c == '\n' || c == '\r')
					ungetchar(ts, c);
				i = k;
				while(--k > 0){
					fnd = _lookup(chartab, NCHARTAB, buf, k, &ans);
					if(fnd){
						while(i > k){
							i--;
							ungetchar(ts, buf[i]);
						}
						break;
					}
				}
			}
		}
	}
	if(!fnd){
		backup(ts, savei);
		ans = '&';
	}
	return ans;
}

/* Get next char, obeying ts.chset. */
/* Returns -1 if no complete character left before current end of data. */
static int
getchar(TokenSource* ts)
{
	uchar*	buf;
	int	c;
	int	n;
	int	ok;
	Rune	r;

	if(ts->i >= ts->edata)
		return -1;
	buf = ts->data;
	c = buf[ts->i];
	switch(ts->chset){
	case ISO_8859_1:
		if(c >= Winstart && c <= Winend)
			c = winchars[c - Winstart];
		ts->i++;
		break;
	case US_Ascii:
		if(c > 127){
			if(warn)
				fprint(2, "non-ascii char (%x) when US-ASCII specified\n", c);
		}
		ts->i++;
		break;
	case UTF_8:
		ok = fullrune((char*)(buf+ts->i), ts->edata-ts->i);
		n = chartorune(&r, (char*)(buf+ts->i));
		if(ok){
			if(warn && c == 0x80)
				fprint(2, "warning: invalid utf-8 sequence (starts with %x)\n", ts->data[ts->i]);
			ts->i += n;
			c = r;
		}
		else {
			/* not enough bytes in buf to complete utf-8 char */
			ts->i = ts->edata;	/* mark "all used" */
			c = -1;
		}
		break;
	case Unicode:
		if(ts->i < ts->edata - 1){
			/*standards say most-significant byte first */
			c = (c << 8)|(buf[ts->i + 1]);
			ts->i += 2;
		}
		else {
			ts->i = ts->edata;	/* mark "all used" */
			c = -1;
		}
		break;
	}
	return c;
}

/* Assuming c was the last character returned by getchar, set */
/* things up so that next getchar will get that same character */
/* followed by the current 'next character', etc. */
static void
ungetchar(TokenSource* ts, int c)
{
	int	n;
	Rune	r;
	char	a[UTFmax];

	n = 1;
	switch(ts->chset){
	case UTF_8:
		if(c >= 128){
			r = c;
			n = runetochar(a, &r);
		}
		break;
	case Unicode:
		n = 2;
		break;
	}
	ts->i -= n;
}

/* Restore ts so that it is at the state where the index was savei. */
static void
backup(TokenSource* ts, int savei)
{
	if(dbglex)
		fprint(2, "lex: backup; i=%d, savei=%d\n", ts->i, savei);
	ts->i = savei;
}


/* Look for value associated with attribute attid in token t. */
/* If there is one, return 1 and put the value in *pans, */
/* else return 0. */
/* If xfer is true, transfer ownership of the string to the caller */
/* (nil it out here); otherwise, caller must duplicate the answer */
/* if it needs to save it. */
/* OK to have pans==0, in which case this is just looking */
/* to see if token is present. */
int
_tokaval(Token* t, int attid, Rune** pans, int xfer)
{
	Attr*	attr;

	attr = t->attr;
	while(attr != nil){
		if(attr->attid == attid){
			if(pans != nil)
				*pans = attr->value;
			if(xfer)
				attr->value = nil;
			return 1;
		}
		attr = attr->next;
	}
	if(pans != nil)
		*pans = nil;
	return 0;
}

static int
Tconv(Fmt *f)
{
	Token*	t;
	int	i;
	int	tag;
	char*	srbra;
	Rune*	aname;
	Rune*	tname;
	Attr*	a;
	char	buf[BIGBUFSIZE];

	t = va_arg(f->args, Token*);
	if(t == nil)
		sprint(buf, "<null>");
	else {
		i = 0;
		if(dbglex > 1)
			i = snprint(buf, sizeof(buf), "[%d]", t->starti);
		tag = t->tag;
		if(tag == Data){
			i += snprint(buf+i, sizeof(buf)-i-1, "'%S'", t->text);
		}
		else {
			srbra = "";
			if(tag >= RBRA){
				tag -= RBRA;
				srbra = "/";
			}
			tname = tagnames[tag];
			if(tag == Notfound)
				tname = L(Lquestion);
			i += snprint(buf+i, sizeof(buf)-i-1, "<%s%S", srbra, tname);
			for(a = t->attr; a != nil; a = a->next){
				aname = attrnames[a->attid];
				i += snprint(buf+i, sizeof(buf)-i-1, " %S", aname);
				if(a->value != nil)
					i += snprint(buf+i, sizeof(buf)-i-1, "=%S", a->value);
			}
			i += snprint(buf+i, sizeof(buf)-i-1, ">");
		}
		buf[i] = 0;
	}
	return fmtstrcpy(f, buf);
}

/* Attrs own their constituent strings, but build may eventually */
/* transfer some values to its items and nil them out in the Attr. */
static Attr*
newattr(int attid, Rune* value, Attr* link)
{
	Attr* ans;

	ans = (Attr*)emalloc(sizeof(Attr));
	ans->attid = attid;
	ans->value = value;
	ans->next = link;
	return ans;
}

/* Free list of Attrs linked through next field */
static void
freeattrs(Attr* ahead)
{
	Attr* a;
	Attr* nexta;

	a = ahead;
	while(a != nil){
		nexta = a->next;
		free(a->value);
		free(a);
		a = nexta;
	}
}

/* Free array of Tokens. */
/* Allocated space might have room for more than n tokens, */
/* but only n of them are initialized. */
/* If caller has transferred ownership of constitutent strings */
/* or attributes, it must have nil'd out the pointers in the Tokens. */
void
_freetokens(Token* tarray, int n)
{
	int i;
	Token* t;

	if(tarray == nil)
		return;
	for(i = 0; i < n; i++){
		t = &tarray[i];
		free(t->text);
		freeattrs(t->attr);
	}
	free(tarray);
}
