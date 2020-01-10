/*
 *
 * Boundingbox code for PostScript translators. The boundingbox for each page
 * is accumulated in bbox - the one for the whole document goes in docbbox. A
 * call to writebbox() puts out an appropriate comment, updates docbbox, and
 * resets bbox for the next page. The assumption made at the end of writebbox()
 * is that we're really printing the current page only if output is now going
 * to stdout - a valid assumption for all supplied translators. Needs the math
 * library.
 *
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <math.h>

#include "comments.h"			/* PostScript file structuring comments */
#include "gen.h"			/* a few general purpose definitions */
#include "ext.h"			/* external variable declarations */

typedef struct bbox {
	int	set;
	double	llx, lly;
	double	urx, ury;
} Bbox;

Bbox	bbox = {FALSE, 0.0, 0.0, 0.0, 0.0};
Bbox	docbbox = {FALSE, 0.0, 0.0, 0.0, 0.0};

double	ctm[6] = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
double	matrix1[6], matrix2[6];

/*****************************************************************************/

void
cover(x, y)

    double	x, y;

{

/*
 *
 * Adds point (x, y) to bbox. Coordinates are in user space - the transformation
 * to default coordinates happens in writebbox().
 *
 */

    if ( bbox.set == FALSE ) {
	bbox.llx = bbox.urx = x;
	bbox.lly = bbox.ury = y;
	bbox.set = TRUE;
    } else {
	if ( x < bbox.llx )
	    bbox.llx = x;
	if ( y < bbox.lly )
	    bbox.lly = y;
	if ( x > bbox.urx )
	    bbox.urx = x;
	if ( y > bbox.ury )
	    bbox.ury = y;
    }	/* End else */

}   /* End of cover */

/*****************************************************************************/
void	resetbbox(int);

void
writebbox(fp, keyword, slop)

    FILE	*fp;			/* the comment is written here */
    char	*keyword;		/* the boundingbox comment string */
    int		slop;			/* expand (or contract?) the box a bit */

{

    Bbox	ubbox;			/* user space bounding box */
    double	x, y;

/*
 *
 * Transforms the numbers in the bbox[] using ctm[], adjusts the corners a bit
 * (depending on slop) and then writes comment. If *keyword is BoundingBox use
 * whatever's been saved in docbbox, otherwise assume the comment is just for
 * the current page.
 *
 */

    if ( strcmp(keyword, BOUNDINGBOX) == 0 )
	bbox = docbbox;

    if ( bbox.set == TRUE ) {
	ubbox = bbox;
	bbox.set = FALSE;		/* so cover() works properly */
	x = ctm[0] * ubbox.llx + ctm[2] * ubbox.lly + ctm[4];
	y = ctm[1] * ubbox.llx + ctm[3] * ubbox.lly + ctm[5];
	cover(x, y);
	x = ctm[0] * ubbox.llx + ctm[2] * ubbox.ury + ctm[4];
	y = ctm[1] * ubbox.llx + ctm[3] * ubbox.ury + ctm[5];
	cover(x, y);
	x = ctm[0] * ubbox.urx + ctm[2] * ubbox.ury + ctm[4];
	y = ctm[1] * ubbox.urx + ctm[3] * ubbox.ury + ctm[5];
	cover(x, y);
	x = ctm[0] * ubbox.urx + ctm[2] * ubbox.lly + ctm[4];
	y = ctm[1] * ubbox.urx + ctm[3] * ubbox.lly + ctm[5];
	cover(x, y);
	bbox.llx -= slop + 0.5;
	bbox.lly -= slop + 0.5;
	bbox.urx += slop + 0.5;
	bbox.ury += slop + 0.5;
	fprintf(fp, "%s %d %d %d %d\n", keyword, (int)bbox.llx, (int)bbox.lly,(int)bbox.urx, (int)bbox.ury);
	bbox = ubbox;
    }	/* End if */

    resetbbox((fp == stdout) ? TRUE : FALSE);

}   /* End of writebbox */

/*****************************************************************************/
void
resetbbox(output)

    int		output;

{

/*
 *
 * Adds bbox to docbbox and resets bbox for the next page. Only update docbbox
 * if we really did output on the last page.
 *
 */

    if ( docbbox.set == TRUE ) {
	cover(docbbox.llx, docbbox.lly);
	cover(docbbox.urx, docbbox.ury);
    }	/* End if */

    if ( output == TRUE ) {
	docbbox = bbox;
	docbbox.set = TRUE;
    }	/* End if */

    bbox.set = FALSE;

}   /* End of resetbbox */

/*****************************************************************************/
void
scale(sx, sy)

    double	sx, sy;

{

/*
 *
 * Scales the default matrix.
 *
 */

    matrix1[0] = sx;
    matrix1[1] = 0;
    matrix1[2] = 0;
    matrix1[3] = sy;
    matrix1[4] = 0;
    matrix1[5] = 0;

    concat(matrix1);

}   /* End of scale */

/*****************************************************************************/
void
translate(tx, ty)

    double	tx, ty;

{

/*
 *
 * Translates the default matrix.
 *
 */

    matrix1[0] = 1.0;
    matrix1[1] = 0.0;
    matrix1[2] = 0.0;
    matrix1[3] = 1.0;
    matrix1[4] = tx;
    matrix1[5] = ty;

    concat(matrix1);

}   /* End of translate */

/*****************************************************************************/
void
rotate(angle)

    double	angle;

{

/*
 *
 * Rotates by angle degrees.
 *
 */

    angle *= 3.1416 / 180;

    matrix1[0] = matrix1[3] = cos(angle);
    matrix1[1] = sin(angle);
    matrix1[2] = -matrix1[1];
    matrix1[4] = 0.0;
    matrix1[5] = 0.0;

    concat(matrix1);

}   /* End of rotate */

/*****************************************************************************/

void
concat(m1)

    double	m1[];

{

    double	m2[6];

/*
 *
 * Replaces the ctm[] by the result of the matrix multiplication m1[] x ctm[].
 *
 */

    m2[0] = ctm[0];
    m2[1] = ctm[1];
    m2[2] = ctm[2];
    m2[3] = ctm[3];
    m2[4] = ctm[4];
    m2[5] = ctm[5];

    ctm[0] = m1[0] * m2[0] + m1[1] * m2[2];
    ctm[1] = m1[0] * m2[1] + m1[1] * m2[3];
    ctm[2] = m1[2] * m2[0] + m1[3] * m2[2];
    ctm[3] = m1[2] * m2[1] + m1[3] * m2[3];
    ctm[4] = m1[4] * m2[0] + m1[5] * m2[2] + m2[4];
    ctm[5] = m1[4] * m2[1] + m1[5] * m2[3] + m2[5];

}   /* End of concat */

/*****************************************************************************/
