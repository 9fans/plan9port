#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#include	"sky.h"

static void	qtree_expand(Biobuf*, uchar*, int, int, uchar*);
static void	qtree_copy(uchar*, int, int, uchar*, int);
static void	qtree_bitins(uchar*, int, int, Pix*, int, int);
static void	read_bdirect(Biobuf*, Pix*, int, int, int, uchar*, int);

void
qtree_decode(Biobuf *infile, Pix *a, int n, int nqx, int nqy, int nbitplanes)
{
	int log2n, k, bit, b, nqmax;
	int nx,ny,nfx,nfy,c;
	int nqx2, nqy2;
	unsigned char *scratch;

	/*
	 * log2n is log2 of max(nqx,nqy) rounded up to next power of 2
	 */
	nqmax = nqy;
	if(nqx > nqmax)
		nqmax = nqx;
	log2n = log(nqmax)/LN2+0.5;
	if (nqmax > (1<<log2n))
		log2n++;

	/*
	 * allocate scratch array for working space
	 */
	nqx2 = (nqx+1)/2;
	nqy2 = (nqy+1)/2;
	scratch = (uchar*)malloc(nqx2*nqy2);
	if(scratch == nil) {
		fprint(2, "qtree_decode: insufficient memory\n");
		exits("memory");
	}

	/*
	 * now decode each bit plane, starting at the top
	 * A is assumed to be initialized to zero
	 */
	for(bit = nbitplanes-1; bit >= 0; bit--) {
		/*
		 * Was bitplane was quadtree-coded or written directly?
		 */
		b = input_nybble(infile);
		if(b == 0) {
			/*
			 * bit map was written directly
			 */
			read_bdirect(infile, a, n, nqx, nqy, scratch, bit);
		} else
		if(b != 0xf) {
			fprint(2, "qtree_decode: bad format code %x\n",b);
			exits("format");
		} else {
			/*
			 * bitmap was quadtree-coded, do log2n expansions
			 * read first code
			 */

			scratch[0] = input_huffman(infile);

			/*
			 * do log2n expansions, reading codes from file as necessary
			 */
			nx = 1;
			ny = 1;
			nfx = nqx;
			nfy = nqy;
			c = 1<<log2n;
			for(k = 1; k<log2n; k++) {
				/*
				 * this somewhat cryptic code generates the sequence
				 * n[k-1] = (n[k]+1)/2 where n[log2n]=nqx or nqy
				 */
				c = c>>1;
				nx = nx<<1;
				ny = ny<<1;
				if(nfx <= c)
					nx--;
				else
					nfx -= c;
				if(nfy <= c)
					ny--;
				else
					nfy -= c;
				qtree_expand(infile, scratch, nx, ny, scratch);
			}

			/*
			 * copy last set of 4-bit codes to bitplane bit of array a
			 */
			qtree_bitins(scratch, nqx, nqy, a, n, bit);
		}
	}
	free(scratch);
}

/*
 * do one quadtree expansion step on array a[(nqx+1)/2,(nqy+1)/2]
 * results put into b[nqx,nqy] (which may be the same as a)
 */
static
void
qtree_expand(Biobuf *infile, uchar *a, int nx, int ny, uchar *b)
{
	uchar *b1;

	/*
	 * first copy a to b, expanding each 4-bit value
	 */
	qtree_copy(a, nx, ny, b, ny);

	/*
	 * now read new 4-bit values into b for each non-zero element
	 */
	b1 = &b[nx*ny];
	while(b1 > b) {
		b1--;
		if(*b1 != 0)
			*b1 = input_huffman(infile);
	}
}

/*
 * copy 4-bit values from a[(nx+1)/2,(ny+1)/2] to b[nx,ny], expanding
 * each value to 2x2 pixels
 * a,b may be same array
 */
static
void
qtree_copy(uchar *a, int nx, int ny, uchar *b, int n)
{
	int i, j, k, nx2, ny2;
	int s00, s10;

	/*
	 * first copy 4-bit values to b
	 * start at end in case a,b are same array
	 */
	nx2 = (nx+1)/2;
	ny2 = (ny+1)/2;
	k = ny2*(nx2-1) + ny2-1;	/* k   is index of a[i,j] */
	for (i = nx2-1; i >= 0; i--) {
		s00 = 2*(n*i+ny2-1);	/* s00 is index of b[2*i,2*j] */
		for (j = ny2-1; j >= 0; j--) {
			b[s00] = a[k];
			k -= 1;
			s00 -= 2;
		}
	}

	/*
	 * now expand each 2x2 block
	 */
	for(i = 0; i<nx-1; i += 2) {
		s00 = n*i;				/* s00 is index of b[i,j] */
		s10 = s00+n;				/* s10 is index of b[i+1,j] */
		for(j = 0; j<ny-1; j += 2) {
			b[s10+1] =  b[s00]     & 1;
			b[s10  ] = (b[s00]>>1) & 1;
			b[s00+1] = (b[s00]>>2) & 1;
			b[s00  ] = (b[s00]>>3) & 1;
			s00 += 2;
			s10 += 2;
		}
		if(j < ny) {
			/*
			 * row size is odd, do last element in row
			 * s00+1, s10+1 are off edge
			 */
			b[s10  ] = (b[s00]>>1) & 1;
			b[s00  ] = (b[s00]>>3) & 1;
		}
	}
	if(i < nx) {
		/*
		 * column size is odd, do last row
		 * s10, s10+1 are off edge
		 */
		s00 = n*i;
		for (j = 0; j<ny-1; j += 2) {
			b[s00+1] = (b[s00]>>2) & 1;
			b[s00  ] = (b[s00]>>3) & 1;
			s00 += 2;
		}
		if(j < ny) {
			/*
			 * both row and column size are odd, do corner element
			 * s00+1, s10, s10+1 are off edge
			 */
			b[s00  ] = (b[s00]>>3) & 1;
		}
	}
}

/*
 * Copy 4-bit values from a[(nx+1)/2,(ny+1)/2] to b[nx,ny], expanding
 * each value to 2x2 pixels and inserting into bitplane BIT of B.
 * A,B may NOT be same array (it wouldn't make sense to be inserting
 * bits into the same array anyway.)
 */
static
void
qtree_bitins(uchar *a, int nx, int ny, Pix *b, int n, int bit)
{
	int i, j;
	Pix *s00, *s10;
	Pix px;

	/*
	 * expand each 2x2 block
	 */
	for(i=0; i<nx-1; i+=2) {
		s00 = &b[n*i];			/* s00 is index of b[i,j] */
		s10 = s00+n;			/* s10 is index of b[i+1,j] */
		for(j=0; j<ny-1; j+=2) {
			px = *a++;
			s10[1] |= ( px     & 1) << bit;
			s10[0] |= ((px>>1) & 1) << bit;
			s00[1] |= ((px>>2) & 1) << bit;
			s00[0] |= ((px>>3) & 1) << bit;
			s00 += 2;
			s10 += 2;
		}
		if(j < ny) {
			/*
			 * row size is odd, do last element in row
			 * s00+1, s10+1 are off edge
			 */
			px = *a++;
			s10[0] |= ((px>>1) & 1) << bit;
			s00[0] |= ((px>>3) & 1) << bit;
		}
	}
	if(i < nx) {
		/*
		 * column size is odd, do last row
		 * s10, s10+1 are off edge
		 */
		s00 = &b[n*i];
		for(j=0; j<ny-1; j+=2) {
			px = *a++;
			s00[1] |= ((px>>2) & 1) << bit;
			s00[0] |= ((px>>3) & 1) << bit;
			s00 += 2;
		}
		if(j < ny) {
			/*
			 * both row and column size are odd, do corner element
			 * s00+1, s10, s10+1 are off edge
			 */
			s00[0] |= ((*a>>3) & 1) << bit;
		}
	}
}

static
void
read_bdirect(Biobuf *infile, Pix *a, int n, int nqx, int nqy, uchar *scratch, int bit)
{
	int i;

	/*
	 * read bit image packed 4 pixels/nybble
	 */
	for(i = 0; i < ((nqx+1)/2) * ((nqy+1)/2); i++) {
		scratch[i] = input_nybble(infile);
	}

	/*
	 * insert in bitplane BIT of image A
	 */
	qtree_bitins(scratch, nqx, nqy, a, n, bit);
}
