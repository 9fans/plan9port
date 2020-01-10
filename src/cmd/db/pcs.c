/*
 *
 *	debugger
 *
 */

#include "defs.h"
#include "fns.h"

char	NOPCS[] = "no process";

/* sub process control */

void
subpcs(int modif)
{
	int	check;
	int	runmode;
	int	keepnote;
	int	n, r;
	ulong line, curr;
	BKPT *bk;
	char *comptr;

	runmode=SINGLE;
	r = 0;
	keepnote=0;
	loopcnt=cntval;
	switch (modif) {

		/* delete breakpoint */
	case 'd':
	case 'D':
		if ((bk=scanbkpt(dot)) == 0)
			error("no breakpoint set");
		bk->flag=BKPTCLR;
		return;

		/* set breakpoint */
	case 'b':
	case 'B':
		if (bk=scanbkpt(dot))
			bk->flag=BKPTCLR;
		for (bk=bkpthead; bk; bk=bk->nxtbkpt)
			if (bk->flag == BKPTCLR)
				break;
		if (bk==0) {
			bk = (BKPT *)malloc(sizeof(*bk));
			if (bk == 0)
				error("too many breakpoints");
			bk->nxtbkpt=bkpthead;
			bkpthead=bk;
		}
		bk->loc = dot;
		bk->initcnt = bk->count = cntval;
		bk->flag = modif == 'b' ? BKPTSET : BKPTTMP;
		check=MAXCOM-1;
		comptr=bk->comm;
		rdc();
		reread();
		do {
			*comptr++ = readchar();
		} while (check-- && lastc!=EOR);
		*comptr=0;
		if(bk->comm[0] != EOR && cntflg == FALSE)
			bk->initcnt = bk->count = HUGEINT;
		reread();
		if (check)
			return;
		error("bkpt command too long");

		/* exit */
	case 'k' :
	case 'K':
		if (pid == 0)
			error(NOPCS);
		dprint("%d: killed", pid);
		pcsactive = 1;	/* force 'kill' ctl */
		endpcs();
		return;

		/* run program */
	case 'r':
	case 'R':
		endpcs();
		setup();
		runmode = CONTIN;
		break;

		/* single step */
	case 's':
		if (pid == 0) {
			setup();
			loopcnt--;
		}
		runmode=SINGLE;
		keepnote=defval(1);
		break;
	case 'S':
		if (pid == 0) {
			setup();
			loopcnt--;
		}
		keepnote=defval(1);
		if(pc2line(dbrget(cormap, mach->pc), &line) < 0)
			error("%r");
		n = loopcnt;
		dprint("%s: running\n", symfil);
		flush();
		for (loopcnt = 1; n > 0; loopcnt = 1) {
			r = runpcs(SINGLE, keepnote);
			if(pc2line(dot, &curr) < 0)
				error("%r");
			if (line != curr) {	/* on a new line of c */
				line = curr;
				n--;
			}
		}
		loopcnt = 0;
		break;
		/* continue with optional note */
	case 'c':
	case 'C':
		if (pid==0)
			error(NOPCS);
		runmode=CONTIN;
		keepnote=defval(1);
		break;

	case 'n':	/* deal with notes */
		if (pid==0)
			error(NOPCS);
		n=defval(-1);
		if(n>=0 && n<nnote){
			nnote--;
			memmove(note[n], note[n+1], (nnote-n)*sizeof(note[0]));
		}
		notes();
		return;

	case 'h':	/* halt the current process */
		if (adrflg && adrval == 0) {
			if (pid == 0)
				error(NOPCS);
			ungrab();
		}
		else {
			grab();
			dprint("stopped at%16t");
			goto Return;
		}
		return;

	case 'x':	/* continue executing the current process */
		if (pid == 0)
			error(NOPCS);
		ungrab();
		return;

	default:
		error("bad `:' command");
	}

	if (loopcnt>0) {
		dprint("%s: running\n", symfil);
		flush();
		r = runpcs(runmode,keepnote);
	}
	if (r)
		dprint("breakpoint%16t");
	else
		dprint("stopped at%16t");
    Return:
	delbp();
	printpc();
	notes();
}
