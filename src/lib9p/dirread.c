#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

void
dirread9p(Req *r, Dirgen *gen, void *aux)
{
	int start;
	uchar *p, *ep;
	uint rv;
	Dir d;

	if(r->ifcall.offset == 0)
		start = 0;
	else
		start = r->fid->dirindex;

	p = (uchar*)r->ofcall.data;
	ep = p+r->ifcall.count;

	while(p < ep){
		memset(&d, 0, sizeof d);
		if((*gen)(start, &d, aux) < 0)
			break;
		rv = convD2M(&d, p, ep-p);
		free(d.name);
		free(d.muid);
		free(d.uid);
		free(d.gid);
		if(rv <= BIT16SZ)
			break;
		p += rv;
		start++;
	}
	r->fid->dirindex = start;
	r->ofcall.count = p - (uchar*)r->ofcall.data;
}
