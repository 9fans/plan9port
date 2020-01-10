#include "a.h"

/*
 * Section 6 - line length and indenting.
 */

/* set line length */
void
ll(int v)
{
	if(v == 0)
		v = getnr(L(".l0"));
	nr(L(".l0"), getnr(L(".l")));
	nr(L(".l"), v);
}
void
r_ll(int argc, Rune **argv)
{
	if(argc < 2)
		ll(0);
	else if(argv[1][0] == '+')
		ll(getnr(L(".l"))+evalscale(argv[1]+1, 'v'));
	else if(argv[1][0] == '-')
		ll(getnr(L(".l"))-evalscale(argv[1]+1, 'v'));
	else
		ll(evalscale(argv[1], 'm'));
	if(argc > 2)
		warn("extra arguments to .ll");
}

void
in(int v)
{
	nr(L(".i0"), getnr(L(".i")));
	nr(L(".i"), v);
	nr(L(".ti"), 0);
	/* XXX */
}
void
r_in(int argc, Rune **argv)
{
	br();
	if(argc < 2)
		in(getnr(L(".i0")));
	else if(argv[1][0] == '+')
		in(getnr(L(".i"))+evalscale(argv[1]+1, 'm'));
	else if(argv[1][0] == '-')
		in(getnr(L(".i"))-evalscale(argv[1]+1, 'm'));
	else
		in(evalscale(argv[1], 'm'));
	if(argc > 3)
		warn("extra arguments to .in");
}

void
ti(int v)
{
	nr(L(".ti"), v);
}
void
r_ti(int argc, Rune **argv)
{
	USED(argc);
	br();
	ti(evalscale(argv[1], 'm'));
}

void
t6init(void)
{
	addreq(L("ll"), r_ll, -1);
	addreq(L("in"), r_in, -1);
	addreq(L("ti"), r_ti, 1);

	nr(L(".l"), eval(L("6.5i")));
}
