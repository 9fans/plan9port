#include "common.h"

/* expand a path relative to some `.' */
extern String *
abspath(char *path, char *dot, String *to)
{
	if (*path == '/') {
		to = s_append(to, path);
	} else {
		to = s_append(to, dot);
		to = s_append(to, "/");
		to = s_append(to, path);
	}
	return to;
}

/* return a pointer to the base component of a pathname */
extern char *
basename(char *path)
{
	char *cp;

	cp = strrchr(path, '/');
	return cp==0 ? path : cp+1;
}

/* append a sub-expression match onto a String */
extern void
append_match(Resub *subexp, String *sp, int se)
{
	char *cp, *ep;

	cp = subexp[se].s.sp;
	ep = subexp[se].e.ep;
	for (; cp < ep; cp++)
		s_putc(sp, *cp);
	s_terminate(sp);
}

/*
 *  check for shell characters in a String
 */
static char *illegalchars = "\r\n";

extern int
shellchars(char *cp)
{
	char *sp;

	for(sp=illegalchars; *sp; sp++)
		if(strchr(cp, *sp))
			return 1;
	return 0;
}

static char *specialchars = " ()<>{};=\\'`^&|";
static char *escape = "%%";

int
hexchar(int x)
{
	x &= 0xf;
	if(x < 10)
		return '0' + x;
	else
		return 'A' + x - 10;
}

/*
 *  rewrite a string to escape shell characters
 */
extern String*
escapespecial(String *s)
{
	String *ns;
	char *sp;

	for(sp = specialchars; *sp; sp++)
		if(strchr(s_to_c(s), *sp))
			break;
	if(*sp == 0)
		return s;

	ns = s_new();
	for(sp = s_to_c(s); *sp; sp++){
		if(strchr(specialchars, *sp)){
			s_append(ns, escape);
			s_putc(ns, hexchar(*sp>>4));
			s_putc(ns, hexchar(*sp));
		} else
			s_putc(ns, *sp);
	}
	s_terminate(ns);
	s_free(s);
	return ns;
}

uint
hex2uint(char x)
{
	if(x >= '0' && x <= '9')
		return x - '0';
	if(x >= 'A' && x <= 'F')
		return (x - 'A') + 10;
	if(x >= 'a' && x <= 'f')
		return (x - 'a') + 10;
	return -512;
}

/*
 *  rewrite a string to remove shell characters escapes
 */
extern String*
unescapespecial(String *s)
{
	String *ns;
	char *sp;
	uint c, n;

	if(strstr(s_to_c(s), escape) == 0)
		return s;
	n = strlen(escape);

	ns = s_new();
	for(sp = s_to_c(s); *sp; sp++){
		if(strncmp(sp, escape, n) == 0){
			c = (hex2uint(sp[n])<<4) + hex2uint(sp[n+1]);
			if(c < 0)
				s_putc(ns, *sp);
			else {
				s_putc(ns, c);
				sp += n+2-1;
			}
		} else
			s_putc(ns, *sp);
	}
	s_terminate(ns);
	s_free(s);
	return ns;

}

int
returnable(char *path)
{

	return strcmp(path, "/dev/null") != 0;
}
