#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

int
_loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	int y, l, lpart, rpart, mx, m, mr;
	uchar *q;

	if(!rectinrect(r, i->r))
		return -1;
	l = bytesperline(r, i->depth);
	if(ndata < l*Dy(r))
		return -1;
	ndata = l*Dy(r);
	q = byteaddr(i, r.min);
	mx = 7/i->depth;
	lpart = (r.min.x & mx) * i->depth;
	rpart = (r.max.x & mx) * i->depth;
	m = 0xFF >> lpart;
	/* may need to do bit insertion on edges */
	if(l == 1){	/* all in one byte */
		if(rpart)
			m ^= 0xFF >> rpart;
		for(y=r.min.y; y<r.max.y; y++){
			*q ^= (*data^*q) & m;
			q += i->width*sizeof(u32int);
			data++;
		}
		return ndata;
	}
	if(lpart==0 && rpart==0){	/* easy case */
		for(y=r.min.y; y<r.max.y; y++){
			memmove(q, data, l);
			q += i->width*sizeof(u32int);
			data += l;
		}
		return ndata;
	}
	mr = 0xFF ^ (0xFF >> rpart);
	if(lpart!=0 && rpart==0){
		for(y=r.min.y; y<r.max.y; y++){
			*q ^= (*data^*q) & m;
			if(l > 1)
				memmove(q+1, data+1, l-1);
			q += i->width*sizeof(u32int);
			data += l;
		}
		return ndata;
	}
	if(lpart==0 && rpart!=0){
		for(y=r.min.y; y<r.max.y; y++){
			if(l > 1)
				memmove(q, data, l-1);
			q[l-1] ^= (data[l-1]^q[l-1]) & mr;
			q += i->width*sizeof(u32int);
			data += l;
		}
		return ndata;
	}
	for(y=r.min.y; y<r.max.y; y++){
		*q ^= (*data^*q) & m;
		if(l > 2)
			memmove(q+1, data+1, l-2);
		q[l-1] ^= (data[l-1]^q[l-1]) & mr;
		q += i->width*sizeof(u32int);
		data += l;
	}
	return ndata;
}
