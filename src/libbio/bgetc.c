#include	"lib9.h"
#include	<bio.h>

int
Bgetc(Biobuf *bp)
{
	int i;

loop:
	i = bp->icount;
	if(i != 0) {
		bp->icount = i+1;
		return bp->ebuf[i];
	}
	if(bp->state != Bractive) {
		if(bp->state == Bracteof)
			bp->state = Bractive;
		return Beof;
	}
	/*
	 * get next buffer, try to keep Bungetsize
	 * characters pre-catenated from the previous
	 * buffer to allow that many ungets.
	 */
	memmove(bp->bbuf-Bungetsize, bp->ebuf-Bungetsize, Bungetsize);
	i = read(bp->fid, bp->bbuf, bp->bsize);
	bp->gbuf = bp->bbuf;
	if(i <= 0) {
		if(i < 0)
			bp->state = Binactive;
		return Beof;
	}
	if(i < bp->bsize) {
		memmove(bp->ebuf-i-Bungetsize, bp->bbuf-Bungetsize, i+Bungetsize);
		bp->gbuf = bp->ebuf-i;
	}
	bp->icount = -i;
	bp->offset += i;
	goto loop;
}

int
Bungetc(Biobuf *bp)
{

	if(bp->state == Bracteof)
		bp->state = Bractive;
	if(bp->state != Bractive)
		return Beof;
	bp->icount--;
	return 1;
}
