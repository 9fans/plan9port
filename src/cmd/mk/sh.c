#include	"mk.h"

char	*termchars = "\"'= \t";	/*used in parse.c to isolate assignment attribute*/
char	*shflags = 0;
int	IWS = ' ';		/* inter-word separator in env */

/*
 *	This file contains functions that depend on the shell's syntax.  Most
 *	of the routines extract strings observing the shell's escape conventions.
 */


/*
 *	skip a token in quotes.
 */
static char *
squote(char *cp, int c)
{
	Rune r;
	int n;

	while(*cp){
		n = chartorune(&r, cp);
		if(r == c)
			return cp;
		if(r == '\\')
			n += chartorune(&r, cp+n);
		cp += n;
	}
	SYNERR(-1);		/* should never occur */
	fprint(2, "missing closing '\n");
	return 0;
}
/*
 *	search a string for unescaped characters in a pattern set
 */
char *
charin(char *cp, char *pat)
{
	Rune r;
	int n, vargen;

	vargen = 0;
	while(*cp){
		n = chartorune(&r, cp);
		switch(r){
		case '\\':			/* skip escaped char */
			cp += n;
			n = chartorune(&r, cp);
			break;
		case '\'':			/* skip quoted string */
		case '"':
			cp = squote(cp+1, r);	/* n must = 1 */
			if(!cp)
				return 0;
			break;
		case '$':
			if(*(cp+1) == '{')
				vargen = 1;
			break;
		case '}':
			if(vargen)
				vargen = 0;
			else if(utfrune(pat, r))
				return cp;
			break;
		default:
			if(vargen == 0 && utfrune(pat, r))
				return cp;
			break;
		}
		cp += n;
	}
	if(vargen){
		SYNERR(-1);
		fprint(2, "missing closing } in pattern generator\n");
	}
	return 0;
}

/*
 *	extract an escaped token.  Possible escape chars are single-quote,
 *	double-quote,and backslash.
 */
char*
expandquote(char *s, Rune esc, Bufblock *b)
{
	Rune r;

	if (esc == '\\') {
		s += chartorune(&r, s);
		rinsert(b, r);
		return s;
	}

	while(*s){
		s += chartorune(&r, s);
		if(r == esc)
			return s;
		if (r == '\\') {
			rinsert(b, r);
			s += chartorune(&r, s);
		}
		rinsert(b, r);
	}
	return 0;
}

/*
 *	Input an escaped token.  Possible escape chars are single-quote,
 *	double-quote and backslash.
 */
int
escapetoken(Biobuf *bp, Bufblock *buf, int preserve, int esc)
{
	int c, line;

	if(esc == '\\') {
		c = Bgetrune(bp);
		if(c == '\r')
			c = Bgetrune(bp);
		if (c == '\n')
			mkinline++;
		rinsert(buf, c);
		return 1;
	}

	line = mkinline;
	while((c = nextrune(bp, 0)) >= 0){
		if(c == esc){
			if(preserve)
				rinsert(buf, c);
			return 1;
		}
		if(c == '\\') {
			rinsert(buf, c);
			c = Bgetrune(bp);
			if(c == '\r')
				c = Bgetrune(bp);
			if (c < 0)
				break;
			if (c == '\n')
				mkinline++;
		}
		rinsert(buf, c);
	}
	SYNERR(line); fprint(2, "missing closing %c\n", esc);
	return 0;
}

/*
 *	copy a quoted string; s points to char after opening quote
 */
static char *
copysingle(char *s, Rune q, Bufblock *buf)
{
	Rune r;

	while(*s){
		s += chartorune(&r, s);
		rinsert(buf, r);
		if(r == q)
			break;
	}
	return s;
}
/*
 *	check for quoted strings.  backquotes are handled here; single quotes above.
 *	s points to char after opening quote, q.
 */
char *
copyq(char *s, Rune q, Bufblock *buf)
{
	if(q == '\'' || q == '"')		/* copy quoted string */
		return copysingle(s, q, buf);

	if(q != '`')				/* not quoted */
		return s;

	while(*s){				/* copy backquoted string */
		s += chartorune(&q, s);
		rinsert(buf, q);
		if(q == '`')
			break;
		if(q == '\'' || q == '"')
			s = copysingle(s, q, buf);	/* copy quoted string */
	}
	return s;
}
