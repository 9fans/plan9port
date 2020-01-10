/****************************************************************
Copyright (C) Lucent Technologies 1997
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name Lucent Technologies or any of
its entities not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
****************************************************************/


#define DEBUG
#include <stdio.h>
#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include <regexp.h>
#include "awk.h"
#include "y.tab.h"

	/* This file provides the interface between the main body of
	 * awk and the pattern matching package.  It preprocesses
	 * patterns prior to compilation to provide awk-like semantics
	 * to character sequences not supported by the pattern package.
	 * The following conversions are performed:
	 *
	 *	"()"		->	"[]"
	 *	"[-"		->	"[\-"
	 *	"[^-"		->	"[^\-"
	 *	"-]"		->	"\-]"
	 *	"[]"		->	"[]*"
	 *	"\xdddd"	->	"\z" where 'z' is the UTF sequence
	 *					for the hex value
	 *	"\ddd"		->	"\o" where 'o' is a char octal value
	 *	"\b"		->	"\B"	where 'B' is backspace
	 *	"\t"		->	"\T"	where 'T' is tab
	 *	"\f"		->	"\F"	where 'F' is form feed
	 *	"\n"		->	"\N"	where 'N' is newline
	 *	"\r"		->	"\r"	where 'C' is cr
	 */

#define	MAXRE	512

static char	re[MAXRE];	/* copy buffer */

char	*patbeg;
int	patlen;			/* number of chars in pattern */

#define	NPATS	20		/* number of slots in pattern cache */

static struct pat_list		/* dynamic pattern cache */
{
	char	*re;
	int	use;
	Reprog	*program;
} pattern[NPATS];

static int npats;		/* cache fill level */

	/* Compile a pattern */
void
*compre(char *pat)
{
	int i, j, inclass;
	char c, *p, *s;
	Reprog *program;

	if (!compile_time) {	/* search cache for dynamic pattern */
		for (i = 0; i < npats; i++)
			if (!strcmp(pat, pattern[i].re)) {
				pattern[i].use++;
				return((void *) pattern[i].program);
			}
	}
		/* Preprocess Pattern for compilation */
	p = re;
	s = pat;
	inclass = 0;
	while (c = *s++) {
		if (c == '\\') {
			quoted(&s, &p, re+MAXRE);
			continue;
		}
		else if (!inclass && c == '(' && *s == ')') {
			if (p < re+MAXRE-2) {	/* '()' -> '[]*' */
				*p++ = '[';
				*p++ = ']';
				c = '*';
				s++;
			}
			else overflow();
		}
		else if (c == '['){			/* '[-' -> '[\-' */
			inclass = 1;
			if (*s == '-') {
				if (p < re+MAXRE-2) {
					*p++ = '[';
					*p++ = '\\';
					c = *s++;
				}
				else overflow();
			}				/* '[^-' -> '[^\-'*/
			else if (*s == '^' && s[1] == '-'){
				if (p < re+MAXRE-3) {
					*p++ = '[';
					*p++ = *s++;
					*p++ = '\\';
					c = *s++;
				}
				else overflow();
			}
			else if (*s == '['){		/* skip '[[' */
				if (p < re+MAXRE-1)
					*p++ = c;
				else overflow();
				c = *s++;
			}
			else if (*s == '^' && s[1] == '[') {	/* skip '[^['*/
				if (p < re+MAXRE-2) {
					*p++ = c;
					*p++ = *s++;
					c = *s++;
				}
				else overflow();
			}
			else if (*s == ']') {		/* '[]' -> '[]*' */
				if (p < re+MAXRE-2) {
					*p++ = c;
					*p++ = *s++;
					c = '*';
					inclass = 0;
				}
				else overflow();
			}
		}
		else if (c == '-' && *s == ']') {	/* '-]' -> '\-]' */
			if (p < re+MAXRE-1)
				*p++ = '\\';
			else overflow();
		}
		else if (c == ']')
			inclass = 0;
		if (p < re+MAXRE-1)
			*p++ = c;
		else overflow();
	}
	*p = 0;
	program = regcomp(re);		/* compile pattern */
	if (!compile_time) {
		if (npats < NPATS)	/* Room in cache */
			i = npats++;
		else {			/* Throw out least used */
			int use = pattern[0].use;
			i = 0;
			for (j = 1; j < NPATS; j++) {
				if (pattern[j].use < use) {
					use = pattern[j].use;
					i = j;
				}
			}
			xfree(pattern[i].program);
			xfree(pattern[i].re);
		}
		pattern[i].re = tostring(pat);
		pattern[i].program = program;
		pattern[i].use = 1;
	}
	return((void *) program);
}

	/* T/F match indication - matched string not exported */
int
match(void *p, char *s, char *start)
{
	return regexec((Reprog *) p, (char *) s, 0, 0);
}

	/* match and delimit the matched string */
int
pmatch(void *p, char *s, char *start)
{
	Resub m;

	m.s.sp = start;
	m.e.ep = 0;
	if (regexec((Reprog *) p, (char *) s, &m, 1)) {
		patbeg = m.s.sp;
		patlen = m.e.ep-m.s.sp;
		return 1;
	}
	patlen = -1;
	patbeg = start;
	return 0;
}

	/* perform a non-empty match */
int
nematch(void *p, char *s, char *start)
{
	if (pmatch(p, s, start) == 1 && patlen > 0)
		return 1;
	patlen = -1;
	patbeg = start;
	return 0;
}
/* in the parsing of regular expressions, metacharacters like . have */
/* to be seen literally;  \056 is not a metacharacter. */

int
hexstr(char **pp)	/* find and eval hex string at pp, return new p */
{
	char c;
	int n = 0;
	int i;

	for (i = 0, c = (*pp)[i]; i < 4 && isxdigit(c); i++, c = (*pp)[i]) {
		if (isdigit(c))
			n = 16 * n + c - '0';
		else if ('a' <= c && c <= 'f')
			n = 16 * n + c - 'a' + 10;
		else if ('A' <= c && c <= 'F')
			n = 16 * n + c - 'A' + 10;
	}
	*pp += i;
	return n;
}

	/* look for awk-specific escape sequences */

#define isoctdigit(c) ((c) >= '0' && (c) <= '7') /* multiple use of arg */

void
quoted(char **s, char **to, char *end)	/* handle escaped sequence */
{
	char *p = *s;
	char *t = *to;
	wchar_t c;

	switch(c = *p++) {
	case 't':
		c = '\t';
		break;
	case 'n':
		c = '\n';
		break;
	case 'f':
		c = '\f';
		break;
	case 'r':
		c = '\r';
		break;
	case 'b':
		c = '\b';
		break;
	default:
		if (t < end-1)		/* all else must be escaped */
			*t++ = '\\';
		if (c == 'x') {		/* hexadecimal goo follows */
			c = hexstr(&p);
			if (t < end-MB_CUR_MAX)
				t += wctomb(t, c);
			else overflow();
			*to = t;
			*s = p;
			return;
		} else if (isoctdigit(c)) {	/* \d \dd \ddd */
			c -= '0';
			if (isoctdigit(*p)) {
				c = 8 * c + *p++ - '0';
				if (isoctdigit(*p))
					c = 8 * c + *p++ - '0';
			}
		}
		break;
	}
	if (t < end-1)
		*t++ = c;
	*s = p;
	*to = t;
}
	/* count rune positions */
int
countposn(char *s, int n)
{
	int i, j;
	char *end;

	for (i = 0, end = s+n; *s && s < end; i++){
		j = mblen(s, n);
		if(j <= 0)
			j = 1;
		s += j;
	}
	return(i);
}

	/* pattern package error handler */

void
regerror(char *s)
{
	FATAL("%s", s);
}

void
overflow(void)
{
	FATAL("%s", "regular expression too big");
}
