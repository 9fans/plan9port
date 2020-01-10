#include "a.h"

/*
 * 13. Hyphenation.
 */

void
t13init(void)
{
	addreq(L("nh"), r_nop, -1);
	addreq(L("hy"), r_nop, -1);
	addreq(L("hc"), r_nop, -1);
	addreq(L("hw"), r_nop, -1);

	addesc('%', e_nop, 0);
}
