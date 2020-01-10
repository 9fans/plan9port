#include	<u.h>
#include	<libc.h>
#include	<libg.h>
#include	<bio.h>
#include	"hdr.h"

enum {
	Charsperfont	= 157,
	Void1b		= (0xC6-0xA2)*Charsperfont+0x3f,
	Void1e		= (0xC9-0xA2)*Charsperfont - 1
};

Bitmap *
breadbits(char *file, int n, long *chars, int size, uchar *bits, int **doneptr)
{
	Bitmap *bm;
	Biobuf *bf;
	int i, j, byt;
	int nch;
	long c;
	uchar *b, *nbits;
	int *done;
	static uchar missing[32] = {0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x04, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x80, 0x00, 0x40, 0x00, 0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	uchar buf[32];

	bf = Bopen(file, OREAD);
	if(bf == 0){
		fprint(2, "%s: %s: %r\n", argv0, file);
		exits("bitfile open error");
	}
	done = (int *)malloc(n*sizeof(done[0]));
	if(done == 0){
		fprint(2, "%s: malloc error (%d bytes)\n", argv0, n);
		exits("malloc error");
	}
	*doneptr = done;
	byt = size/8;
	nch = 0;
	for(i = 0; i < n; i++){
		done[i] = 1;
		nch++;
		c = chars[i];
		if((c >= Void1b) && (c <= Void1e)){
			done[i] = 0;
			nch--;
			continue;
		}
		/* magic via presotto for calculating the seek */
		j = c;
		if(c >= 2*Charsperfont)
			j += 294;	/* baffling hole between A3 and A4 */
		if(c > Void1e)
			j -= 3*Charsperfont - 0x3F;
		j *= byt*size;		/* bytes per char */
		j += 256;		/* 256 front matter */
		Bseek(bf, j, 0);
		Bread(bf, buf, sizeof(missing));
		if(memcmp(buf, missing, sizeof(missing)) == 0){
			done[i] = 0;
			nch--;
			continue;
		}
		Bseek(bf, j, 0);
		b = bits + i*byt;
		for(j = 0; j < size; j++){	/* rows */
			Bread(bf, b, byt);
			b += n*byt;
		}
	}
	nbits = (uchar *)malloc(nch*byt*size);
	if(nbits == 0){
		fprint(2, "%s: malloc error (%d bytes)\n", argv0, nch*byt);
		exits("malloc error");
	}
	c = 0;
	for(i = 0; i < n; i++)
		if(done[i]){
			for(j = 0; j < size; j++)
				memmove(nbits+c*byt+j*nch*byt, bits+i*byt+j*n*byt, byt);
			c++;
		}
	bm = balloc((Rectangle){(Point){0, 0}, (Point){nch*size, size}}, 0);
	if(bm == 0){
		fprint(2, "%s: balloc failure\n", argv0);
		exits("balloc failure");
	}
	wrbitmap(bm, 0, size, nbits);
	return(bm);
}
