/*
 *
 * The font data for a printer is saved in an array of the following type.
 *
 */

typedef struct map {
	char	*font;		/* a request for this PostScript font */
	char	*file;		/* means copy this unix file */
	int	downloaded;	/* TRUE after *file is downloaded */
} Map;

Map	*allocate();
