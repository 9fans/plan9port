#include	"lib9.h"
#include	<bio.h>

long long
Bseek(Biobuf *bp, long long offset, int base)
{
	vlong n, d;
	int bufsz;

	switch(bp->state) {
	default:
		fprint(2, "Bseek: unknown state %d\n", bp->state);
		return Beof;

	case Bracteof:
		bp->state = Bractive;
		bp->icount = 0;
		bp->gbuf = bp->ebuf;

	case Bractive:
		n = offset;
		if(base == 1) {
			n += Boffset(bp);
			base = 0;
		}

		/*
		 * try to seek within buffer
		 */
		if(base == 0) {
			d = n - Boffset(bp);
			bufsz = bp->ebuf - bp->gbuf;
			if(-bufsz <= d && d <= bufsz){
				bp->icount += d;
				if(d >= 0) {
					if(bp->icount <= 0)
						return n;
				} else {
					if(bp->ebuf - bp->gbuf >= -bp->icount)
						return n;
				}
			}
		}

		/*
		 * reset the buffer
		 */
		n = lseek(bp->fid, n, base);
		bp->icount = 0;
		bp->gbuf = bp->ebuf;
		break;

	case Bwactive:
		Bflush(bp);
		n = seek(bp->fid, offset, base);
		break;
	}
	bp->offset = n;
	return n;
}
