#include	"lib9.h"
#include	<bio.h>

static char*
badd(char *p, int *np, char *data, int ndata, int delim, int nulldelim)
{
	int n;

	n = *np;
	p = realloc(p, n+ndata+1);
	if(p){
		memmove(p+n, data, ndata);
		n += ndata;
		if(n>0 && nulldelim && p[n-1]==delim)
			p[--n] = '\0';
		else
			p[n] = '\0';
		*np = n;
	}
	return p;
}

char*
Brdstr(Biobuf *bp, int delim, int nulldelim)
{
	char *ip, *ep, *p;
	int i, j;

	i = -bp->icount;
	bp->rdline = 0;
	if(i == 0) {
		/*
		 * eof or other error
		 */
		if(bp->state != Bractive) {
			if(bp->state == Bracteof)
				bp->state = Bractive;
			bp->gbuf = bp->ebuf;
			return nil;
		}
	}

	/*
	 * first try in remainder of buffer (gbuf doesn't change)
	 */
	ip = (char*)bp->ebuf - i;
	ep = memchr(ip, delim, i);
	if(ep) {
		j = (ep - ip) + 1;
		bp->icount += j;
		return badd(nil, &bp->rdline, ip, j, delim, nulldelim);
	}

	/*
	 * copy data to beginning of buffer
	 */
	if(i < bp->bsize)
		memmove(bp->bbuf, ip, i);
	bp->gbuf = bp->bbuf;

	/*
	 * append to buffer looking for the delim
	 */
	p = nil;
	for(;;){
		ip = (char*)bp->bbuf + i;
		while(i < bp->bsize) {
			j = read(bp->fid, ip, bp->bsize-i);
			if(j <= 0 && i == 0)
				return p;
			if(j <= 0 && i > 0){
				/*
				 * end of file but no delim. pretend we got a delim
				 * by making the delim \0 and smashing it with nulldelim.
				 */
				j = 1;
				ep = ip;
				delim = '\0';
				nulldelim = 1;
				*ep = delim;	/* there will be room for this */
			}else{
				bp->offset += j;
				ep = memchr(ip, delim, j);
			}
			i += j;
			if(ep) {
				/*
				 * found in new piece
				 * copy back up and reset everything
				 */
				ip = (char*)bp->ebuf - i;
				if(i < bp->bsize){
					memmove(ip, bp->bbuf, i);
					bp->gbuf = (unsigned char*)ip;
				}
				j = (ep - (char*)bp->bbuf) + 1;
				bp->icount = j - i;
				return badd(p, &bp->rdline, ip, j, delim, nulldelim);
			}
			ip += j;
		}

		/*
		 * full buffer without finding; add to user string and continue
		 */
		p = badd(p, &bp->rdline, (char*)bp->bbuf, bp->bsize, 0, 0);
		i = 0;
		bp->icount = 0;
		bp->gbuf = bp->ebuf;
	}
}
