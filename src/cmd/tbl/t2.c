/* t2.c:  subroutine sequencing for one table */
# include "t.h"
void
tableput(void)
{
	saveline();
	savefill();
	ifdivert();
	cleanfc();
	getcomm();
	getspec();
	gettbl();
	getstop();
	checkuse();
	choochar();
	maktab();
	runout();
	release();
	rstofill();
	endoff();
	freearr();
	restline();
}
