#include	"lib9.h"
#include	<bio.h>

int
Bprint(Biobuf *bp, char *fmt, ...)
{
	va_list	ap;
	char *ip, *ep, *out;
	int n;

	ep = (char*)bp->ebuf;
	ip = ep + bp->ocount;
	va_start(ap, fmt);
	out = vseprint(ip, ep, fmt, ap);
	va_end(ap);
	if(out == 0 || out >= ep-5) {
		Bflush(bp);
		ip = ep + bp->ocount;
		va_start(ap, fmt);
		out = vseprint(ip, ep, fmt, ap);
		va_end(ap);
		if(out >= ep-5)
			return Beof;
	}
	n = out-ip;
	bp->ocount += n;
	return n;
}
