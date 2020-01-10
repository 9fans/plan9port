/*
 *
 * A few definitions that shouldn't have to change. Used by most programs in
 * this package.
 *
 */

#define PROGRAMVERSION	"3.3.2"

/* XXX: replace tempnam with something safer, but leaky */
extern	char*	safe_tempnam(char*, char*);
#define	tempnam	safe_tempnam

#define NON_FATAL	0
#define FATAL		1
#define USER_FATAL	2

#define OFF		0
#define ON		1

#define FALSE		0
#define TRUE		1

#define BYTE		8
#define BMASK		0377

#define POINTS		72.3

#ifndef PI
#define PI		3.141592654
#endif

#define ONEBYTE		0
#define UTFENCODING	1

#define READING		ONEBYTE
#define WRITING		ONEBYTE

/*
 *
 * DOROUND controls whether some translators include file ROUNDPAGE (path.h)
 * after the prologue. Used to round page dimensions obtained from the clippath
 * to know paper sizes. Enabled by setting DOROUND to TRUE (or 1).
 *
 */

#define DOROUND	TRUE

/*
 *
 * Default resolution and the height and width of a page (in case we need to get
 * to upper left corner) - only used in BoundingBox calculations!!
 *
 */

#define DEFAULT_RES	72
#define PAGEHEIGHT	11.0 * DEFAULT_RES
#define PAGEWIDTH	8.5 * DEFAULT_RES

/*
 *
 * Simple macros.
 *
 */

#define ABS(A)		((A) >= 0 ? (A) : -(A))
#define MIN(A, B)	((A) < (B) ? (A) : (B))
#define MAX(A, B)	((A) > (B) ? (A) : (B))
