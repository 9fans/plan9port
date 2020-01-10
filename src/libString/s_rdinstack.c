
#include <u.h>
#include <libc.h>
#include <bio.h>
#include "libString.h"

struct Sinstack{
	int	depth;
	Biobuf	*fp[32];	/* hard limit to avoid infinite recursion */
};

/* initialize */
extern Sinstack *
s_allocinstack(char *file)
{
	Sinstack *sp;
	Biobuf *fp;

	fp = Bopen(file, OREAD);
	if(fp == nil)
		return nil;

	sp = malloc(sizeof *sp);
	sp->depth = 0;
	sp->fp[0] = fp;
	return sp;
}

extern void
s_freeinstack(Sinstack *sp)
{
	while(sp->depth >= 0)
		Bterm(sp->fp[sp->depth--]);
	free(sp);
}

/*  Append an input line to a String.
 *
 *  Empty lines and leading whitespace are removed.
 */
static char *
rdline(Biobuf *fp, String *to)
{
	int c;
	int len = 0;

	c = Bgetc(fp);

	/* eat leading white */
	while(c==' ' || c=='\t' || c=='\n' || c=='\r')
		c = Bgetc(fp);

	if(c < 0)
		return 0;

	for(;;){
		switch(c) {
		case -1:
			goto out;
		case '\\':
			c = Bgetc(fp);
			if (c != '\n') {
				s_putc(to, '\\');
				s_putc(to, c);
				len += 2;
			}
			break;
		case '\r':
			break;
		case '\n':
			if(len != 0)
				goto out;
			break;
		default:
			s_putc(to, c);
			len++;
			break;
		}
		c = Bgetc(fp);
	}
out:
	s_terminate(to);
	return to->ptr - len;
}

/* Append an input line to a String.
 *
 * Returns a pointer to the character string (or 0).
 * Leading whitespace and newlines are removed.
 * Lines starting with #include cause us to descend into the new file.
 * Empty lines and other lines starting with '#' are ignored.
 */
extern char *
s_rdinstack(Sinstack *sp, String *to)
{
	char *p;
	Biobuf *fp, *nfp;

	s_terminate(to);
	fp = sp->fp[sp->depth];

	for(;;){
		p = rdline(fp, to);
		if(p == nil){
			if(sp->depth == 0)
				break;
			Bterm(fp);
			sp->depth--;
			return s_rdinstack(sp, to);
		}

		if(strncmp(p, "#include", 8) == 0 && (p[8] == ' ' || p[8] == '\t')){
			to->ptr = p;
			p += 8;

			/* sanity (and looping) */
			if(sp->depth >= nelem(sp->fp))
				sysfatal("s_rdinstack: includes too deep");

			/* skip white */
			while(*p == ' ' || *p == '\t')
				p++;

			nfp = Bopen(p, OREAD);
			if(nfp == nil)
				continue;
			sp->depth++;
			sp->fp[sp->depth] = nfp;
			return s_rdinstack(sp, to);
		}

		/* got milk? */
		if(*p != '#')
			break;

		/* take care of comments */
		to->ptr = p;
		s_terminate(to);
	}
	return p;
}
