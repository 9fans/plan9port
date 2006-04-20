#include	"mk.h"

/* table-driven version in bootes dump of 12/31/96 */

long
mtime(char *name)
{
	return mkmtime(name);
}

long
timeof(char *name, int force)
{
	Symtab *sym;
	long t;

	if(utfrune(name, '('))
		return atimeof(force, name);	/* archive */

	if(force)
		return mtime(name);


	sym = symlook(name, S_TIME, 0);
	if (sym)
		return sym->u.value;

	t = mtime(name);
	if(t == 0)
		return 0;

	symlook(name, S_TIME, (void*)t);		/* install time in cache */
	return t;
}

void
touch(char *name)
{
	Bprint(&bout, "touch(%s)\n", name);
	if(nflag)
		return;

	if(utfrune(name, '('))
		atouch(name);		/* archive */
	else if(chgtime(name) < 0) {
		fprint(2, "%s: %r\n", name);
		Exit();
	}
}

void
delete(char *name)
{
	if(utfrune(name, '(') == 0) {		/* file */
		if(remove(name) < 0)
			fprint(2, "remove %s: %r\n", name);
	} else
		fprint(2, "hoon off; mk can'tdelete archive members\n");
}

void
timeinit(char *s)
{
	long t;
	char *cp;
	Rune r;
	int c, n;

	t = time(0);
	while (*s) {
		cp = s;
		do{
			n = chartorune(&r, s);
			if (r == ' ' || r == ',' || r == '\n')
				break;
			s += n;
		} while(*s);
		c = *s;
		*s = 0;
		symlook(strdup(cp), S_TIME, (void *)t)->u.value = t;
		if (c)
			*s++ = c;
		while(*s){
			n = chartorune(&r, s);
			if(r != ' ' && r != ',' && r != '\n')
				break;
			s += n;
		}
	}
}
