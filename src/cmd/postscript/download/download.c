/*
 *
 * download - host resident font downloader
 *
 * Prepends host resident fonts to PostScript input files. The program assumes
 * the input files are part of a single PostScript job and that requested fonts
 * can be downloaded at the start of each input file. Downloaded fonts are the
 * ones named in a %%DocumentFonts: comment and listed in a special map table.
 * Map table pathnames (supplied using the -m option) that begin with a / are
 * taken as is. Otherwise the final pathname is built using *hostfontdir (-H
 * option), *mapname (-m option), and *suffix.
 *
 * The map table consists of fontname-filename pairs, separated by white space.
 * Comments are introduced by % (as in PostScript) and extend to the end of the
 * current line. The only fonts that can be downloaded are the ones listed in
 * the active map table that point the program to a readable Unix file. A request
 * for an unlisted font or inaccessible file is ignored. All font requests are
 * ignored if the map table can't be read. In that case the program simply copies
 * the input files to stdout.
 *
 * An example (but not one to follow) of what can be in a map table is,
 *
 *	%
 *	% Map requests for Bookman-Light to file *hostfontdir/KR
 *	%
 *
 *	  Bookman-Light		KR	% Keeping everything (including the map
 *					% table) in *hostfontdir seems like the
 *					% cleanest approach.
 *
 *	%
 *	% Map Palatino-Roman to file *hostfontdir/palatino/Roman
 *	%
 *	  Palatino-Roman	palatino/Roman
 *
 *	% Map ZapfDingbats to file /usr/lib/host/dingbats
 *
 *	  ZapfDingbats		/usr/lib/host/dingbats
 *
 * Once again, file names that begin with a / are taken as is. All others have
 * *hostfontdir/ prepended to the file string associated with a particular font.
 *
 * Map table can be associated with a printer model (e.g. a LaserWriter), a
 * printer destination, or whatever - the choice is up to an administrator.
 * By destination may be best if your spooler is running several private
 * printers. Host resident fonts are usually purchased under a license that
 * restricts their use to a limited number of printers. A font licensed for
 * a single printer should only be used on that printer.
 *
 * Was written quickly, so there's much room for improvement. Undoubtedly should
 * be a more general program (e.g. scan for other comments).
 *
 */

#include <u.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libc.h>

#include "../common/ext.h"
#include "comments.h"			/* PostScript file structuring comments */
#include "gen.h"			/* general purpose definitions */
#include "path.h"			/* for temporary directory */
#include "ext.h"			/* external variable declarations */
#include "download.h"			/* a few special definitions */

char	*temp_dir = TEMPDIR;		/* temp directory - for copying stdin */
char	*hostfontdir = HOSTDIR;		/* host resident directory */
char	*mapname = "map";		/* map table - usually in *hostfontdir */
char	*suffix = "";			/* appended to the map table pathname */
Map	*map = NULL;			/* device font map table */
char	*stringspace = NULL;		/* for storing font and file strings */
int	next = 0;			/* next free slot in map[] */

char	*residentfonts = NULL;		/* list of printer resident fonts */
char	*printer = NULL;		/* printer name - only for Unix 4.0 lp */

char	buf[2048];			/* input file line buffer */
char	*comment = DOCUMENTFONTS;	/* look for this comment */
int	atend = FALSE;			/* TRUE only if a comment says so */

FILE	*fp_in;				/* next input file */
FILE	*fp_temp = NULL;		/* for copying stdin */

void init_signals(void);
void options(void);
void readmap(void);
void readresident(void);
void arguments(void);
void done(void);
void download(void);
int lookup(char *font);
void copyfonts(char *list);
void copyinput(void);
extern int cat(char *file);
extern void error(int errtype, char *fmt, ...);

/*****************************************************************************/

int
main(agc, agv)

    int		agc;
    char	*agv[];

{

/*
 *
 * Host resident font downloader. The input files are assumed to be part of a
 * single PostScript job.
 *
 */

    argc = agc;				/* other routines may want them */
    argv = agv;

    hostfontdir = unsharp(hostfontdir);

    fp_in = stdin;

    prog_name = argv[0];		/* just for error messages */

    init_signals();			/* sets up interrupt handling */
    options();				/* first get command line options */
    readmap();				/* read the font map table */
    readresident();			/* and the optional resident font list */
    arguments();			/* then process non-option arguments */
    done();				/* and clean things up */
    exit(x_stat);			/* not much could be wrong */

}   /* End of main */

/*****************************************************************************/

void
init_signals(void)
{

/*
 *
 * Makes sure we handle interrupts properly.
 *
 */

    if ( signal(SIGINT, interrupt) == SIG_IGN ) {
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
    } else {
	signal(SIGHUP, interrupt);
	signal(SIGQUIT, interrupt);
    }   /* End else */

    signal(SIGTERM, interrupt);

}   /* End of init_signals */

/*****************************************************************************/

void
options(void)
{

    int		ch;			/* return value from getopt() */
    char	*optnames = "c:fm:p:r:H:T:DI";

    extern char	*optarg;		/* used by getopt() */
    extern int	optind;

/*
 *
 * Reads and processes the command line options.
 *
 */

    while ( (ch = getopt(argc, argv, optnames)) != EOF ) {
	switch ( ch ) {
	    case 'c':			/* look for this comment */
		    comment = optarg;
		    break;

	    case 'f':			/* force a complete input file scan */
		    atend = TRUE;
		    break;

	    case 'm':			/* printer map table name */
		    mapname = optarg;
		    break;

	    case 'p':			/* printer name - for Unix 4.0 lp */
		    printer = optarg;
		    break;

	    case 'r':			/* resident font list */
		    residentfonts = optarg;
		    break;

	    case 'H':			/* host resident font directory */
		    hostfontdir = optarg;
		    break;

	    case 'T':			/* temporary file directory */
		    temp_dir = optarg;
		    break;

	    case 'D':			/* debug flag */
		    debug = ON;
		    break;

	    case 'I':			/* ignore FATAL errors */
		    ignore = ON;
		    break;

	    case '?':			/* don't understand the option */
		    error(FATAL, "");
		    break;

	    default:			/* don't know what to do for ch */
		    error(FATAL, "missing case for option %c\n", ch);
		    break;
	}   /* End switch */
    }   /* End while */

    argc -= optind;			/* get ready for non-option args */
    argv += optind;

}   /* End of options */

/*****************************************************************************/

void
readmap(void)
{

    char	*path;
    char	*ptr;
    int		fd;
    struct stat	sbuf;

/*
 *
 * Initializes the map table by reading an ASCII mapping file. If mapname begins
 * with a / it's the map table. Otherwise hostfontdir, mapname, and suffix are
 * combined to build the final pathname. If we can open the file we read it all
 * into memory, erase comments, and separate the font and file name pairs. When
 * we leave next points to the next free slot in the map[] array. If it's zero
 * nothing was in the file or we couldn't open it.
 *
 */

    if ( hostfontdir == NULL || mapname == NULL )
	return;

    if ( *mapname != '/' ) {
	if ( (path = (char *)malloc(strlen(hostfontdir) + strlen(mapname) +
						strlen(suffix) + 2)) == NULL )
	    error(FATAL, "no memory");
	sprintf(path, "%s/%s%s", hostfontdir, mapname, suffix);
    } else path = mapname;

    if ( (fd = open(path, 0)) != -1 ) {
	if ( fstat(fd, &sbuf) == -1 )
	    error(FATAL, "can't fstat %s", path);
	if ( (stringspace = (char *)malloc(sbuf.st_size + 2)) == NULL )
	    error(FATAL, "no memory");
	if ( read(fd, stringspace, sbuf.st_size) == -1 )
	    error(FATAL, "can't read %s", path);
	close(fd);

	stringspace[sbuf.st_size] = '\n';	/* just to be safe */
	stringspace[sbuf.st_size+1] = '\0';
	for ( ptr = stringspace; *ptr != '\0'; ptr++ )	/* erase comments */
	    if ( *ptr == '%' )
		for ( ; *ptr != '\n' ; ptr++ )
		    *ptr = ' ';

	for ( ptr = stringspace; ; next++ ) {
	    if ( (next % 50) == 0 )
		map = allocate(map, next+50);
	    map[next].downloaded = FALSE;
	    map[next].font = strtok(ptr, " \t\n");
	    map[next].file = strtok(ptr = NULL, " \t\n");
	    if ( map[next].font == NULL )
		break;
	    if ( map[next].file == NULL )
		error(FATAL, "map table format error - check %s", path);
	}   /* End for */
    }	/* End if */

}   /* End of readmap */

/*****************************************************************************/

void
readresident(void)
{

    FILE	*fp;
    char	*path;
    int		ch;
    int		n;

/*
 *
 * Reads a file that lists the resident fonts for a particular printer and marks
 * each font as already downloaded. Nothing's done if the file can't be read or
 * there's no mapping file. Comments, as in the map file, begin with a % and
 * extend to the end of the line. Added for Unix 4.0 lp.
 *
 */

    if ( next == 0 || (printer == NULL && residentfonts == NULL) )
	return;

    if ( printer != NULL ) {		/* use Unix 4.0 lp pathnames */
	sprintf(buf, "%s/printers/%s", HOSTDIR, printer);
	path = unsharp(buf);
    } else path = residentfonts;

    if ( (fp = fopen(path, "r")) != NULL ) {
	while ( fscanf(fp, "%s", buf) != EOF )
	    if ( buf[0] == '%' )
		while ( (ch = getc(fp)) != EOF && ch != '\n' ) ;
	    else if ( (n = lookup(buf)) < next )
		map[n].downloaded = TRUE;
	fclose(fp);
    }	/* End if */

}   /* End of readresident */

/*****************************************************************************/

void
arguments(void)
{

/*
 *
 * Makes sure all the non-option command line arguments are processed. If we get
 * here and there aren't any arguments left, or if '-' is one of the input files
 * we'll translate stdin. Assumes input files are part of a single PostScript
 * job and fonts can be downloaded at the start of each file.
 *
 */

    if ( argc < 1 )
	download();
    else {
	while ( argc > 0 ) {
	    fp_temp = NULL;
	    if ( strcmp(*argv, "-") == 0 )
		fp_in = stdin;
	    else if ( (fp_in = fopen(*argv, "r")) == NULL )
		error(FATAL, "can't open %s", *argv);
	    download();
	    if ( fp_in != stdin )
		fclose(fp_in);
	    if ( fp_temp != NULL )
		fclose(fp_temp);
	    argc--;
	    argv++;
	}   /* End while */
    }	/* End else */

}   /* End of arguments */

/*****************************************************************************/

void
done(void)
{

/*
 *
 * Clean things up before we quit.
 *
 */

    if ( temp_file != NULL )
	unlink(temp_file);

}   /* End of done */

/*****************************************************************************/

void
download(void)
{

    int		infontlist = FALSE;

/*
 *
 * If next is zero the map table is empty and all we do is copy the input file
 * to stdout. Otherwise we read the input file looking for %%DocumentFonts: or
 * continuation comments, add any accessible fonts to the output file, and then
 * append the input file. When reading stdin we append lines to fp_temp and
 * recover them when we're ready to copy the input file. fp_temp will often
 * only contain part of stdin - if there's no %%DocumentFonts: (atend) comment
 * we stop reading fp_in after the header.
 *
 */

    if ( next > 0 ) {
	if ( fp_in == stdin ) {
	    if ( (temp_file = tempnam(temp_dir, "post")) == NULL )
		error(FATAL, "can't generate temp file name");
	    if ( (fp_temp = fopen(temp_file, "w+r")) == NULL )
		error(FATAL, "can't open %s", temp_file);
	    unlink(temp_file);
	}   /* End if */

	while ( fgets(buf, sizeof(buf), fp_in) != NULL ) {
	    if ( fp_temp != NULL )
		fprintf(fp_temp, "%s", buf);
	    if ( buf[0] != '%' || buf[1] != '%' ) {
		if ( (buf[0] != '%' || buf[1] != '!') && atend == FALSE )
		    break;
		infontlist = FALSE;
	    } else if ( strncmp(buf, comment, strlen(comment)) == 0 ) {
		copyfonts(buf);
		infontlist = TRUE;
	    } else if ( buf[2] == '+' && infontlist == TRUE )
		copyfonts(buf);
	    else infontlist = FALSE;
	}   /* End while */
    }	/* End if */

    copyinput();

}   /* End of download */

/*****************************************************************************/

void
copyfonts(list)

    char	*list;

{

    char	*font;
    char	*path;
    int		n;

/*
 *
 * list points to a %%DocumentFonts: or continuation comment. What follows the
 * the keyword will be a list of fonts separated by white space (or (atend)).
 * Look for each font in the map table and if it's found copy the font file to
 * stdout (once only).
 *
 */

    strtok(list, " \n");		/* skip to the font list */

    while ( (font = strtok(NULL, " \t\n")) != NULL ) {
	if ( strcmp(font, ATEND) == 0 ) {
	    atend = TRUE;
	    break;
	}   /* End if */
	if ( (n = lookup(font)) < next ) {
	    if ( *map[n].file != '/' ) {
		if ( (path = (char *)malloc(strlen(hostfontdir)+strlen(map[n].file)+2)) == NULL )
		    error(FATAL, "no memory");
		sprintf(path, "%s/%s", hostfontdir, map[n].file);
		cat(path);
		free(path);
	    } else cat(map[n].file);
	    map[n].downloaded = TRUE;
	}   /* End if */
    }	/* End while */

}   /* End of copyfonts */

/*****************************************************************************/

void
copyinput(void)
{

/*
 *
 * Copies the input file to stdout. If fp_temp isn't NULL seek to the start and
 * add it to the output file - it's a partial (or complete) copy of stdin made
 * by download(). Then copy fp_in, but only seek to the start if it's not stdin.
 *
 */

    if ( fp_temp != NULL ) {
	fseek(fp_temp, 0L, 0);
	while ( fgets(buf, sizeof(buf), fp_temp) != NULL )
	    printf("%s", buf);
    }	/* End if */

    if ( fp_in != stdin )
	fseek(fp_in, 0L, 0);

    while ( fgets(buf, sizeof(buf), fp_in) != NULL )
	printf("%s", buf);

}   /* End of copyinput */

/*****************************************************************************/

int
lookup(font)

    char	*font;

{

    int		i;

/*
 *
 * Looks for *font in the map table. Return the map table index if found and
 * not yet downloaded - otherwise return next.
 *
 */

    for ( i = 0; i < next; i++ )
	if ( strcmp(font, map[i].font) == 0 ) {
	    if ( map[i].downloaded == TRUE )
		i = next;
	    break;
	}   /* End if */

    return(i);

}   /* End of lookup */

/*****************************************************************************/

Map *allocate(ptr, num)

    Map		*ptr;
    int		num;

{

/*
 *
 * Allocates space for num Map elements. Calls malloc() if ptr is NULL and
 * realloc() otherwise.
 *
 */

    if ( ptr == NULL )
	ptr = (Map *)malloc(num * sizeof(Map));
    else ptr = (Map *)realloc(ptr, num * sizeof(Map));

    if ( ptr == NULL )
	error(FATAL, "no map memory");

    return(ptr);

}   /* End of allocate */

/*****************************************************************************/
