#include "a.h"

/*
 * Section 3 - page control (mostly irrelevant).
 */

/* page offset */
void
po(int o)
{
	nr(L(".o0"), getnr(L(".o")));
	nr(L(".o"), o);
}

void
r_po(int argc, Rune **argv)
{
	if(argc == 1){
		po(getnr(L(".o0")));
		return;
	}
	if(argv[1][0] == '+')
		po(getnr(L(".o"))+evalscale(argv[1]+1, 'v'));
	else if(argv[1][0] == '-')
		po(getnr(L(".o"))-evalscale(argv[1]+1, 'v'));
	else
		po(evalscale(argv[1], 'v'));
}

/* .ne - need vertical space */
/* .mk - mark current vertical place */
/* .rt - return upward */

void
t3init(void)
{
	nr(L(".o"), eval(L("1i")));
	nr(L(".o0"), eval(L("1i")));
	nr(L(".p"), eval(L("11i")));

	addreq(L("pl"), r_warn, -1);
	addreq(L("bp"), r_nop, -1);
	addreq(L("pn"), r_warn, -1);
	addreq(L("po"), r_po, -1);
	addreq(L("ne"), r_nop, -1);
	addreq(L("mk"), r_nop, -1);
	addreq(L("rt"), r_warn, -1);
}
