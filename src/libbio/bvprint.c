#include "lib9.h"
#include <bio.h>

static int 
fmtBflush(Fmt *f)
{
	Biobuf *bp;

	bp = f->farg;
	bp->ocount = (char*)f->to - (char*)f->stop;
	if(Bflush(bp) < 0)
		return 0;
	f->stop = bp->ebuf;
	f->to = (char*)f->stop + bp->ocount;
	f->start = f->to;
	return 1;
}

int
Bvprint(Biobuf *bp, char *fmt, va_list arg)
{
	int n;
	Fmt f;

	f.runes = 0;
	f.stop = bp->ebuf;
	f.start = (char*)f.stop + bp->ocount;
	f.to = f.start;
	f.flush = fmtBflush;
	f.farg = bp;
	f.nfmt = 0;
	n = fmtvprint(&f, fmt, arg);
	bp->ocount = (char*)f.to - (char*)f.stop;
	return n;
}


	
