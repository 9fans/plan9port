/*
 *
 * External varibles - most are in glob.c.
 *
 */

extern char	**argv;			/* global so everyone can use them */
extern int	argc;

extern int	x_stat;			/* program exit status */
extern int	debug;			/* debug flag */
extern int	ignore;			/* what we do with FATAL errors */

extern long	lineno;			/* line number */
extern long	position;		/* byte position */
extern char	*prog_name;		/* and program name - for errors */
extern char	*temp_file;		/* temporary file - for some programs */
extern char	*fontencoding;		/* text font encoding scheme */

extern int	dobbox;			/* enable BoundingBox stuff if TRUE */
extern double	pageheight;		/* only for BoundingBox calculations! */
extern double	pagewidth;

extern int	reading;		/* input */
extern int	writing;		/* and output encoding */

extern char	*optarg;		/* for getopt() */
extern int	optind;

extern void	interrupt();
extern void	error();
extern int	cat();
extern void	concat();

/* 
 * extern char	*tempnam(char*,char*);
 * extern char	*malloc();
 * extern char	*calloc();
 * extern char	*strtok();
 * extern long	ftell();
 * extern double	atof();
 * extern double	sqrt();
 * extern double	atan2();
 */
