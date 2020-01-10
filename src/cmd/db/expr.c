/*
 *
 *	debugger
 *
 */

#include "defs.h"
#include "fns.h"

static long	dbround(long, long);

extern	ADDR	ditto;
vlong	expv;

static WORD
ascval(void)
{
	Rune r;

	if (readchar() == 0)
		return (0);
	r = lastc;
	while(quotchar())	/*discard chars to ending quote */
		;
	return((WORD) r);
}

/*
 * read a floating point number
 * the result must fit in a WORD
 */

static WORD
fpin(char *buf)
{
	union {
		WORD w;
		float f;
	} x;

	x.f = atof(buf);
	return (x.w);
}

WORD
defval(WORD w)
{
	if (expr(0))
		return (expv);
	else
		return (w);
}

int
expr(int a)
{	/* term | term dyadic expr |  */
	int	rc;
	WORD	lhs;

	rdc();
	reread();
	rc=term(a);
	while (rc) {
		lhs = expv;
		switch ((int)readchar()) {

		case '+':
			term(a|1);
			expv += lhs;
			break;

		case '-':
			term(a|1);
			expv = lhs - expv;
			break;

		case '#':
			term(a|1);
			expv = dbround(lhs,expv);
			break;

		case '*':
			term(a|1);
			expv *= lhs;
			break;

		case '%':
			term(a|1);
			if(expv != 0)
				expv = lhs/expv;
			else{
				if(lhs)
					expv = 1;
				else
					expv = 0;
			}
			break;

		case '&':
			term(a|1);
			expv &= lhs;
			break;

		case '|':
			term(a|1);
			expv |= lhs;
			break;

		case ')':
			if ((a&2)==0)
				error("unexpected `)'");

		default:
			reread();
			return(rc);
		}
	}
	return(rc);
}

int
term(int a)
{	/* item | monadic item | (expr) | */
	u32int u;

	switch ((int)readchar()) {

	case '*':
		term(a|1);
		if (get4(cormap, (ADDR)expv, &u) < 0)
			error("%r");
		expv = u;
		return(1);

	case '@':
		term(a|1);
		if (get4(symmap, (ADDR)expv, &u) < 0)
			error("%r");
		expv = u;
		return(1);

	case '-':
		term(a|1);
		expv = -expv;
		return(1);

	case '~':
		term(a|1);
		expv = ~expv;
		return(1);

	case '(':
		expr(2);
		if (readchar()!=')')
			error("syntax error: `)' expected");
		return(1);

	default:
		reread();
		return(item(a));
	}
}

int
item(int a)
{	/* name [ . local ] | number | . | ^  | <register | 'x | | */
	char	*base;
	char	savc;
	u64int u;
	Symbol s;
	char gsym[MAXSYM], lsym[MAXSYM];

	readchar();
	if (isfileref()) {
		readfname(gsym);
		rdc();			/* skip white space */
		if (lastc == ':') {	/* it better be */
			rdc();		/* skip white space */
			if (!getnum(readchar))
				error("bad number");
			if (expv == 0)
				expv = 1;	/* file begins at line 1 */
			if(file2pc(gsym, expv, &u) < 0)
				error("%r");
			expv = u;
			return 1;
		}
		error("bad file location");
	} else if (symchar(0)) {
		readsym(gsym);
		if (lastc=='.') {
			readchar();	/* ugh */
			if (lastc == '.') {
				lsym[0] = '.';
				readchar();
				readsym(lsym+1);
			} else if (symchar(0)) {
				readsym(lsym);
			} else
				lsym[0] = 0;
			if (localaddr(cormap, correg, gsym, lsym, &u) < 0)
				error("%r");
			expv = u;
		}
		else {
			if (lookupsym(0, gsym, &s) < 0)
				error("symbol not found");
			if (s.loc.type != LADDR)
				error("symbol not kept in memory");
			expv = s.loc.addr;
		}
		reread();
	} else if (getnum(readchar)) {
		;
	} else if (lastc=='.') {
		readchar();
		if (!symchar(0) && lastc != '.') {
			expv = dot;
		} else {
			if (findsym(locaddr(dbrget(cormap, mach->pc)), CTEXT, &s) < 0)
				error("no current function");
			if (lastc == '.') {
				lsym[0] = '.';
				readchar();
				readsym(lsym+1);
			} else
				readsym(lsym);
			if (localaddr(cormap, correg, s.name, lsym, &u) < 0)
				error("%r");
			expv = u;
		}
		reread();
	} else if (lastc=='"') {
		expv=ditto;
	} else if (lastc=='+') {
		expv=inkdot(dotinc);
	} else if (lastc=='^') {
		expv=inkdot(-dotinc);
	} else if (lastc=='<') {
		savc=rdc();
		base = regname(savc);
		expv = dbrget(cormap, base);
	}
	else if (lastc=='\'')
		expv = ascval();
	else if (a)
		error("address expected");
	else {
		reread();
		return(0);
	}
	return(1);
}

#define	MAXBASE	16

/* service routines for expression reading */
int
getnum(int (*rdf)(void))
{
	char *cp;
	int base, d;
	BOOL fpnum;
	char num[MAXLIN];

	base = 0;
	fpnum = FALSE;
	if (lastc == '#') {
		base = 16;
		(*rdf)();
	}
	if (convdig(lastc) >= MAXBASE)
		return (0);
	if (lastc == '0')
		switch ((*rdf)()) {
		case 'x':
		case 'X':
			base = 16;
			(*rdf)();
			break;

		case 't':
		case 'T':
			base = 10;
			(*rdf)();
			break;

		case 'o':
		case 'O':
			base = 8;
			(*rdf)();
			break;
		default:
			if (base == 0)
				base = 8;
			break;
		}
	if (base == 0)
		base = 10;
	expv = 0;
	for (cp = num, *cp = lastc; ;(*rdf)()) {
		if ((d = convdig(lastc)) < base) {
			expv *= base;
			expv += d;
			*cp++ = lastc;
		}
		else if (lastc == '.') {
			fpnum = TRUE;
			*cp++ = lastc;
		} else {
			reread();
			break;
		}
	}
	if (fpnum)
		expv = fpin(num);
	return (1);
}

void
readsym(char *isymbol)
{
	char	*p;
	Rune r;

	p = isymbol;
	do {
		if (p < &isymbol[MAXSYM-UTFmax-1]){
			r = lastc;
			p += runetochar(p, &r);
		}
		readchar();
	} while (symchar(1));
	*p = 0;
}

void
readfname(char *filename)
{
	char	*p;
	Rune	c;

	/* snarf chars until un-escaped char in terminal char set */
	p = filename;
	do {
		if ((c = lastc) != '\\' && p < &filename[MAXSYM-UTFmax-1])
			p += runetochar(p, &c);
		readchar();
	} while (c == '\\' || strchr(CMD_VERBS, lastc) == 0);
	*p = 0;
	reread();
}

int
convdig(int c)
{
	if (isdigit(c))
		return(c-'0');
	else if (!isxdigit(c))
		return(MAXBASE);
	else if (isupper(c))
		return(c-'A'+10);
	else
		return(c-'a'+10);
}

int
symchar(int dig)
{
	if (lastc=='\\') {
		readchar();
		return(TRUE);
	}
	return(isalpha(lastc) || lastc>0x80 || lastc=='_' || dig && isdigit(lastc));
}

static long
dbround(long a, long b)
{
	long w;

	w = (a/b)*b;
	if (a!=w)
		w += b;
	return(w);
}

ulong
dbrget(Map *map, char *name)
{
	u64int u;

	USED(map);
	if(rget(correg, name, &u) < 0)
		return ~(ulong)0;
	return u;
}
