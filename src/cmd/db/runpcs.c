/*
 *
 *	debugger
 *
 */

#include "defs.h"
#include "fns.h"

BKPT *bkpthead;

BOOL bpin;

int pid;
int nnote;
int ending;
char note[NNOTE][ERRMAX];

/* service routines for sub process control */

int
runpcs(int runmode, int keepnote)
{
	int rc;
	BKPT *bkpt;
	ADDR x;

	rc = 0;
	if (adrflg)
		rput(correg, mach->pc, dot);
	if(rget(correg, mach->pc, &dot) < 0)
		error("%r");
	flush();
	while (--loopcnt >= 0) {
		if(loopcnt != 0)
			printpc();
		if (runmode == SINGLE) {
			bkpt = scanbkpt(dot);
			if (bkpt) {
				switch(bkpt->flag){
				case BKPTTMP:
					bkpt->flag = BKPTCLR;
					break;
				case BKPTSKIP:
					bkpt->flag = BKPTSET;
					break;
				}
			}
			runstep(dot, keepnote);
		} else {
			if(rget(correg, mach->pc, &x) < 0)
				error("%r");
			if ((bkpt = scanbkpt(x)) != 0) {
				execbkpt(bkpt, keepnote);
				keepnote = 0;
			}
			setbp();
			runrun(keepnote);
		}
		keepnote = 0;
		delbp();
		if(rget(correg, mach->pc, &dot) < 0)
			error("%r");
		/* real note? */
		if (nnote > 0) {
			keepnote = 1;
			rc = 0;
			continue;
		}
		bkpt = scanbkpt(dot);
		if(bkpt == 0){
			keepnote = 0;
			rc = 0;
			continue;
		}
		/* breakpoint */
		if (bkpt->flag == BKPTTMP)
			bkpt->flag = BKPTCLR;
		else if (bkpt->flag == BKPTSKIP) {
			execbkpt(bkpt, keepnote);
			keepnote = 0;
			loopcnt++;	/* we didn't really stop */
			continue;
		}
		else {
			bkpt->flag = BKPTSKIP;
			--bkpt->count;
			if ((bkpt->comm[0] == EOR || command(bkpt->comm, ':') != 0)
			&&  bkpt->count != 0) {
				execbkpt(bkpt, keepnote);
				keepnote = 0;
				loopcnt++;
				continue;
			}
			bkpt->count = bkpt->initcnt;
		}
		rc = 1;
	}
	return(rc);
}

/*
 * finish the process off;
 * kill if still running
 */

void
endpcs(void)
{
	BKPT *bk;

	if(ending)
		return;
	ending = 1;
	if (pid) {
		if(pcsactive){
			killpcs();
			pcsactive = 0;
		}
		pid=0;
		nnote=0;
		for (bk=bkpthead; bk; bk = bk->nxtbkpt)
			if (bk->flag == BKPTTMP)
				bk->flag = BKPTCLR;
			else if (bk->flag != BKPTCLR)
				bk->flag = BKPTSET;
	}
	bpin = FALSE;
	ending = 0;
}

/*
 * start up the program to be debugged in a child
 */

void
setup(void)
{

	nnote = 0;
	startpcs();
	bpin = FALSE;
	pcsactive = 1;
}

/*
 * skip over a breakpoint:
 * remove breakpoints, then single step
 * so we can put it back
 */
void
execbkpt(BKPT *bk, int keepnote)
{
	runstep(bk->loc, keepnote);
	bk->flag = BKPTSET;
}

/*
 * find the breakpoint at adr, if any
 */

BKPT *
scanbkpt(ADDR adr)
{
	BKPT *bk;

	for (bk = bkpthead; bk; bk = bk->nxtbkpt)
		if (bk->flag != BKPTCLR && bk->loc == adr)
			break;
	return(bk);
}

/*
 * remove all breakpoints from the process' address space
 */

void
delbp(void)
{
	BKPT *bk;

	if (bpin == FALSE || pid == 0)
		return;
	for (bk = bkpthead; bk; bk = bk->nxtbkpt)
		if (bk->flag != BKPTCLR)
			bkput(bk, 0);
	bpin = FALSE;
}

/*
 * install all the breakpoints
 */

void
setbp(void)
{
	BKPT *bk;

	if (bpin == TRUE || pid == 0)
		return;
	for (bk = bkpthead; bk; bk = bk->nxtbkpt)
		if (bk->flag != BKPTCLR)
			bkput(bk, 1);
	bpin = TRUE;
}
