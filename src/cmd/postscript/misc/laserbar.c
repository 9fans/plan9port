/* laserbar -- filter to print barcodes on postscript printer */

#define MAIN 1

#define	LABEL	01
#define	NFLAG	02
#define	SFLAG	04

#include <stdio.h>
#include <ctype.h>

static int code39[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*	sp    !     "     #     $     %     &     '	*/
	0304, 0,    0,    0,    0250, 0052, 0,    0,
/*	(     )     *     +     ,     -     -     /	*/
	0,    0,    0224, 0212, 0,    0205, 0604, 0242,
/*	0     1     2     3     4     5     6     7	*/
	0064, 0441, 0141, 0540, 0061, 0460, 0160, 0045,
/*	8     9     :     ;     <     =     >     ?	*/
	0444, 0144, 0,    0,    0,    0,    0,    0,
/*	@     A     B     C     D     E     F     G	*/
	0,    0411, 0111, 0510, 0031, 0430, 0130, 0015,
/*	H     I     J     K     L     M     N     O	*/
	0414, 0114, 0034, 0403, 0103, 0502, 0023, 0422,
/*	P     Q     R     S     T     U     V     W	*/
	0122, 0007, 0406, 0106, 0026, 0601, 0301, 0700,
/*	X     Y     Z     [     \     ]     ^     _	*/
	0221, 0620, 0320, 0,    0,    0,    0,    0,
/*	`     a     b     c     d     e     f     g	*/
	0,    0411, 0111, 0510, 0031, 0430, 0130, 0015,
/*	h     i     j     k     l     m     n     o	*/
	0414, 0114, 0034, 0403, 0103, 0502, 0023, 0422,
/*	p     q     r     s     t     u     v     w	*/
	0122, 0007, 0406, 0106, 0026, 0601, 0301, 0700,
/*	x     y     z     {     |     }     ~     del	*/
	0221, 0620, 0320, 0,    0,    0,    0,    0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void barprt();
void laserbar();

#ifdef MAIN

main(argc, argv)
char **argv;
{
	int c, flags = 0, error = 0;
	double rotate = 0, xoffset = 0, yoffset = 0, xscale = 1, yscale = 1;
	extern char *optarg;
	extern int optind;
	extern double atof();
	extern void exit();

	while ((c = getopt(argc, argv, "r:x:y:X:Y:lns")) != EOF) {
		switch(c) {
		    case 'r':
			rotate = atof(optarg);
			break;
		    case 'x':
			xoffset = atof(optarg);
			break;
		    case 'y':
			yoffset = atof(optarg);
			break;
		    case 'X':
			xscale = atof(optarg);
			break;
		    case 'Y':
			yscale = atof(optarg);
			break;
		    case 'l':
			flags |= LABEL;
			break;
		    case 'n':
			flags |= NFLAG;
			break;
		    case 's':
			flags |= SFLAG;
			break;
		    case '?':
			++error;
		}
	}
	if ((argc - optind) != 1)
		++error;
	if (error) {
		(void) fprintf(stderr,
"Usage: %s [-r rotate] [-x xoffset] [-y yoffset] [-X xscale] [-Y yscale] [-lns] string\n",
		    *argv);
		exit(1);
	}
	laserbar(stdout, argv[optind], rotate, xoffset, yoffset, xscale, yscale, flags);
	return 0;
}

#endif /*MAIN*/

static int right = 0;

void
laserbar(fp, str, rotate, xoffset, yoffset, xscale, yscale, flags)
FILE *fp;
char *str;
double rotate, xoffset, yoffset, xscale, yscale;
int flags;
{
	xoffset *= 72.;
	yoffset *= 72.;
	(void) fprintf(fp, "gsave %s\n", (flags & NFLAG) ? "newpath" : "");
	if (xoffset || yoffset)
		(void) fprintf(fp, "%f %f moveto\n", xoffset, yoffset);
	if (xscale != 1 || yscale != 1)
		(void) fprintf(fp, "%f %f scale\n", xscale, yscale);
	if (rotate)
		(void) fprintf(fp, "%f rotate\n", rotate);
	(void) fputs("/Helvetica findfont 16 scalefont setfont\n", fp);
	(void) fputs("/w { 0 rmoveto gsave 3 setlinewidth 0 -72 rlineto stroke grestore } def\n", fp);
	(void) fputs("/n { 0 rmoveto gsave 1 setlinewidth 0 -72 rlineto stroke grestore } def\n", fp);
	(void) fputs("/l { gsave 2 -88 rmoveto show grestore } def\n", fp);
	barprt(fp, '*', 0);
	while (*str)
		barprt(fp, *(str++), (flags & LABEL));
	barprt(fp, '*', 0);
	(void) fprintf(fp, "%sgrestore\n", (flags & SFLAG) ? "showpage " : "");
	right = 0;
}

static void
barprt(fp, c, label)
FILE *fp;
int c, label;
{
	int i, mask, bar, wide;

	if (!(i = code39[c]))
		return;
	if (islower(c))
		c = toupper(c);
	if (label)
		(void) fprintf(fp, "(%c) l", c);
	else
		(void) fputs("     ", fp);
	for (bar = 1, mask = 0400; mask; bar = 1 - bar, mask >>= 1) {
		wide = mask & i;
		if (bar) {
			if (wide)
				++right;
			(void) fprintf(fp, " %d %s", right, wide ? "w" : "n");
			right = (wide ? 2 : 1);
		}
		else
			right += (wide ? 3 : 1);
	}
	(void) fputs("\n", fp);
	++right;
}
