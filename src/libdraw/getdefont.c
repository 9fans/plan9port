#include <u.h>
#include <libc.h>
#include <draw.h>

Subfont*
getdefont(Display *d)
{
	char *hdr, *p;
	int n;
	Fontchar *fc;
	Subfont *f;
	int ld;
	Rectangle r;
	Image *i;

	/*
	 * make sure data is word-aligned.  this is true with Plan 9 compilers
	 * but not in general.  the byte order is right because the data is
	 * declared as char*, not ulong*.
	 */
	p = (char*)defontdata;
	n = (ulong)p & 3;
	if(n != 0){
		memmove(p+(4-n), p, sizeofdefont-n);
		p += 4-n;
	}
	ld = atoi(p+0*12);
	r.min.x = atoi(p+1*12);
	r.min.y = atoi(p+2*12);
	r.max.x = atoi(p+3*12);
	r.max.y = atoi(p+4*12);

	i = allocimage(d, r, drawld2chan[ld], 0, 0);
	if(i == 0)
		return 0;

	p += 5*12;
	n = loadimage(i, r, (uchar*)p, (defontdata+sizeofdefont)-(uchar*)p);
	if(n < 0){
		freeimage(i);
		return 0;
	}

	hdr = p+n;
	n = atoi(hdr);
	p = hdr+3*12;
	fc = malloc(sizeof(Fontchar)*(n+1));
	if(fc == 0){
		freeimage(i);
		return 0;
	}
	_unpackinfo(fc, (uchar*)p, n);
	f = allocsubfont("*default*", n, atoi(hdr+12), atoi(hdr+24), fc, i);
	if(f == 0){
		freeimage(i);
		free(fc);
		return 0;
	}
	return f;
}
