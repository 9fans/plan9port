#include	"lib9.h"
#include	<bio.h>

long
Bwrite(Biobuf *bp, void *ap, long count)
{
	long c;
	unsigned char *p;
	int i, n, oc;

	p = ap;
	c = count;
	oc = bp->ocount;

	while(c > 0) {
		n = -oc;
		if(n > c)
			n = c;
		if(n == 0) {
			if(bp->state != Bwactive)
				return Beof;
			i = write(bp->fid, bp->bbuf, bp->bsize);
			if(i != bp->bsize) {
				bp->state = Binactive;
				return Beof;
			}
			bp->offset += i;
			oc = -bp->bsize;
			continue;
		}
		memmove(bp->ebuf+oc, p, n);
		oc += n;
		c -= n;
		p += n;
	}
	bp->ocount = oc;
	return count-c;
}
