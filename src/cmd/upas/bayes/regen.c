#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include "dfa.h"

/***
 * Regular expression for matching.
 */

char *ignore[] =
{
	/* HTML that isn't A, IMG, or FONT */
	/* Must have a space somewhere to avoid catching <email@address> */
	"<[ 	\n\r]*("
		"[^aif]|"
		"a[^> \t\r\n]|"
		"i[^mM \t\r\n]|"
		"im[^gG \t\r\n]|"
		"img[^> \t\r\n]|"
		"f[^oO \t\r\n]|"
		"fo[^Nn \t\r\n]|"
		"fon[^tT \t\r\n]|"
		"font[^> \r\t\n]"
	")[^>]*[ \t\n\r][^>]*>",
	"<[ 	\n\r]*("
		"i|im|f|fo|fon"
	")[ \t\r\n][^>]*>",

	/* ignore html comments */
	"<!--([^\\-]|-[^\\-]|--[^>]|\n)*-->",

	/* random mail strings */
	"^message-id:.*\n([ 	].*\n)*",
	"^in-reply-to:.*\n([ 	].*\n)*",
	"^references:.*\n([ 	].*\n)*",
	"^date:.*\n([ 	].*\n)*",
	"^delivery-date:.*\n([ 	].*\n)*",
	"e?smtp id .*",
	"^	id.*",
	"boundary=.*",
	"name=\"",
	"filename=\"",
	"news:<[^>]+>",
	"^--[^ 	]*$",

	/* base64 encoding */
	"^[0-9a-zA-Z+\\-=/]+$",

	/* uu encoding */
	"^[!-Z]+$",

	/* little things */
	".",
	"\n"
};

char *keywords[] =
{
	"([a-zA-Z'`$!¡-￿]|[0-9]([.,][0-9])*)+"
};

int debug;

Dreprog*
dregcomp(char *buf)
{
	Reprog *r;
	Dreprog *d;

	if(debug)
		print(">>> '%s'\n", buf);

	r = regcomp(buf);
	if(r == nil)
		sysfatal("regcomp");
	d = dregcvt(r);
	if(d == nil)
		sysfatal("dregcomp");
	free(r);
	return d;
}

char*
strcpycase(char *d, char *s)
{
	int cc, esc;

	cc = 0;
	esc = 0;
	while(*s){
		if(*s == '[')
			cc++;
		if(*s == ']')
			cc--;
		if(!cc && 'a' <= *s && *s <= 'z'){
			*d++ = '[';
			*d++ = *s;
			*d++ = *s+'A'-'a';
			*d++ = ']';
		}else
			*d++ = *s;
		if(*s == '\\')
			esc++;
		else if(esc)
			esc--;
		s++;
	}
	return d;
}

void
regerror(char *msg)
{
	sysfatal("regerror: %s", msg);
}

void
buildre(Dreprog *re[3])
{
	int i;
	static char buf[16384], *s;

	re[0] = dregcomp("^From ");

	s = buf;
	for(i=0; i<nelem(keywords); i++){
		if(i != 0)
			*s++ = '|';
		s = strcpycase(s, keywords[i]);
	}
	*s = 0;
	re[1] = dregcomp(buf);

	s = buf;
	for(i=0; i<nelem(ignore); i++){
		if(i != 0)
			*s++ = '|';
		s = strcpycase(s, ignore[i]);
	}
	*s = 0;
	re[2] = dregcomp(buf);
}

void
usage(void)
{
	fprint(2, "usage: regen [-d]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	Dreprog *re[3];
	Biobuf b;

	ARGBEGIN{
	default:
		usage();
	case 'd':
		debug = 1;
	}ARGEND

	if(argc != 0)
		usage();

	buildre(re);
	Binit(&b, 1, OWRITE);
	Bprintdfa(&b, re[0]);
	Bprintdfa(&b, re[1]);
	Bprintdfa(&b, re[2]);
	exits(0);
}
