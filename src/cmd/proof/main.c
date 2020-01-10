#include	<u.h>
#include	<libc.h>
#include	<draw.h>
#include	<event.h>
#include	<bio.h>
#include	"proof.h"

Rectangle rpage = { 0, 0, 850, 1150 };
char devname[64];
double mag = DEFMAG;
int dbg = 0;
char *track = 0;
Biobuf bin;
char *libfont = "#9/font";
char *mapfile = "MAP";
char *mapname = "MAP";

void
usage(void)
{
	fprint(2, "usage: proof [-m mag] [-/ nview] [-x xoff] [-y yoff] [-M mapfile] [-F fontdir] [-dt] file...\n");
	exits("usage");
}

double
getnum(char *s)
{
	if(s == nil)
		usage();
	return atof(s);
}

char*
getstr(char *s)
{
	if(s == nil)
		usage();
	return s;
}

void
main(int argc, char *argv[])
{
	char c;
	int dotrack = 0;

	libfont = unsharp(libfont);
	ARGBEGIN{
	case 'm':	/* magnification */
		mag = getnum(ARGF());
		if (mag < 0.1 || mag > 10){
			fprint(2, "ridiculous mag argument ignored\n");
			mag = DEFMAG;
		}
		break;
	case '/':
		nview = getnum(ARGF());
		if (nview < 1 || nview > MAXVIEW)
			nview = 1;
		break;
	case 'x':
		xyoffset.x += getnum(ARGF()) * 100;
		break;
	case 'y':
		xyoffset.y += getnum(ARGF()) * 100;
		break;
	case 'M':	/* change MAP file */
		mapname = EARGF(usage());
		break;
	case 'F':	/* change /lib/font/bit directory */
		libfont = EARGF(usage());
		break;
	case 'd':
		dbg = 1;
		break;
	case 't':
		dotrack = 1;
		break;
	default:
		usage();
	}ARGEND

	if (argc > 0) {
		close(0);
		if (open(argv[0], 0) != 0) {
			sysfatal("can't open %s: %r\n", argv[0]);
			exits("open failure");
		}
		if(dotrack)
			track = argv[0];
	}
	Binit(&bin, 0, OREAD);
	mapfile = smprint("%s/%s", libfont, mapname);
	readmapfile(mapfile);
	for (c = 0; c < NFONT; c++)
		loadfontname(c, "??");
	mapscreen();
	clearscreen();
	readpage();
}

/*
 * Input buffer to allow us to back up
 */
#define	SIZE	100000	/* 8-10 pages, typically */

char	bufc[SIZE];
char	*inc = bufc;		/* where next input character goes */
char	*outc = bufc;	/* next character to be read from buffer */
int	off;		/* position of outc in total input stream */

void
addc(int c)
{
	*inc++ = c;
	if(inc == &bufc[SIZE])
		inc = &bufc[0];
}

int
getc(void)
{
	int c;

	if(outc == inc){
		c = Bgetc(&bin);
		if(c == Beof)
			return Beof;
		addc(c);
	}
	off++;
	c = *outc++;
	if(outc == &bufc[SIZE])
		outc = &bufc[0];
	return c;
}

int
getrune(void)
{
	int c, n;
	Rune r;
	char buf[UTFmax];

	for(n=0; !fullrune(buf, n); n++){
		c = getc();
		if(c == Beof)
			return Beof;
		buf[n] = c;
	}
	chartorune(&r, buf);
	return r;
}

int
nbuf(void)	/* return number of buffered characters */
{
	int ini, outi;

	ini = inc-bufc;
	outi = outc-bufc;
	if(ini < outi)
		ini += SIZE;
	return ini-outi;
}

ulong
seekc(ulong o)
{
	ulong avail;
	long delta;

	delta = off-o;
	if(delta < 0)
		return Beof;
	avail = SIZE-nbuf();
	if(delta < avail){
		off = o;
		outc -= delta;
		if(outc < &bufc[0])
			outc += SIZE;
		return off;
	}
	return Beof;
}

void
ungetc(void)
{
	if(off == 0)
		return;
	if(nbuf() == SIZE){
		fprint(2, "backup buffer overflow\n");
		return;
	}
	if(outc == &bufc[0])
		outc = &bufc[SIZE];
	--outc;
	--off;
}

ulong
offsetc(void)
{
	return off;
}

char*
rdlinec(void)
{
	static char buf[2048];
	int c, i;

	for(i=0; i<sizeof buf; ){
		c = getc();
		if(c == Beof)
			break;
		buf[i++] = c;
		if(c == '\n')
			break;
	}

	if(i == 0)
		return nil;
	return buf;
}
