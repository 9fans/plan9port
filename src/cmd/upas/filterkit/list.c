#include <u.h>
#include <libc.h>
#include <regexp.h>
#include <libsec.h>
#include <libString.h>
#include <bio.h>
#include "dat.h"

int debug;

enum
{
	Tregexp=	(1<<0),		/* ~ */
	Texact=		(1<<1),		/* = */
};

typedef struct Pattern Pattern;
struct Pattern
{
	Pattern	*next;
	int	type;
	char	*arg;
	int	bang;
};

String	*patternpath;
Pattern	*patterns;
String	*mbox;

static void
usage(void)
{
	fprint(2, "usage: %s 'check|add' patternfile addr [addr*]\n", argv0);
	exits("usage");
}

/*
 *  convert string to lower case
 */
static void
mklower(char *p)
{
	int c;

	for(; *p; p++){
		c = *p;
		if(c <= 'Z' && c >= 'A')
			*p = c - 'A' + 'a';
	}
}

/*
 *  simplify an address, reduce to a domain
 */
static String*
simplify(char *addr)
{
	int dots;
	char *p, *at;
	String *s;

	mklower(addr);
	at = strchr(addr, '@');
	if(at == nil){
		/* local address, make it an exact match */
		s = s_copy("=");
		s_append(s, addr);
		return s;
	}

	/* copy up to the '@' sign */
	at++;
	s = s_copy("~");
	for(p = addr; p < at; p++){
		if(strchr(".*+?(|)\\[]^$", *p))
			s_putc(s, '\\');
		s_putc(s, *p);
	}

	/* just any address matching the two most significant domain elements */
	s_append(s, "(.*\\.)?");
	p = addr+strlen(addr);
	dots = 0;
	for(; p > at; p--){
		if(*p != '.')
			continue;
		if(dots++ > 0){
			p++;
			break;
		}
	}
	for(; *p; p++){
		if(strchr(".*+?(|)\\[]^$", *p) != 0)
			s_putc(s, '\\');
		s_putc(s, *p);
	}
	s_terminate(s);

	return s;
}

/*
 *  link patterns in order
 */
static int
newpattern(int type, char *arg, int bang)
{
	Pattern *p;
	static Pattern *last;

	mklower(arg);

	p = mallocz(sizeof *p, 1);
	if(p == nil)
		return -1;
	if(type == Tregexp){
		p->arg = malloc(strlen(arg)+3);
		if(p->arg == nil){
			free(p);
			return -1;
		}
		p->arg[0] = 0;
		strcat(p->arg, "^");
		strcat(p->arg, arg);
		strcat(p->arg, "$");
	} else {
		p->arg = strdup(arg);
		if(p->arg == nil){
			free(p);
			return -1;
		}
	}
	p->type = type;
	p->bang = bang;
	if(last == nil)
		patterns = p;
	else
		last->next = p;
	last = p;

	return 0;
}

/*
 *  patterns are either
 *	~ regular expression
 *	= exact match string
 *
 *  all comparisons are case insensitive
 */
static int
readpatterns(char *path)
{
	Biobuf *b;
	char *p;
	char *token[2];
	int n;
	int bang;

	b = Bopen(path, OREAD);
	if(b == nil)
		return -1;
	while((p = Brdline(b, '\n')) != nil){
		p[Blinelen(b)-1] = 0;
		n = tokenize(p, token, 2);
		if(n == 0)
			continue;

		mklower(token[0]);
		p = token[0];
		if(*p == '!'){
			p++;
			bang = 1;
		} else
			bang = 0;

		if(*p == '='){
			if(newpattern(Texact, p+1, bang) < 0)
				return -1;
		} else if(*p == '~'){
			if(newpattern(Tregexp, p+1, bang) < 0)
				return -1;
		} else if(strcmp(token[0], "#include") == 0 && n == 2)
			readpatterns(token[1]);
	}
	Bterm(b);
	return 0;
}

/* fuck, shit, bugger, damn */
void regerror(char *err)
{
	USED(err);
}

/*
 *  check lower case version of address agains patterns
 */
static Pattern*
checkaddr(char *arg)
{
	Pattern *p;
	Reprog *rp;
	String *s;

	s = s_copy(arg);
	mklower(s_to_c(s));

	for(p = patterns; p != nil; p = p->next)
		switch(p->type){
		case Texact:
			if(strcmp(p->arg, s_to_c(s)) == 0){
				free(s);
				return p;
			}
			break;
		case Tregexp:
			rp = regcomp(p->arg);
			if(rp == nil)
				continue;
			if(regexec(rp, s_to_c(s), nil, 0)){
				free(rp);
				free(s);
				return p;
			}
			free(rp);
			break;
		}
	s_free(s);
	return 0;
}
static char*
check(int argc, char **argv)
{
	int i;
	Addr *a;
	Pattern *p;
	int matchedbang;

	matchedbang = 0;
	for(i = 0; i < argc; i++){
		a = readaddrs(argv[i], nil);
		for(; a != nil; a = a->next){
			p = checkaddr(a->val);
			if(p == nil)
				continue;
			if(p->bang)
				matchedbang = 1;
			else
				return nil;
		}
	}
	if(matchedbang)
		return "!match";
	else
		return "no match";
}

/*
 *  add anything that isn't already matched, all matches are lower case
 */
static char*
add(char *pp, int argc, char **argv)
{
	int fd, i;
	String *s;
	char *cp;
	Addr *a;

	a = nil;
	for(i = 0; i < argc; i++)
		a = readaddrs(argv[i], a);

	fd = open(pp, OWRITE);
	seek(fd, 0, 2);
	for(; a != nil; a = a->next){
		if(checkaddr(a->val))
			continue;
		s = simplify(a->val);
		cp = s_to_c(s);
		fprint(fd, "%q\t%q\n", cp, a->val);
		if(*cp == '=')
			newpattern(Texact, cp+1, 0);
		else if(*cp == '~')
			newpattern(Tregexp, cp+1, 0);
		s_free(s);
	}
	close(fd);
	return nil;
}

void
main(int argc, char **argv)
{
	char *patternpath;

	ARGBEGIN {
	case 'd':
		debug++;
		break;
	} ARGEND;

	quotefmtinstall();

	if(argc < 3)
		usage();

	patternpath = argv[1];
	readpatterns(patternpath);
	if(strcmp(argv[0], "add") == 0)
		exits(add(patternpath, argc-2, argv+2));
	else if(strcmp(argv[0], "check") == 0)
		exits(check(argc-2, argv+2));
	else
		usage();
}
