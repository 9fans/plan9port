#include "lib9.h"
#include <bio.h>

static int
_Bfmtflush(Fmt *f)
{
	Biobuf *b;

	b = f->farg;
	b->ocount = (char*)f->to - (char*)f->stop;
	if(Bflush(b) < 0)
		return 0;
	f->to = b->ebuf+b->ocount;
	return 1;
}

int
Bfmtinit(Fmt *f, Biobuf *b)
{
	if(b->state != Bwactive)
		return -1;
	memset(f, 0, sizeof *f);
	f->farg = b;
	f->start = b->bbuf;
	f->to = b->ebuf+b->ocount;
	f->stop = b->ebuf;
	f->flush = _Bfmtflush;
	return 0;
}

int
Bfmtflush(Fmt *f)
{
	if(_Bfmtflush(f) <= 0)
		return -1;
	return f->nfmt;
}
