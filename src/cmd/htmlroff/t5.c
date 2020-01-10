#include "a.h"

/*
 * 5.  Vertical spacing.
 */

/* set vertical baseline spacing */
void
vs(int v)
{
	if(v == 0)
		v = getnr(L(".v0"));
	nr(L(".v0"), getnr(L(".v")));
	nr(L(".v"), v);
}

void
r_vs(int argc, Rune **argv)
{
	if(argc < 2)
		vs(eval(L("12p")));
	else if(argv[1][0] == '+')
		vs(getnr(L(".v"))+evalscale(argv[1]+1, 'p'));
	else if(argv[1][0] == '-')
		vs(getnr(L(".v"))-evalscale(argv[1]+1, 'p'));
	else
		vs(evalscale(argv[1], 'p'));
}

/* set line spacing */
void
ls(int v)
{
	if(v == 0)
		v = getnr(L(".ls0"));
	nr(L(".ls0"), getnr(L(".ls")));
	nr(L(".ls"), v);
}
void
r_ls(int argc, Rune **argv)
{
	ls(argc < 2 ? 0 : eval(argv[1]));
}

/* .sp - space vertically */
/* .sv - save a contiguous vertical block */
void
sp(int v)
{
	Rune buf[100];
	double fv;

	br();
	fv = v * 1.0/UPI;
	if(fv > 5)
		fv = eval(L("1v")) * 1.0/UPI;
	runesnprint(buf, nelem(buf), "<p style=\"margin-top: 0; margin-bottom: %.2fin\"></p>\n", fv);
	outhtml(buf);
}
void
r_sp(int argc, Rune **argv)
{
	if(getnr(L(".ns")))
		return;
	if(argc < 2)
		sp(eval(L("1v")));
	else{
		if(argv[1][0] == '|'){
			/* XXX if there's no output yet, do the absolute! */
			if(verbose)
				warn("ignoring absolute .sp %d", eval(argv[1]+1));
			return;
		}
		sp(evalscale(argv[1], 'v'));
	}
}

void
r_ns(int argc, Rune **argv)
{
	USED(argc);
	USED(argv);
	nr(L(".ns"), 1);
}

void
r_rs(int argc, Rune **argv)
{
	USED(argc);
	USED(argv);
	nr(L(".ns"), 0);
}

void
t5init(void)
{
	addreq(L("vs"), r_vs, -1);
	addreq(L("ls"), r_ls, -1);
	addreq(L("sp"), r_sp, -1);
	addreq(L("sv"), r_sp, -1);
	addreq(L("os"), r_nop, -1);
	addreq(L("ns"), r_ns, 0);
	addreq(L("rs"), r_rs, 0);

	nr(L(".v"), eval(L("12p")));
	nr(L(".v0"), eval(L("12p")));
	nr(L(".ls"), 1);
	nr(L(".ls0"), 1);
}
