#include	<u.h>
#include	<libc.h>
#include	<libg.h>
#include	<bio.h>
#include	"hdr.h"

/*
	produces the bitmap for the designated han characters
*/

static void usage(void);
enum { Jis = 0, Big5, Gb_bdf, Gb_qw };
enum { Size24 = 0, Size16 };
struct {
	char *names[2];
	mapfn *mfn;
	readbitsfn *bfn;
} source[] = {
	[Jis] { "../han/jis.bits", "../han/jis16.bits", kmap, kreadbits },
	[Big5] { "no 24 bit file", "../han/big5.16.bits", bmap, breadbits },
	[Gb_bdf] { "no 24 bit file", "../han/cclib16fs.bdf", gmap, greadbits },
	[Gb_qw] { "no 24 bit file", "no 16bit file", gmap, qreadbits },
};

void
main(int argc, char **argv)
{
	int from, to;
	int size = 24;
	int src = Jis;
	char *file = 0;
	long nc, nb;
	int x;
	uchar *bits;
	long *chars;
	int raw = 0;
	Bitmap *b, *b1;
	Subfont *f;
	int *found;

	ARGBEGIN{
	case 'f':	file = ARGF(); break;
	case 'r':	raw = 1; break;
	case '5':	src = Big5; break;
	case 's':	size = 16; break;
	case 'g':	src = Gb_bdf; break;
	case 'q':	src = Gb_qw; break;
	default:	usage();
	}ARGEND
	if(file == 0)
		file = source[src].names[(size==24)? Size24:Size16];
	if(argc != 2)
		usage();
	from = strtol(argv[0], (char **)0, 0);
	to = strtol(argv[1], (char **)0, 0);
	binit(0, 0, "fontgen");
	nc = to-from+1;
	nb = size*size/8;		/* bytes per char */
	nb *= nc;
	bits = (uchar *)malloc(nb);
	chars = (long *)malloc(sizeof(long)*nc);
	found = (int *)malloc(sizeof(found[0])*nc);
	if(bits == 0 || chars == 0){
		fprint(2, "%s: couldn't malloc %d bytes for %d chars\n", argv0, nb, nc);
		exits("out of memory");
	}
	if(raw){
		for(x = from; x <= to; x++)
			chars[x-from] = x;
	} else
		source[src].mfn(from, to, chars);
	memset(bits, 0, nb);
	b = source[src].bfn(file, nc, chars, size, bits, &found);
	b1 = balloc(b->r, b->ldepth);
	bitblt(b1, b1->r.min, b, b->r, S);
	f = bf(nc, size, b1, found);
	wrbitmapfile(1, b);
	wrsubfontfile(1, f);/**/
	exits(0);
}

static void
usage(void)
{
	fprint(2, "Usage: %s [-s] from to\n", argv0);
	exits("usage");
}
