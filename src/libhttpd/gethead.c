#include <u.h>
#include <libc.h>
#include <bin.h>
#include <httpd.h>

/*
 * read in some header lines, either one or all of them.
 * copy results into header log buffer.
 */
int
hgethead(HConnect *c, int many)
{
	Hio *hin;
	char *s, *p, *pp;
	int n;

	hin = &c->hin;
	for(;;){
		s = (char*)hin->pos;
		pp = s;
		while(p = memchr(pp, '\n', (char*)hin->stop - pp)){
			if(!many || p == pp || (p == pp + 1 && *pp == '\r')){
				pp = p + 1;
				break;
			}
			pp = p + 1;
		}
		hin->pos = (uchar*)pp;
		n = pp - s;
		if(c->hstop + n > &c->header[HBufSize])
			return -1;
		memmove(c->hstop, s, n);
		c->hstop += n;
		*c->hstop = '\0';
		if(p != nil)
			return 0;
		if(hreadbuf(hin, hin->pos) == nil || hin->state == Hend)
			return -1;
	}
}
