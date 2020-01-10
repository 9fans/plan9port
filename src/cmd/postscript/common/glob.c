/*
 *
 * Global varibles - for PostScript translators.
 *
 */

#include <stdio.h>
#include "gen.h"

char	**argv;				/* global so everyone can use them */
int	argc;

int	x_stat = 0;			/* program exit status */
int	debug = OFF;			/* debug flag */
int	ignore = OFF;			/* what we do with FATAL errors */

long	lineno = 0;			/* line number */
long	position = 0;			/* byte position */
char	*prog_name = "";		/* and program name - for errors */
char	*temp_file = NULL;		/* temporary file - for some programs */
char	*fontencoding = NULL;		/* text font encoding scheme */

int	dobbox = FALSE;			/* enable BoundingBox stuff if TRUE */
double	pageheight = PAGEHEIGHT;	/* only for BoundingBox calculations! */
double	pagewidth = PAGEWIDTH;

int	reading = UTFENCODING;		/* input */
int	writing = WRITING;		/* and output encoding */
