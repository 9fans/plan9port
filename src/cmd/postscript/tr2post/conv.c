#include <u.h>
#include <libc.h>
#include <bio.h>
#include "../common/common.h"
#include "tr2post.h"

void
conv(Biobufhdr *Bp) {
	long c, n;
	int r;
	char special[10];
	int save;

	inputlineno = 1;
	if (debug) Bprint(Bstderr, "conv(Biobufhdr *Bp=0x%x)\n", Bp);
	while ((r = Bgetrune(Bp)) >= 0) {
/* Bprint(Bstderr, "r=<%c>,0x%x\n", r, r); */
/*		Bflush(Bstderr); */
		switch (r) {
		case 's':	/* set point size */
			Bgetfield(Bp, 'd', &fontsize, 0);
			break;
		case 'f':	/* set font to postion */
			Bgetfield(Bp, 'd', &fontpos, 0);
			save = inputlineno;
			settrfont();
			inputlineno = save;	/* ugh */
			break;
		case 'c':	/* print rune */
			r = Bgetrune(Bp);
			runeout(r);
			break;
		case 'C':	/* print special character */
			Bgetfield(Bp, 's', special, 10);
			specialout(special);
			break;
		case 'N':	/* print character with numeric value from current font */
			Bgetfield(Bp, 'd', &n, 0);
			break;
		case 'H':	/* go to absolute horizontal position */
			Bgetfield(Bp, 'd', &n, 0);
			hgoto(n);
			break;
		case 'V':	/* go to absolute vertical position */
			Bgetfield(Bp, 'd', &n, 0);
			vgoto(n);
			break;
		case 'h':	/* go to relative horizontal position */
			Bgetfield(Bp, 'd', &n, 0);
			hmot(n);
			break;
		case 'v':	/* go to relative vertical position */
			Bgetfield(Bp, 'd', &n, 0);
			vmot(n);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
				/* move right nn units, then print character c */
			n = (r - '0') * 10;
			r = Bgetrune(Bp);
			if (r < 0)
				error(FATAL, "EOF or error reading input\n");
			else if (r < '0' || r > '9')
				error(FATAL, "integer expected\n");
			n += r - '0';
			r = Bgetrune(Bp);
			hmot(n);
			runeout(r);
			break;
		case 'p':	/* begin page */
			Bgetfield(Bp, 'd', &n, 0);
			endpage();
			startpage();
			break;
		case 'n':	/* end of line (information only 'b a' follows) */
			Brdline(Bp, '\n');	/* toss rest of line */
			inputlineno++;
			break;
		case 'w':	/* paddable word space (information only) */
			break;
		case 'D':	/* graphics function */
			draw(Bp);
			break;
		case 'x':	/* device control functions */
			devcntl(Bp);
			break;
		case '#':	/* comment */
			Brdline(Bp, '\n');	/* toss rest of line */
		case '\n':
			inputlineno++;
			break;
		default:
			error(WARNING, "unknown troff function <%c>\n", r);
			break;
		}
	}
	endpage();
	if (debug) Bprint(Bstderr, "r=0x%x\n", r);
	if (debug) Bprint(Bstderr, "leaving conv\n");
}
