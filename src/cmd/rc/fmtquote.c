/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
#include <u.h>
#include <libc.h>
#include "fmt.h"
#include "fmtdef.h"

extern int (*doquote)(int);

/*
 * How many bytes of output UTF will be produced by quoting (if necessary) this string?
 * How many runes? How much of the input will be consumed?
 * The parameter q is filled in by _quotesetup.
 * The string may be UTF or Runes (s or r).
 * Return count does not include NUL.
 * Terminate the scan at the first of:
 *	NUL in input
 *	count exceeded in input
 *	count exceeded on output
 * *ninp is set to number of input bytes accepted.
 * nin may be <0 initially, to avoid checking input by count.
 */
void
__quotesetup(char *s, int nin, int nout, Quoteinfo *q, int sharp)
{
	int c;

	q->quoted = 0;
	q->nbytesout = 0;
	q->nrunesout = 0;
	q->nbytesin = 0;
	q->nrunesin = 0;
	if(sharp || nin==0 || *s=='\0'){
		if(nout < 2)
			return;
		q->quoted = 1;
		q->nbytesout = 2;
		q->nrunesout = 2;
	}
	for(; nin!=0; nin-=1){
		c = *s;

		if(c == '\0')
			break;
		if(q->nrunesout+1 > nout)
			break;

		if((c <= L' ') || (c == L'\'') || (doquote!=nil && doquote(c))){
			if(!q->quoted){
				if(1+q->nrunesout+1+1 > nout)	/* no room for quotes */
					break;
				q->nrunesout += 2;	/* include quotes */
				q->nbytesout += 2;	/* include quotes */
				q->quoted = 1;
			}
			if(c == '\'')	{
				q->nbytesout++;
				q->nrunesout++;	/* quotes reproduce as two characters */
			}
		}

		/* advance input */
		s++;
		q->nbytesin++;
		q->nrunesin++;

		/* advance output */
		q->nbytesout++;
		q->nrunesout++;
	}
}

static int
qstrfmt(char *sin, Quoteinfo *q, Fmt *f)
{
	int r;
	char *t, *s, *m, *me;
	ulong fl;
	int nc, w;

	m = sin;
	me = m + q->nbytesin;

	w = f->width;
	fl = f->flags;
	if(!(fl & FmtLeft) && __fmtpad(f, w - q->nbytesout) < 0)
		return -1;
	t = f->to;
	s = f->stop;
	FMTCHAR(f, t, s, '\'');
	for(nc = q->nrunesin; nc > 0; nc--){
		r = *(uchar*)m++;
		FMTCHAR(f, t, s, r);
		if(r == '\'')
			FMTCHAR(f, t, s, r);
	}

	FMTCHAR(f, t, s, '\'');
	f->nfmt += t - (char *)f->to;
	f->to = t;
	if(fl & FmtLeft && __fmtpad(f, w - q->nbytesout) < 0)
		return -1;
	return 0;
}

int
__quotestrfmt(int runesin, Fmt *f)
{
	int outlen;
	char *s;
	Quoteinfo q;

	f->flags &= ~FmtPrec;	/* ignored for %q %Q, so disable for %s %S in easy case */
	s = va_arg(f->args, char *);
	if(!s)
		return __fmtcpy(f, "<nil>", 5, 5);

	if(f->flush)
		outlen = 0x7FFFFFFF;	/* if we can flush, no output limit */
	else
		outlen = (char*)f->stop - (char*)f->to;

	__quotesetup(s, -1, outlen, &q, f->flags&FmtSharp);

	if(!q.quoted)
		return __fmtcpy(f, s, q.nrunesin, q.nbytesin);
	return qstrfmt(s, &q, f);
}

int
quotestrfmt(Fmt *f)
{
	return __quotestrfmt(0, f);
}

void
quotefmtinstall(void)
{
	fmtinstall('q', quotestrfmt);
}

int
__needsquotes(char *s, int *quotelenp)
{
	Quoteinfo q;

	__quotesetup(s, -1, 0x7FFFFFFF, &q, 0);
	*quotelenp = q.nbytesout;

	return q.quoted;
}
