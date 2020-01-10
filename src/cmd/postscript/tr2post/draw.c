#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include "../common/common.h"
#include "tr2post.h"

BOOLEAN drawflag = FALSE;
BOOLEAN	inpath = FALSE;			/* TRUE if we're putting pieces together */

void
cover(double x, double y) {
}

void
drawspline(Biobuf *Bp, int flag) {	/* flag!=1 connect end points */
	int x[100], y[100];
	int i, N;
/*
 *
 * Spline drawing routine for Postscript printers. The complicated stuff is
 * handled by procedure Ds, which should be defined in the library file. I've
 * seen wrong implementations of troff's spline drawing, so fo the record I'll
 * write down the parametric equations and the necessary conversions to Bezier
 * cubic splines (as used in Postscript).
 *
 *
 * Parametric equation (x coordinate only):
 *
 *
 *	    (x2 - 2 * x1 + x0)    2                    (x0 + x1)
 *	x = ------------------ * t   + (x1 - x0) * t + ---------
 *		    2					   2
 *
 *
 * The coefficients in the Bezier cubic are,
 *
 *
 *	A = 0
 *	B = (x2 - 2 * x1 + x0) / 2
 *	C = x1 - x0
 *
 *
 * while the current point is,
 *
 *	current-point = (x0 + x1) / 2
 *
 * Using the relationships given in the Postscript manual (page 121) it's easy to
 * see that the control points are given by,
 *
 *
 *	x0' = (x0 + 5 * x1) / 6
 *	x1' = (x2 + 5 * x1) / 6
 *	x2' = (x1 + x2) / 2
 *
 *
 * where the primed variables are the ones used by curveto. The calculations
 * shown above are done in procedure Ds using the coordinates set up in both
 * the x[] and y[] arrays.
 *
 * A simple test of whether your spline drawing is correct would be to use cip
 * to draw a spline and some tangent lines at appropriate points and then print
 * the file.
 *
 */

	for (N=2; N<sizeof(x)/sizeof(x[0]); N++)
		if (Bgetfield(Bp, 'd', &x[N], 0)<=0 || Bgetfield(Bp, 'd', &y[N], 0)<=0)
			break;

	x[0] = x[1] = hpos;
	y[0] = y[1] = vpos;

	for (i = 1; i < N; i++) {
		x[i+1] += x[i];
		y[i+1] += y[i];
	}

	x[N] = x[N-1];
	y[N] = y[N-1];

	for (i = ((flag!=1)?0:1); i < ((flag!=1)?N-1:N-2); i++) {
		endstring();
		if (pageon())
			Bprint(Bstdout, "%d %d %d %d %d %d Ds\n", x[i], y[i], x[i+1], y[i+1], x[i+2], y[i+2]);
/*		if (dobbox == TRUE) {		/* could be better */
/*	    		cover((double)(x[i] + x[i+1])/2,(double)-(y[i] + y[i+1])/2);
/*	    		cover((double)x[i+1], (double)-y[i+1]);
/*	    		cover((double)(x[i+1] + x[i+2])/2, (double)-(y[i+1] + y[i+2])/2);
/*		}
 */
	}

	hpos = x[N];			/* where troff expects to be */
	vpos = y[N];
}

void
draw(Biobuf *Bp) {

	int r, x1, y1, x2, y2, i;
	int d1, d2;

	drawflag = TRUE;
	r = Bgetrune(Bp);
	switch(r) {
	case 'l':
		if (Bgetfield(Bp, 'd', &x1, 0)<=0 || Bgetfield(Bp, 'd', &y1, 0)<=0 || Bgetfield(Bp, 'r', &i, 0)<=0) {
			error(FATAL, "draw line function, destination coordinates not found.\n");
			return;
		}

		endstring();
		if (pageon())
			Bprint(Bstdout, "%d %d %d %d Dl\n", hpos, vpos, hpos+x1, vpos+y1);
		hpos += x1;
		vpos += y1;
		break;
	case 'c':
		if (Bgetfield(Bp, 'd', &d1, 0)<=0) {
			error(FATAL, "draw circle function, diameter coordinates not found.\n");
			return;
		}

		endstring();
		if (pageon())
			Bprint(Bstdout, "%d %d %d %d De\n", hpos, vpos, d1, d1);
		hpos += d1;
		break;
	case 'e':
		if (Bgetfield(Bp, 'd', &d1, 0)<=0 || Bgetfield(Bp, 'd', &d2, 0)<=0) {
			error(FATAL, "draw ellipse function, diameter coordinates not found.\n");
			return;
		}

		endstring();
		if (pageon())
			Bprint(Bstdout, "%d %d %d %d De\n", hpos, vpos, d1, d2);
		hpos += d1;
		break;
	case 'a':
		if (Bgetfield(Bp, 'd', &x1, 0)<=0 || Bgetfield(Bp, 'd', &y1, 0)<=0 || Bgetfield(Bp, 'd', &x2, 0)<=0 || Bgetfield(Bp, 'd', &y2, 0)<=0) {
			error(FATAL, "draw arc function, coordinates not found.\n");
			return;
		}

		endstring();
		if (pageon())
			Bprint(Bstdout, "%d %d %d %d %d %d Da\n", hpos, vpos, x1, y1, x2, y2);
		hpos += x1 + x2;
		vpos += y1 + y2;
		break;
	case 'q':
		drawspline(Bp, 1);
		break;
	case '~':
		drawspline(Bp, 2);
		break;
	default:
		error(FATAL, "unknown draw function <%c>\n", r);
		return;
	}
}

void
beginpath(char *buf, int copy) {

/*
 * Called from devcntrl() whenever an "x X BeginPath" command is read. It's used
 * to mark the start of a sequence of drawing commands that should be grouped
 * together and treated as a single path. By default the drawing procedures in
 * *drawfile treat each drawing command as a separate object, and usually start
 * with a newpath (just as a precaution) and end with a stroke. The newpath and
 * stroke isolate individual drawing commands and make it impossible to deal with
 * composite objects. "x X BeginPath" can be used to mark the start of drawing
 * commands that should be grouped together and treated as a single object, and
 * part of what's done here ensures that the PostScript drawing commands defined
 * in *drawfile skip the newpath and stroke, until after the next "x X DrawPath"
 * command. At that point the path that's been built up can be manipulated in
 * various ways (eg. filled and/or stroked with a different line width).
 *
 * Color selection is one of the options that's available in parsebuf(),
 * so if we get here we add *colorfile to the output file before doing
 * anything important.
 *
 */
	if (inpath == FALSE) {
		endstring();
	/*	getdraw();	*/
	/*	getcolor(); */
		Bprint(Bstdout, "gsave\n");
		Bprint(Bstdout, "newpath\n");
		Bprint(Bstdout, "%d %d m\n", hpos, vpos);
		Bprint(Bstdout, "/inpath true def\n");
		if ( copy == TRUE )
			Bprint(Bstdout, "%s\n", buf);
		inpath = TRUE;
	}
}

static void parsebuf(char*);

void
drawpath(char *buf, int copy) {

/*
 *
 * Called from devcntrl() whenever an "x X DrawPath" command is read. It marks the
 * end of the path started by the last "x X BeginPath" command and uses whatever
 * has been passed along in *buf to manipulate the path (eg. fill and/or stroke
 * the path). Once that's been done the drawing procedures are restored to their
 * default behavior in which each drawing command is treated as an isolated path.
 * The new version (called after "x X DrawPath") has copy set to FALSE, and calls
 * parsebuf() to figure out what goes in the output file. It's a feeble attempt
 * to free users and preprocessors (like pic) from having to know PostScript. The
 * comments in parsebuf() describe what's handled.
 *
 * In the early version a path was started with "x X BeginObject" and ended with
 * "x X EndObject". In both cases *buf was just copied to the output file, and
 * was expected to be legitimate PostScript that manipulated the current path.
 * The old escape sequence will be supported for a while (for Ravi), and always
 * call this routine with copy set to TRUE.
 *
 *
 */

	if ( inpath == TRUE ) {
		if ( copy == TRUE )
			Bprint(Bstdout, "%s\n", buf);
		else
			parsebuf(buf);
		Bprint(Bstdout, "grestore\n");
		Bprint(Bstdout, "/inpath false def\n");
/*		reset();		*/
		inpath = FALSE;
	}
}


/*****************************************************************************/

static void
parsebuf(char *buf)
{
	char	*p = (char*)0;			/* usually the next token */
	char *q;
	int		gsavelevel = 0;		/* non-zero if we've done a gsave */

/*
 *
 * Simple minded attempt at parsing the string that followed an "x X DrawPath"
 * command. Everything not recognized here is simply ignored - there's absolutely
 * no error checking and what was originally in buf is clobbered by strtok().
 * A typical *buf might look like,
 *
 *	gray .9 fill stroke
 *
 * to fill the current path with a gray level of .9 and follow that by stroking the
 * outline of the path. Since unrecognized tokens are ignored the last example
 * could also be written as,
 *
 *	with gray .9 fill then stroke
 *
 * The "with" and "then" strings aren't recognized tokens and are simply discarded.
 * The "stroke", "fill", and "wfill" force out appropriate PostScript code and are
 * followed by a grestore. In otherwords changes to the grahics state (eg. a gray
 * level or color) are reset to default values immediately after the stroke, fill,
 * or wfill tokens. For now "fill" gets invokes PostScript's eofill operator and
 * "wfill" calls fill (ie. the operator that uses the non-zero winding rule).
 *
 * The tokens that cause temporary changes to the graphics state are "gray" (for
 * setting the gray level), "color" (for selecting a known color from the colordict
 * dictionary defined in *colorfile), and "line" (for setting the line width). All
 * three tokens can be extended since strncmp() makes the comparison. For example
 * the strings "line" and "linewidth" accomplish the same thing. Colors are named
 * (eg. "red"), but must be appropriately defined in *colorfile. For now all three
 * tokens must be followed immediately by their single argument. The gray level
 * (ie. the argument that follows "gray") should be a number between 0 and 1, with
 * 0 for black and 1 for white.
 *
 * To pass straight PostScript through enclose the appropriate commands in double
 * quotes. Straight PostScript is only bracketed by the outermost gsave/grestore
 * pair (ie. the one from the initial "x X BeginPath") although that's probably
 * a mistake. Suspect I may have to change the double quote delimiters.
 *
 */

	for( ; p != nil ; p = q ) {
		if( q = strchr(p, ' ') ) {
			*q++ = '\0';
		}

		if ( gsavelevel == 0 ) {
			Bprint(Bstdout, "gsave\n");
			gsavelevel++;
		}
		if ( strcmp(p, "stroke") == 0 ) {
			Bprint(Bstdout, "closepath stroke\ngrestore\n");
			gsavelevel--;
		} else if ( strcmp(p, "openstroke") == 0 ) {
			Bprint(Bstdout, "stroke\ngrestore\n");
			gsavelevel--;
		} else if ( strcmp(p, "fill") == 0 ) {
			Bprint(Bstdout, "eofill\ngrestore\n");
			gsavelevel--;
		} else if ( strcmp(p, "wfill") == 0 ) {
			Bprint(Bstdout, "fill\ngrestore\n");
			gsavelevel--;
		} else if ( strcmp(p, "sfill") == 0 ) {
			Bprint(Bstdout, "eofill\ngrestore\ngsave\nstroke\ngrestore\n");
			gsavelevel--;
		} else if ( strncmp(p, "gray", strlen("gray")) == 0 ) {
			if( q ) {
				p = q;
				if ( q = strchr(p, ' ') )
					*q++ = '\0';
				Bprint(Bstdout, "%s setgray\n", p);
			}
		} else if ( strncmp(p, "color", strlen("color")) == 0 ) {
			if( q ) {
				p = q;
				if ( q = strchr(p, ' ') )
					*q++ = '\0';
				Bprint(Bstdout, "/%s setcolor\n", p);
			}
		} else if ( strncmp(p, "line", strlen("line")) == 0 ) {
			if( q ) {
				p = q;
				if ( q = strchr(p, ' ') )
					*q++ = '\0';
				Bprint(Bstdout, "%s resolution mul 2 div setlinewidth\n", p);
			}
		} else if ( strncmp(p, "reverse", strlen("reverse")) == 0 )
			Bprint(Bstdout, "reversepath\n");
		else if ( *p == '"' ) {
			for ( ; gsavelevel > 0; gsavelevel-- )
				Bprint(Bstdout, "grestore\n");
			if ( q != nil )
				*--q = ' ';
			if ( (q = strchr(p, '"')) != nil ) {
				*q++ = '\0';
				Bprint(Bstdout, "%s\n", p);
			}
		}
	}

	for ( ; gsavelevel > 0; gsavelevel-- )
		Bprint(Bstdout, "grestore\n");

}
