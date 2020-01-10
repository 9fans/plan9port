#include <u.h>
#include <libc.h>
#include "dat.h"

void*
emalloc(int size)
{
	void *a;

	a = mallocz(size, 1);
	if(a == nil)
		sysfatal("%r");
	return a;
}

char*
estrdup(char *s)
{
	s = strdup(s);
	if(s == nil)
		sysfatal("%r");
	return s;
}

/*
 * like tokenize but obey "" quoting
 */
int
tokenize822(char *str, char **args, int max)
{
	int na;
	int intok = 0, inquote = 0;

	if(max <= 0)
		return 0;
	for(na=0; ;str++)
		switch(*str) {
		case ' ':
		case '\t':
			if(inquote)
				goto Default;
			/* fall through */
		case '\n':
			*str = 0;
			if(!intok)
				continue;
			intok = 0;
			if(na < max)
				continue;
			/* fall through */
		case 0:
			return na;
		case '"':
			inquote ^= 1;
			/* fall through */
		Default:
		default:
			if(intok)
				continue;
			args[na++] = str;
			intok = 1;
		}
	return 0;	/* can't get here; silence compiler */
}

Addr*
readaddrs(char *file, Addr *a)
{
	int fd;
	int i, n;
	char buf[8*1024];
	char *f[128];
	Addr **l;
	Addr *first;

	/* add to end */
	first = a;
	for(l = &first; *l != nil; l = &(*l)->next)
		;

	/* read in the addresses */
	fd = open(file, OREAD);
	if(fd < 0)
		return first;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return first;
	buf[n] = 0;

	n = tokenize822(buf, f, nelem(f));
	for(i = 0; i < n; i++){
		*l = a = emalloc(sizeof *a);
		l = &a->next;
		a->val = estrdup(f[i]);
	}
	return first;
}
