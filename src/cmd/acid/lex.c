#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mach.h>
#define Extern extern
#include "acid.h"
#include "y.tab.h"

struct keywd
{
	char	*name;
	int	terminal;
}
keywds[] =
{
	"do",		Tdo,
	"if",		Tif,
	"then",		Tthen,
	"else",		Telse,
	"while",	Twhile,
	"loop",		Tloop,
	"head",		Thead,
	"tail",		Ttail,
	"append",	Tappend,
	"defn",		Tfn,
	"return",	Tret,
	"local",	Tlocal,
	"aggr",		Tcomplex,
	"union",	Tcomplex,
	"adt",		Tcomplex,
	"complex",	Tcomplex,
	"delete",	Tdelete,
	"whatis",	Twhat,
	"eval",		Teval,
	"builtin",	Tbuiltin,
	0,		0
};

char cmap[256];

void
initcmap(void)
{
	cmap['0']=	'\0'+1;
	cmap['n']=	'\n'+1;
	cmap['r']=	'\r'+1;
	cmap['t']=	'\t'+1;
	cmap['b']=	'\b'+1;
	cmap['f']=	'\f'+1;
	cmap['a']=	'\a'+1;
	cmap['v']=	'\v'+1;
	cmap['\\']=	'\\'+1;
	cmap['"']=	'"'+1;
}

void
kinit(void)
{
	int i;

	initcmap();

	for(i = 0; keywds[i].name; i++)
		enter(keywds[i].name, keywds[i].terminal);
}

typedef struct IOstack IOstack;
struct IOstack
{
	char	*name;
	int	line;
	char	*text;
	char	*ip;
	Biobuf	*fin;
	IOstack	*prev;
};
IOstack *lexio;
uint nlexio;

void
setacidfile(void)
{
	char *name;
	Lsym *l;

	if(lexio)
		name = lexio->name;
	else
		name = "";
	l = mkvar("acidfile");
	l->v->set = 1;
	l->v->store.fmt = 's';
	l->v->type = TSTRING;
	l->v->store.u.string = strnode(name);
}

void
pushfile(char *file)
{
	Biobuf *b;
	IOstack *io;

	if(nlexio > 64)
		error("too many includes");

	if(file)
		b = Bopen(file, OREAD);
	else{
		b = Bopen(unsharp("#d/0"), OREAD);
		file = "<stdin>";
	}

	if(b == 0)
		error("pushfile: %s: %r", file);

	io = malloc(sizeof(IOstack));
	if(io == 0)
		fatal("no memory");
	io->name = strdup(file);
	if(io->name == 0)
		fatal("no memory");
	io->line = line;
	line = 1;
	io->text = 0;
	io->fin = b;
	io->prev = lexio;
	lexio = io;
	nlexio++;
	setacidfile();
}

void
pushfd(int fd)
{
	pushfile("/dev/null");
	close(lexio->fin->fid);
	free(lexio->name);
	lexio->name = smprint("<fd#d>", fd);
	lexio->fin->fid = fd;
}

void
pushstr(Node *s)
{
	IOstack *io;

	io = malloc(sizeof(IOstack));
	if(io == 0)
		fatal("no memory");
	io->line = line;
	line = 1;
	io->name = strdup("<string>");
	if(io->name == 0)
		fatal("no memory");
	io->line = line;
	line = 1;
	io->text = strdup(s->store.u.string->string);
	if(io->text == 0)
		fatal("no memory");
	io->ip = io->text;
	io->fin = 0;
	io->prev = lexio;
	nlexio++;
	lexio = io;
	setacidfile();
}

void
restartio(void)
{
	Bflush(lexio->fin);
	Binit(lexio->fin, 0, OREAD);
}

int
popio(void)
{
	IOstack *s;

	if(lexio == 0)
		return 0;

	if(lexio->prev == 0){
		if(lexio->fin)
			restartio();
		return 0;
	}

	if(lexio->fin)
		Bterm(lexio->fin);
	else
		free(lexio->text);
	free(lexio->name);
	line = lexio->line;
	s = lexio;
	lexio = s->prev;
	free(s);
	nlexio--;
	setacidfile();
	return 1;
}

int
Zfmt(Fmt *f)
{
	char buf[1024], *p;
	IOstack *e;

	e = lexio;
	if(e) {
		p = seprint(buf, buf+sizeof buf, "%s:%d", e->name, line);
		while(e->prev) {
			e = e->prev;
			if(initialising && e->prev == 0)
				break;
			p = seprint(p, buf+sizeof buf, " [%s:%d]", e->name, e->line);
		}
	} else
		sprint(buf, "no file:0");
	fmtstrcpy(f, buf);
	return 0;
}

void
unlexc(int s)
{
	if(s == '\n')
		line--;

	if(lexio->fin)
		Bungetc(lexio->fin);
	else
		lexio->ip--;
}

int
lexc(void)
{
	int c;

	if(lexio->fin) {
		c = Bgetc(lexio->fin);
		if(gotint)
			error("interrupt");
		return c;
	}

	c = *lexio->ip++;
	if(c == 0)
		return -1;
	return c;
}

int
escchar(int c)
{
	int n;
	char buf[Strsize];

	if(c >= '0' && c <= '9') {
		n = 1;
		buf[0] = c;
		for(;;) {
			c = lexc();
			if(c == Eof)
				error("%d: <eof> in escape sequence", line);
			if(strchr("0123456789xX", c) == 0) {
				unlexc(c);
				break;
			}
			buf[n++] = c;
		}
		buf[n] = '\0';
		return strtol(buf, 0, 0);
	}

	n = cmap[(unsigned char)c];
	if(n == 0)
		return c;
	return n-1;
}

void
eatstring(void)
{
	int esc, c, cnt;
	char buf[Strsize];

	esc = 0;
	for(cnt = 0;;) {
		c = lexc();
		switch(c) {
		case Eof:
			error("%d: <eof> in string constant", line);

		case '\n':
			error("newline in string constant");
			goto done;

		case '\\':
			if(esc)
				goto Default;
			esc = 1;
			break;

		case '"':
			if(esc == 0)
				goto done;

			/* Fall through */
		default:
		Default:
			if(esc) {
				c = escchar(c);
				esc = 0;
			}
			buf[cnt++] = c;
			break;
		}
		if(cnt >= Strsize)
			error("string token too long");
	}
done:
	buf[cnt] = '\0';
	yylval.string = strnode(buf);
}

void
eatnl(void)
{
	int c;

	line++;
	for(;;) {
		c = lexc();
		if(c == Eof)
			error("eof in comment");
		if(c == '\n')
			return;
	}
}

int
bqsymbol(void)
{
	int c;
	char *p;
	Lsym *s;

	p = symbol;
	while((c = lexc()) != '`'){
		if(c == Eof)
			error("eof in backquote");
		if(c == '\n')
			error("newline in backquote");
		*p++ = c;
	}
	if(p >= symbol+sizeof symbol)
		sysfatal("overflow in bqsymbol");
	*p = 0;

	s = look(symbol);
	if(s == 0)
		s = enter(symbol, Tid);
	yylval.sym = s;
	return s->lexval;
}

int
yylex(void)
{
	int c;
	extern char vfmt[];

loop:
	Bflush(bout);
	c = lexc();
	switch(c) {
	case Eof:
		if(gotint) {
			gotint = 0;
			stacked = 0;
			Bprint(bout, "\nacid; ");
			goto loop;
		}
		return Eof;

	case '`':
		return bqsymbol();

	case '"':
		eatstring();
		return Tstring;

	case ' ':
	case '\t':
		goto loop;

	case '\n':
		line++;
		if(interactive == 0)
			goto loop;
		if(stacked) {
			print("\t");
			goto loop;
		}
		nlcount++;
		return ';';

	case '.':
		c = lexc();
		unlexc(c);
		if(isdigit(c))
			return numsym('.');

		return '.';

	case '(':
	case ')':
	case '[':
	case ']':
	case ';':
	case ':':
	case ',':
	case '~':
	case '?':
	case '*':
	case '@':
	case '^':
	case '%':
		return c;
	case '{':
		stacked++;
		return c;
	case '}':
		stacked--;
		return c;

	case '\\':
		c = lexc();
		if(strchr(vfmt, c) == 0) {
			unlexc(c);
			return '\\';
		}
		yylval.ival = c;
		return Tfmt;

	case '!':
		c = lexc();
		if(c == '=')
			return Tneq;
		unlexc(c);
		return '!';

	case '+':
		c = lexc();
		if(c == '+')
			return Tinc;
		unlexc(c);
		return '+';

	case '/':
		c = lexc();
		if(c == '/') {
			eatnl();
			goto loop;
		}
		unlexc(c);
		return '/';

	case '\'':
		c = lexc();
		if(c == '\\')
			yylval.ival = escchar(lexc());
		else
			yylval.ival = c;
		c = lexc();
		if(c != '\'') {
			error("missing '");
			unlexc(c);
		}
		return Tconst;

	case '&':
		c = lexc();
		if(c == '&')
			return Tandand;
		unlexc(c);
		return '&';

	case '=':
		c = lexc();
		if(c == '=')
			return Teq;
		unlexc(c);
		return '=';

	case '|':
		c = lexc();
		if(c == '|')
			return Toror;
		unlexc(c);
		return '|';

	case '<':
		c = lexc();
		if(c == '=')
			return Tleq;
		if(c == '<')
			return Tlsh;
		unlexc(c);
		return '<';

	case '>':
		c = lexc();
		if(c == '=')
			return Tgeq;
		if(c == '>')
			return Trsh;
		unlexc(c);
		return '>';

	case '-':
		c = lexc();

		if(c == '>')
			return Tindir;

		if(c == '-')
			return Tdec;
		unlexc(c);
		return '-';

	default:
		return numsym(c);
	}
}

int
numsym(char first)
{
	int c, isbin, isfloat, ishex;
	char *sel, *p;
	Lsym *s;

	symbol[0] = first;
	p = symbol;

	ishex = 0;
	isbin = 0;
	isfloat = 0;
	if(first == '.')
		isfloat = 1;

	if(isdigit((uchar)*p++) || isfloat) {
		for(;;) {
			c = lexc();
			if(c < 0)
				error("%d: <eof> eating symbols", line);

			if(c == '\n')
				line++;
			sel = "01234567890.xb";
			if(ishex)
				sel = "01234567890abcdefABCDEF";
			else if(isbin)
				sel = "01";
			else if(isfloat)
				sel = "01234567890eE-+";

			if(strchr(sel, c) == 0) {
				unlexc(c);
				break;
			}
			if(c == '.')
				isfloat = 1;
			if(!isbin && c == 'x')
				ishex = 1;
			if(!ishex && c == 'b')
				isbin = 1;
			*p++ = c;
		}
		*p = '\0';
		if(isfloat) {
			yylval.fval = atof(symbol);
			return Tfconst;
		}

		if(isbin)
			yylval.ival = strtoull(symbol+2, 0, 2);
		else
			yylval.ival = strtoll(symbol, 0, 0);
		return Tconst;
	}

	for(;;) {
		c = lexc();
		if(c < 0)
			error("%d <eof> eating symbols", line);
		if(c == '\n')
			line++;
		/* allow :: in name */
		if(c == ':'){
			c = lexc();
			if(c == ':'){
				*p++ = ':';
				*p++ = ':';
				continue;
			}
			unlexc(c);
			unlexc(':');
			break;
		}
		if(c != '_' && c != '$' && c < Runeself && !isalnum(c)) {
			unlexc(c);
			break;
		}
		*p++ = c;
	}

	*p = '\0';

	s = look(symbol);
	if(s == 0)
		s = enter(symbol, Tid);

	yylval.sym = s;
	return s->lexval;
}

Lsym*
enter(char *name, int t)
{
	Lsym *s;
	ulong h;
	char *p;
	Value *v;

	h = 0;
	for(p = name; *p; p++)
		h = h*3 + *p;
	h %= Hashsize;

	s = gmalloc(sizeof(Lsym));
	memset(s, 0, sizeof(Lsym));
	s->name = strdup(name);

	s->hash = hash[h];
	hash[h] = s;
	s->lexval = t;

	v = gmalloc(sizeof(Value));
	s->v = v;

	v->store.fmt = 'X';
	v->type = TINT;
	memset(v, 0, sizeof(Value));

	return s;
}

void
delsym(Lsym *s)
{
	char *q;
	ulong h;
	Lsym *p;

	h = 0;
	for(q = s->name; *q; q++)
		h = h*3 + *q;
	h %= Hashsize;

	if(hash[h] == s)
		hash[h] = s->hash;
	else{
		for(p=hash[h]; p && p->hash != s; p=p->hash)
			;
		if(p)
			p->hash = s->hash;
	}
	s->hash = nil;
}

Lsym*
look(char *name)
{
	Lsym *s;
	ulong h;
	char *p;

	h = 0;
	for(p = name; *p; p++)
		h = h*3 + *p;
	h %= Hashsize;

	for(s = hash[h]; s; s = s->hash)
		if(strcmp(name, s->name) == 0)
			return s;
	return 0;
}

Lsym*
mkvar(char *s)
{
	Lsym *l;

	l = look(s);
	if(l == 0)
		l = enter(s, Tid);
	return l;
}
