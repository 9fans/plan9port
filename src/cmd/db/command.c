/*
 *
 *	debugger
 *
 */

#include "defs.h"
#include "fns.h"

char	BADEQ[] = "unexpected `='";

BOOL	executing;
extern	char	*lp;

char	eqformat[ARB] = "z";
char	stformat[ARB] = "zMi";

ADDR	ditto;

ADDR	dot;
WORD	dotinc;
WORD	adrval, cntval, loopcnt;
int	adrflg, cntflg;

/* command decoding */

int
command(char *buf, int defcom)
{
	char	*reg;
	char	savc;
	char	*savlp=lp;
	char	savlc = lastc;
	char	savpc = peekc;
	static char lastcom = '=', savecom = '=';

	if (defcom == 0)
		defcom = lastcom;
	if (buf) {
		if (*buf==EOR)
			return(FALSE);
		clrinp();
		lp=buf;
	}
	do {
		adrflg=expr(0);		/* first address */
		if (adrflg){
			dot=expv;
			ditto=expv;
		}
		adrval=dot;

		if (rdc()==',' && expr(0)) {	/* count */
			cntflg=TRUE;
			cntval=expv;
		} else {
			cntflg=FALSE;
			cntval=1;
			reread();
		}

		if (!eol(rdc()))
			lastcom=lastc;		/* command */
		else {
			if (adrflg==0)
				dot=inkdot(dotinc);
			reread();
			lastcom=defcom;
		}
		switch(lastcom) {
		case '/':
		case '=':
		case '?':
			savecom = lastcom;
			acommand(lastcom);
			break;

		case '>':
			lastcom = savecom;
			savc=rdc();
			if (reg=regname(savc))
				rput(correg, reg, dot);
			else
				error("bad variable");
			break;

		case '!':
			lastcom=savecom;
			shell();
			break;

		case '$':
			lastcom=savecom;
			printdollar(nextchar());
			break;

		case ':':
			if (!executing) {
				executing=TRUE;
				subpcs(nextchar());
				executing=FALSE;
				lastcom=savecom;
			}
			break;

		case 0:
			prints(DBNAME);
			break;

		default:
			error("bad command");
		}
		flushbuf();
	} while (rdc()==';');
	if (buf == 0)
		reread();
	else {
		clrinp();
		lp=savlp;
		lastc = savlc;
		peekc = savpc;
	}

	if(adrflg)
		return dot;
	return 1;
}

/*
 * [/?][wml]
 */

void
acommand(int pc)
{
	int eqcom;
	Map *map;
	char *fmt;
	char buf[512];

	if (pc == '=') {
		eqcom = 1;
		fmt = eqformat;
		map = dotmap;
	} else {
		eqcom = 0;
		fmt = stformat;
		if (pc == '/')
			map = cormap;
		else
			map = symmap;
	}
	if (!map) {
		sprint(buf, "no map for %c", pc);
		error(buf);
	}

	switch (rdc())
	{
	case 'm':
		if (eqcom)
			error(BADEQ);
		cmdmap(map);
		break;

	case 'L':
	case 'l':
		if (eqcom)
			error(BADEQ);
		cmdsrc(lastc, map);
		break;

	case 'W':
	case 'w':
		if (eqcom)
			error(BADEQ);
		cmdwrite(lastc, map);
		break;

	default:
		reread();
		getformat(fmt);
		scanform(cntval, !eqcom, fmt, map, eqcom);
	}
}

void
cmdsrc(int c, Map *map)
{
	u32int w;
	long locval, locmsk;
	ADDR savdot;
	ushort sh;
	char buf[512];
	int ret;

	if (c == 'L')
		dotinc = 4;
	else
		dotinc = 2;
	savdot=dot;
	expr(1);
	locval=expv;
	if (expr(0))
		locmsk=expv;
	else
		locmsk = ~0;
	if (c == 'L')
		while ((ret = get4(map, dot, &w)) > 0 &&  (w&locmsk) != locval)
			dot = inkdot(dotinc);
	else
		while ((ret = get2(map, dot, &sh)) > 0 && (sh&locmsk) != locval)
			dot = inkdot(dotinc);
	if (ret < 0) {
		dot=savdot;
		error("%r");
	}
	symoff(buf, 512, dot, CANY);
	dprint(buf);
}

static char badwrite[] = "can't write process memory or text image";

void
cmdwrite(int wcom, Map *map)
{
	ADDR savdot;
	char *format;
	int pass;

	if (wcom == 'w')
		format = "x";
	else
		format = "X";
	expr(1);
	pass = 0;
	do {
		pass++;
		savdot=dot;
		exform(1, 1, format, map, 0, pass);
		dot=savdot;
		if (wcom == 'W') {
			if (put4(map, dot, expv) <= 0)
				error(badwrite);
		} else {
			if (put2(map, dot, expv) <= 0)
				error(badwrite);
		}
		savdot=dot;
		dprint("=%8t");
		exform(1, 0, format, map, 0, pass);
		newline();
	} while (expr(0));
	dot=savdot;
}

/*
 * collect a register name; return register offset
 * this is not what i'd call a good division of labour
 */

char *
regname(int regnam)
{
	static char buf[64];
	char *p;
	int c;

	p = buf;
	*p++ = regnam;
	while (isalnum(c = readchar())) {
		if (p >= buf+sizeof(buf)-1)
			error("register name too long");
		*p++ = c;
	}
	*p = 0;
	reread();
	return (buf);
}

/*
 * shell escape
 */

void
shell(void)
{
	int	rc, unixpid;
	char *argp = lp;

	while (lastc!=EOR)
		rdc();
	if ((unixpid=fork())==0) {
		*lp=0;
		execl("/bin/rc", "rc", "-c", argp, 0);
		exits("execl");				/* botch */
	} else if (unixpid == -1) {
		error("cannot fork");
	} else {
		mkfault = 0;
		while ((rc = waitpid()) != unixpid){
			if(rc == -1 && mkfault){
				mkfault = 0;
				continue;
			}
			break;
		}
		prints("!");
		reread();
	}
}
