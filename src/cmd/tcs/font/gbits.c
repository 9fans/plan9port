#include	<u.h>
#include	<libc.h>
#include	<libg.h>
#include	<bio.h>
#include	"hdr.h"

/*
	this file reads bdf fonts. it is mapping dependent in that the ENCODING
	field is font dependent.
*/

static char *field(Biobuf *, char *);

Bitmap *
greadbits(char *file, int n, long *chars, int size, uchar *bits, int **doneptr)
{
	Bitmap *bm;
	Biobuf *bf;
	char *p, *s;
	long kmin, kmax;
	int i, j, byt;
	int nch;
	long c;
	uchar *b;
	int *done;
	uchar *nbits;
	char buf[1024];	/* big enough for one char! */
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
	for(;;){
		while(s = Brdline(bf, '\n')){
			if(strncmp(s, "STARTCHAR", 9) == 0)
				break;
		}
		if(s == 0)
			break;
		s = field(bf, "ENCODING");
		c = strtol(s, (char **)0, 10);
		c = (c/256 - 0xA0)*100 + (c%256 - 0xA0);
		field(bf, "BITMAP");
		p = buf;
		for(i = 0; i < size; i++){	/* rows */
			if((s = Brdline(bf, '\n')) == 0){
				exits("bad bdf");
			}
			memcpy(p, s, 2*byt);
			p += 2*byt;
		}
		field(bf, "ENDCHAR");
		if((c < kmin) || (c > kmax))
			continue;
		for(i = 0; i < n; i++)
			if(c == chars[i]){
				nch++;
				done[i] = 1;
				p = buf;
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

static char *
field(Biobuf *bf, char *name)
{
	char *s;
	int n;

	n = strlen(name);
	while(s = Brdline(bf, '\n')){
		if(strncmp(s, name, n) == 0)
			return(s+n);
		if(strncmp(s, "ENDCHAR", 7) == 0){
			fprint(2, "%s: char missing %s field\n", argv0, name);
			exits("bad bdf");
		}
	}
	fprint(2, "%s: char missing %s field; unexpected EOF\n", argv0, name);
	exits("bad bdf");
	return(0);
}
