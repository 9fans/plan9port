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
fprint(2, "hgethead top %p - %p\n", hin->pos, hin->stop);
	for(;;){
		s = (char*)hin->pos;
		pp = s;
fprint(2, "hgethead %p - %p\n", pp, hin->stop);
		while(p = memchr(pp, '\n', (char*)hin->stop - pp)){
fprint(2, "hgethead %p - %p newline at %p %d\n", pp, hin->stop, p, *pp);
			if(!many || p == pp || (p == pp + 1 && *pp == '\r')){
fprint(2, "breaking\n");
				pp = p + 1;
				break;
			}
			pp = p + 1;
		}
		hin->pos = (uchar*)pp;
		n = pp - s;
		if(c->hstop + n > &c->header[HBufSize])
			return 0;
		memmove(c->hstop, s, n);
		c->hstop += n;
		*c->hstop = '\0';
fprint(2, "p %p\n", p);
		if(p != nil)
			return 1;
		if(hreadbuf(hin, hin->pos) == nil || hin->state == Hend)
			return 0;
	}
}
