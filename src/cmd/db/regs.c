/*
 * code to keep track of registers
 */

#include "defs.h"
#include "fns.h"

/*
 * print the registers
 */
void
printregs(int c)
{
	Regdesc *rp;
	int i;
	ADDR u;

	if(correg == nil){
		dprint("registers not mapped\n");
		return;
	}

	for (i = 1, rp = mach->reglist; rp->name; rp++, i++) {
		if ((rp->flags & RFLT)) {
			if (c != 'R')
				continue;
			if (rp->format == '8' || rp->format == '3')
				continue;
		}
		rget(correg, rp->name, &u);
		if(rp->format == 'Y')
			dprint("%-8s %-20#llux", rp->name, (uvlong)u);
		else
			dprint("%-8s %-12#lux", rp->name, (ulong)u);
		if ((i % 3) == 0) {
			dprint("\n");
			i = 0;
		}
	}
	if (i != 1)
		dprint("\n");
	dprint ("%s\n", mach->exc(cormap, correg));
	printpc();
}
