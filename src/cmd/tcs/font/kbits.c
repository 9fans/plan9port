#include	<u.h>
#include	<libc.h>
#include	<libg.h>
#include	<bio.h>
#include	"hdr.h"

Bitmap *
kreadbits(char *file, int n, long *chars, int size, uchar *bits, int **doneptr)
{
	Bitmap *bm;
	Biobuf *bf;
	char *p;
	long kmin, kmax;
	int i, j, byt;
	int nch;
	long c;
	uchar *b;
	int *done;
	uchar *nbits;
	static int dig[256] = {
	['0'] 0, ['1'] 1, ['2'] 2, ['3'] 3, ['4'] 4, ['5'] 5, ['6'] 6, ['7'] 7,
	['8'] 8, ['9'] 9, ['a'] 10, ['b'] 11, ['c'] 12, ['d'] 13, ['e'] 14, ['f'] 15,
	};

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
	memset(done, 0, n*sizeof(done[0]));
	kmin = 65536;
	kmax = 0;
	for(i = 0; i < n; i++){
		c = chars[i];
		if(kmin > c)
			kmin = c;
		if(kmax < c)
			kmax = c;
	}
	nch = 0;
	byt = size/8;
	Brdline(bf, '\n');		/* header line 1 */
	Brdline(bf, '\n');		/* header line 2 */
	while(p = (char *)Brdline(bf, '\n')){
		c = strtol(p+17, (char **)0, 10);
		if((c < kmin) || (c > kmax))
			continue;
		for(i = 0; i < n; i++)
			if(c == chars[i]){
				nch++;
				done[i] = 1;
				p += 25;
				b = bits + i*byt;
				for(i = 0; i < size; i++){	/* rows */
					for(j = 0; j < byt; j++){
						*b++ = (dig[*p]<<4) | dig[p[1]];
						p += 2;
					}
					b += (n-1)*byt;
				}
				break;
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
	for(i = 0; i < n; i++)
		if(done[i] == 0){
			/*fprint(2, "char 0x%x (%d) not found\n", chars[i], chars[i]);/**/
		}
	return(bm);
}
