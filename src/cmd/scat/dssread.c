#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#include	"sky.h"

static	void	dodecode(Biobuf*, Pix*, int, int, uchar*);
static	long	getlong(uchar*);
int	debug;

Img*
dssread(char *file)
{
	int nx, ny, scale, sumall;
	Pix  *p, *pend;
	uchar buf[21];
	Biobuf *bp;
	Img *ip;

	if(debug)
		Bprint(&bout, "reading %s\n", file);
	bp = Bopen(file, OREAD);
	if(bp == 0)
		return 0;
	if(Bread(bp, buf, sizeof(buf)) != sizeof(buf) ||
	   buf[0] != 0xdd || buf[1] != 0x99){
		werrstr("bad format");
		return 0;
	}
	nx = getlong(buf+2);
	ny = getlong(buf+6);
	scale = getlong(buf+10);
	sumall = getlong(buf+14);
	if(debug)
		fprint(2, "%s: nx=%d, ny=%d, scale=%d, sumall=%d, nbitplanes=%d,%d,%d\n",
			file, nx, ny, scale, sumall, buf[18], buf[19], buf[20]);
	ip = malloc(sizeof(Img) + (nx*ny-1)*sizeof(int));
	if(ip == 0){
		Bterm(bp);
		werrstr("no memory");
		return 0;
	}
	ip->nx = nx;
	ip->ny = ny;
	dodecode(bp, ip->a, nx, ny, buf+18);
	ip->a[0] = sumall;	/* sum of all pixels */
	Bterm(bp);
	if(scale > 1){
		p = ip->a;
		pend = &ip->a[nx*ny];
		while(p < pend)
			*p++ *= scale;
	}
	hinv(ip->a, nx, ny);
	return ip;
}

static
void
dodecode(Biobuf *infile, Pix  *a, int nx, int ny, uchar *nbitplanes)
{
	int nel, nx2, ny2, bits, mask;
	Pix *aend, px;

	nel = nx*ny;
	nx2 = (nx+1)/2;
	ny2 = (ny+1)/2;
	memset(a, 0, nel*sizeof(*a));

	/*
	 * Initialize bit input
	 */
	start_inputing_bits();

	/*
	 * read bit planes for each quadrant
	 */
	qtree_decode(infile, &a[0],          ny, nx2,  ny2,  nbitplanes[0]);
	qtree_decode(infile, &a[ny2],        ny, nx2,  ny/2, nbitplanes[1]);
	qtree_decode(infile, &a[ny*nx2],     ny, nx/2, ny2,  nbitplanes[1]);
	qtree_decode(infile, &a[ny*nx2+ny2], ny, nx/2, ny/2, nbitplanes[2]);

	/*
	 * make sure there is an EOF symbol (nybble=0) at end
	 */
	if(input_nybble(infile) != 0){
		fprint(2, "dodecode: bad bit plane values\n");
		exits("format");
	}

	/*
	 * get the sign bits
	 */
	aend = &a[nel];
	mask = 0;
	bits = 0;;
	for(; a<aend; a++) {
		if(px = *a) {
			if(mask == 0) {
				mask = 0x80;
				bits = Bgetc(infile);
			}
			if(mask & bits)
				*a = -px;
			mask >>= 1;
		}
	}
}

static
long	getlong(uchar *p)
{
	return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}
