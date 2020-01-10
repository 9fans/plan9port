/*
 *
 * An array of type Pages is used to keep track of the starting and ending byte
 * offsets for the pages we've been asked to print.
 *
 */

typedef struct {
	long	start;			/* page starts at this byte offset */
	long	stop;			/* and ends here */
	int	empty;			/* dummy page if TRUE */
} Pages;

/*
 *
 * Some of the non-integer functions in postreverse.c.
 *
 */

char	*copystdin();
