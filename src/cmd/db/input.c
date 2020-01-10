/*
 *
 *	debugger
 *
 */

#include "defs.h"
#include "fns.h"

Rune	line[LINSIZ];
extern	int	infile;
Rune	*lp;
int	peekc,lastc = EOR;
int	eof;

/* input routines */

int
eol(int c)
{
	return(c==EOR || c==';');
}

int
rdc(void)
{
	do {
		readchar();
	} while (lastc==SPC || lastc==TB);
	return(lastc);
}

void
reread(void)
{
	peekc = lastc;
}

void
clrinp(void)
{
	flush();
	lp = 0;
	peekc = 0;
}

int
readrune(int fd, Rune *r)
{
	char buf[UTFmax];
	int i;

	for(i=0; i<UTFmax && !fullrune(buf, i); i++)
		if(read(fd, buf+i, 1) <= 0)
			return -1;
	chartorune(r, buf);
	return 1;
}

int
readchar(void)
{
	Rune *p;

	if (eof)
		lastc=0;
	else if (peekc) {
		lastc = peekc;
		peekc = 0;
	}
	else {
		if (lp==0) {
			for (p = line; p < &line[LINSIZ-1]; p++) {
				eof = readrune(infile, p) <= 0;
				if (mkfault) {
					eof = 0;
					error(0);
				}
				if (eof) {
					p--;
					break;
				}
				if (*p == EOR) {
					if (p <= line)
						break;
					if (p[-1] != '\\')
						break;
					p -= 2;
				}
			}
			p[1] = 0;
			lp = line;
		}
		if ((lastc = *lp) != 0)
			lp++;
	}
	return(lastc);
}

int
nextchar(void)
{
	if (eol(rdc())) {
		reread();
		return(0);
	}
	return(lastc);
}

int
quotchar(void)
{
	if (readchar()=='\\')
		return(readchar());
	else if (lastc=='\'')
		return(0);
	else
		return(lastc);
}

void
getformat(char *deformat)
{
	char *fptr;
	BOOL	quote;
	Rune r;

	fptr=deformat;
	quote=FALSE;
	while ((quote ? readchar()!=EOR : !eol(readchar()))){
		r = lastc;
		fptr += runetochar(fptr, &r);
		if (lastc == '"')
			quote = ~quote;
	}
	lp--;
	if (fptr!=deformat)
		*fptr = '\0';
}

/*
 *	check if the input line if of the form:
 *		<filename>:<digits><verb> ...
 *
 *	we handle this case specially because we have to look ahead
 *	at the token after the colon to decide if it is a file reference
 *	or a colon-command with a symbol name prefix.
 */

int
isfileref(void)
{
	Rune *cp;

	for (cp = lp-1; *cp && !strchr(CMD_VERBS, *cp); cp++)
		if (*cp == '\\' && cp[1])	/* escape next char */
			cp++;
	if (*cp && cp > lp-1) {
		while (*cp == ' ' || *cp == '\t')
			cp++;
		if (*cp++ == ':') {
			while (*cp == ' ' || *cp == '\t')
				cp++;
			if (isdigit(*cp))
				return 1;
		}
	}
	return 0;
}
