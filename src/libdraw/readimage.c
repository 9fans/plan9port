#include <u.h>
#include <libc.h>
#include <draw.h>

Image*
readimage(Display *d, int fd, int dolock)
{
	char hdr[5*12+1];
	int dy;
	int new;
	uint l, n;
	int m, j, chunk;
	int miny, maxy;
	Rectangle r;
	int ldepth;
	u32int chan;
	uchar *tmp;
	Image *i;

	if(readn(fd, hdr, 11) != 11)
		return nil;
	if(memcmp(hdr, "compressed\n", 11) == 0)
		return creadimage(d, fd, dolock);
	if(readn(fd, hdr+11, 5*12-11) != 5*12-11)
		return nil;
	if(d)
		chunk = d->bufsize - 32;	/* a little room for header */
	else
		chunk = 8192;

	/*
	 * distinguish new channel descriptor from old ldepth.
	 * channel descriptors have letters as well as numbers,
	 * while ldepths are a single digit formatted as %-11d.
	 */
	new = 0;
	for(m=0; m<10; m++){
		if(hdr[m] != ' '){
			new = 1;
			break;
		}
	}
	if(hdr[11] != ' '){
		werrstr("readimage: bad format");
		return nil;
	}
	if(new){
		hdr[11] = '\0';
		if((chan = strtochan(hdr)) == 0){
			werrstr("readimage: bad channel string %s", hdr);
			return nil;
		}
	}else{
		ldepth = ((int)hdr[10])-'0';
		if(ldepth<0 || ldepth>3){
			werrstr("readimage: bad ldepth %d", ldepth);
			return nil;
		}
		chan = drawld2chan[ldepth];
	}

	r.min.x = atoi(hdr+1*12);
	r.min.y = atoi(hdr+2*12);
	r.max.x = atoi(hdr+3*12);
	r.max.y = atoi(hdr+4*12);
	if(r.min.x>r.max.x || r.min.y>r.max.y){
		werrstr("readimage: bad rectangle");
		return nil;
	}

	miny = r.min.y;
	maxy = r.max.y;

	l = bytesperline(r, chantodepth(chan));
	if(d){
		if(dolock)
			lockdisplay(d);
		i = allocimage(d, r, chan, 0, -1);
		if(dolock)
			unlockdisplay(d);
		if(i == nil)
			return nil;
	}else{
		i = mallocz(sizeof(Image), 1);
		if(i == nil)
			return nil;
	}

	tmp = malloc(chunk);
	if(tmp == nil)
		goto Err;
	while(maxy > miny){
		dy = maxy - miny;
		if(dy*l > chunk)
			dy = chunk/l;
		if(dy <= 0){
			werrstr("readimage: image too wide for buffer");
			goto Err;
		}
		n = dy*l;
		m = readn(fd, tmp, n);
		if(m != n){
			werrstr("readimage: read count %d not %d: %r", m, n);
   Err:
			if(dolock)
				lockdisplay(d);
   Err1:
 			freeimage(i);
			if(dolock)
				unlockdisplay(d);
			free(tmp);
			return nil;
		}
		if(!new)	/* an old image: must flip all the bits */
			for(j=0; j<chunk; j++)
				tmp[j] ^= 0xFF;

		if(d){
			if(dolock)
				lockdisplay(d);
			if(loadimage(i, Rect(r.min.x, miny, r.max.x, miny+dy), tmp, chunk) <= 0)
				goto Err1;
			if(dolock)
				unlockdisplay(d);
		}
		miny += dy;
	}
	free(tmp);
	return i;
}
