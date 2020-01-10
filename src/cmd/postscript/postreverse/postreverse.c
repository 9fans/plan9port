/*
 *
 * postreverse - reverse the page order in certain PostScript files.
 *
 * Page reversal relies on being able to locate sections of a document using file
 * structuring comments defined by Adobe (ie. the 1.0 and now 2.0 conventions) and
 * a few I've added. Among other things a minimally conforming document, according
 * to the 1.0 conventions,
 *
 *	1) Marks the end of the prologue with an %%EndProlog comment.
 *
 *	2) Starts each page with a %%Page: comment.
 *
 *	3) Marks the end of all the pages %%Trailer comment.
 *
 *	4) Obeys page independence (ie. pages can be arbitrarily rearranged).
 *
 * The most important change (at least for this program) that Adobe made in going
 * from the 1.0 to the 2.0 structuring conventions was in the prologue. They now
 * say the prologue should only define things, and the global initialization that
 * was in the prologue (1.0 conventions) should now come after the %%EndProlog
 * comment but before the first %%Page: comment and be bracketed by %%BeginSetup
 * and %%EndSetup comments. So a document that conforms to Adobe's 2.0 conventions,
 *
 *	1) Marks the end of the prologue (only definitions) with %%EndProlog.
 *
 *	2) Brackets global initialization with %%BeginSetup and %%EndSetup comments
 *	   which come after the prologue but before the first %Page: comment.
 *
 *	3) Starts each page with a %%Page: comment.
 *
 *	4) Marks the end of all the pages with a %%Trailer comment.
 *
 *	5) Obeys page independence.
 *
 * postreverse can handle documents that follow the 1.0 or 2.0 conventions, but has
 * also been extended slightly so it works properly with the translators (primarily
 * dpost) supplied with this package. The page independence requirement has been
 * relaxed some. In particular definitions exported to the global environment from
 * within a page should be bracketed by %%BeginGlobal and %%EndGlobal comments.
 * postreverse pulls them out of each page and inserts them in the setup section
 * of the document, immediately before it writes the %%EndProlog (for version 1.0)
 * or %%EndSetup (for version 2.0) comments.
 *
 * In addition postreverse accepts documents that choose to mark the end of each
 * page with a %%EndPage: comment, which from a translator's point of view is often
 * a more natural approach. Both page boundary comments (ie. Page: and %%EndPage:)
 * are also accepted, but be warned that everything between consecutive %%EndPage:
 * and %%Page: comments will be ignored.
 *
 * So a document that will reverse properly with postreverse,
 *
 *	1) Marks the end of the prologue with %%EndProlog.
 *
 *	2) May have a %%BeginSetup/%%EndSetup comment pair before the first %%Page:
 *	   comment that brackets any global initialization.
 *
 *	3) Marks the start of each page with a %%Page: comment, or the end of each
 *	   page with a %%EndPage: comment. Both page boundary comments are allowed.
 *
 *	4) Marks the end of all the pages with a %%Trailer comment.
 *
 *	5) Obeys page independence or violates it to a rather limited extent and
 *	   marks the violations with %%BeginGlobal and %%EndGlobal comments.
 *
 * If no file arguments are given postreverse copies stdin to a temporary file and
 * then processes that file. That means the input is read three times (rather than
 * two) whenever we handle stdin. That's expensive, and shouldn't be too difficult
 * to fix, but I haven't gotten around to it yet.
 *
 */

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>

#include "comments.h"			/* PostScript file structuring comments */
#include "gen.h"			/* general purpose definitions */
#include "path.h"			/* for temporary directory */
#include "ext.h"			/* external variable declarations */
#include "postreverse.h"		/* a few special definitions */

int	page = 1;			/* current page number */
int	forms = 1;			/* forms per page in the input file */

char	*temp_dir = TEMPDIR;		/* temp directory for copying stdin */

Pages	pages[1000];			/* byte offsets for all pages */
int	next_page = 0;			/* next page goes here */
long	start;				/* starting offset for next page */
long	endoff = -1;			/* offset where TRAILER was found */
int	noreverse = FALSE;		/* don't reverse pages if TRUE */
char	*endprolog = ENDPROLOG;		/* occasionally changed to ENDSETUP */

double	version = 3.3;			/* of the input file */
int	ignoreversion = FALSE;		/* ignore possible forms.ps problems */

char	buf[2048];			/* line buffer for input file */

FILE	*fp_in;				/* stuff is read from this file */
FILE	*fp_out;			/* and written here */

/*****************************************************************************/

main(agc, agv)

    int		agc;
    char	*agv[];

{

/*
 *
 * A simple program that reverses the pages in specially formatted PostScript
 * files. Will work with all the translators in this package, and should handle
 * any document that conforms to Adobe's version 1.0 or 2.0 file structuring
 * conventions. Only one input file is allowed, and it can either be a named (on
 * the command line) file or stdin.
 *
 */

    argc = agc;				/* other routines may want them */
    argv = agv;

    prog_name = argv[0];		/* just for error messages */

    fp_in = stdin;
    fp_out = stdout;

    init_signals();			/* sets up interrupt handling */
    options();				/* first get command line options */
    arguments();			/* then process non-option arguments */
    done();				/* and clean things up */

    exit(x_stat);			/* not much could be wrong */

}   /* End of main */

/*****************************************************************************/

init_signals()

{

/*
 *
 * Makes sure we handle interrupts properly.
 *
 */

    if ( signal(SIGINT, interrupt) == SIG_IGN )  {
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

options()

{

    int		ch;			/* return value from getopt() */
    char	*optnames = "n:o:rvT:DI";

    extern char	*optarg;		/* used by getopt() */
    extern int	optind;

/*
 *
 * Reads and processes the command line options. The -r option (ie. the one that
 * turns page reversal off) is really only useful if you want to take dpost output
 * and produce a page independent output file. In that case global definitions
 * made within pages and bracketed by %%BeginGlobal/%%EndGlobal comments will be
 * moved into the prologue or setup section of the document.
 *
 */

    while ( (ch = getopt(argc, argv, optnames)) != EOF )  {
	switch ( ch )  {
	    case 'n':			/* forms per page */
		    if ( (forms = atoi(optarg)) <= 0 )
			error(FATAL, "illegal forms request %s", optarg);
		    break;

	    case 'o':			/* output page list */
		    out_list(optarg);
		    break;

	    case 'r':			/* don't reverse the pages */
		    noreverse = TRUE;
		    break;

	    case 'v':			/* ignore possible forms.ps problems */
		    ignoreversion = TRUE;
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

arguments()

{

    char	*name;			/* name of the input file */

/*
 *
 * postreverse only handles one input file at a time, so if there's more than one
 * argument left when we get here we'll quit. If none remain we copy stdin to a
 * temporary file and process that file.
 *
 */

    if ( argc > 1 )			/* can't handle more than one file */
	error(FATAL, "too many arguments");

    if ( argc == 0 )			/* copy stdin to a temporary file */
	name = copystdin();
    else name = *argv;

    if ( (fp_in = fopen(name, "r")) == NULL )
	error(FATAL, "can't open %s", name);

    reverse();

}   /* End of arguments */

/*****************************************************************************/

done()

{

/*
 *
 * Cleans things up after we've finished reversing the pages in the input file.
 * All that's really left to do is remove the temp file, provided we used one.
 *
 */

    if ( temp_file != NULL )
	unlink(temp_file);

}   /* End of done */

/*****************************************************************************/

char *copystdin()

{

    int		fd_out;			/* for the temporary file */
    int		fd_in;			/* for stdin */
    int		count;			/* number of bytes put in buf[] */

/*
 *
 * Copies stdin to a temporary file and returns the pathname of that file to the
 * caller. It's an expensive way of doing things, because it means we end up
 * reading the input file three times - rather than just twice. Could probably be
 * fixed by creating the temporary file on the fly as we read the file the first
 * time.
 *
 */

    if ( (temp_file = tempnam(temp_dir, "post")) == NULL )
	error(FATAL, "can't generate temp file name");

    if ( (fd_out = creat(temp_file, 0660)) == -1 )
	error(FATAL, "can't open %s", temp_file);

    fd_in = fileno(stdin);

    while ( (count = read(fd_in, buf, sizeof(buf))) > 0 )
	if ( write(fd_out, buf, count) != count )
	    error(FATAL, "error writing to %s", temp_file);

    close(fd_out);

    return(temp_file);

}   /* End of copystdin */

/*****************************************************************************/

reverse()

{

/*
 *
 * Begins by looking for the ENDPROLOG comment in the input file. Everything up to
 * that comment is copied to the output file. If the comment isn't found the entire
 * input file is copied and moreprolog() returns FALSE. Otherwise readpages() reads
 * the rest of the input file and remembers (in pages[]) where each page starts and
 * ends. In addition everything bracketed by %%BeginGlobal and %%EndGlobal comments
 * is immediately added to the new prologue (or setup section) and ends up being
 * removed from the individual pages. When readpages() finds the TRAILER comment
 * or gets to the end of the input file we go back to the pages[] array and use
 * the saved offsets to write the pages out in reverse order. Finally everything
 * from the TRAILER comment to the end of the input file is copied to the output
 * file.
 *
 */

    if ( moreprolog(ENDPROLOG) == TRUE )  {
	readpages();
	writepages();
	trailer();
    }	/* End if */

}   /* End of reverse */

/*****************************************************************************/

moreprolog(str)

    char	*str;			/* copy everything up to this string */

{

    int		len;			/* length of FORMSPERPAGE string */
    int		vlen;			/* length of VERSION string */

/*
 *
 * Looks for string *str at the start of a line and copies everything up to that
 * string to the output file. If *str isn't found the entire input file will end
 * up being copied to the output file and FALSE will be returned to the caller.
 * The first call (made from reverse()) looks for ENDPROLOG. Any other call comes
 * from readpages() and will be looking for the ENDSETUP comment.
 *
 */

    len = strlen(FORMSPERPAGE);
    vlen = strlen(VERSION);

    while ( fgets(buf, sizeof(buf), fp_in) != NULL )  {
	if ( strcmp(buf, str) == 0 )
	    return(TRUE);
	else if ( strncmp(buf, FORMSPERPAGE, len) == 0 )
	    forms = atoi(&buf[len+1]);
	else if ( strncmp(buf, VERSION, vlen) == 0 )
	    version = atof(&buf[vlen+1]);
	fprintf(fp_out, "%s", buf);
    }	/* End while */

    return(FALSE);

}   /* End of moreprolog */

/*****************************************************************************/

readpages()

{

    int		endpagelen;		/* length of ENDPAGE */
    int		pagelen;		/* and PAGE strings */
    int		sawendpage = TRUE;	/* ENDPAGE equivalent marked last page */
    int		gotpage = FALSE;	/* TRUE disables BEGINSETUP stuff */

/*
 *
 * Records starting and ending positions of the requested pages (usually all of
 * them), puts global definitions in the prologue, and remembers where the TRAILER
 * was found.
 *
 * Page boundaries are marked by the strings PAGE, ENDPAGE, or perhaps both.
 * Application programs will normally find one or the other more convenient, so
 * in most cases only one kind of page delimiter will be found in a particular
 * document.
 *
 */

    pages[0].start = ftell(fp_in);	/* first page starts after ENDPROLOG */
    endprolog = ENDPROLOG;

    endpagelen = strlen(ENDPAGE);
    pagelen = strlen(PAGE);

    while ( fgets(buf, sizeof(buf), fp_in) != NULL )
	if ( buf[0] != '%' )
	    continue;
	else if ( strncmp(buf, ENDPAGE, endpagelen) == 0 )  {
	    if ( in_olist(page++) == ON )  {
		pages[next_page].empty = FALSE;
		pages[next_page++].stop = ftell(fp_in);
	    }	/* End if */
	    pages[next_page].start = ftell(fp_in);
	    sawendpage = TRUE;
	    gotpage = TRUE;
	} else if ( strncmp(buf, PAGE, pagelen) == 0 )  {
	    if ( sawendpage == FALSE && in_olist(page++) == ON )  {
		pages[next_page].empty = FALSE;
		pages[next_page++].stop = ftell(fp_in) - strlen(buf);
	    }	/* End if */
	    pages[next_page].start = ftell(fp_in) - strlen(buf);
	    sawendpage = FALSE;
	    gotpage = TRUE;
	} else if ( gotpage == FALSE && strcmp(buf, BEGINSETUP) == 0 )  {
	    fprintf(fp_out, "%s", endprolog);
	    fprintf(fp_out, "%s", BEGINSETUP);
	    moreprolog(ENDSETUP);
	    endprolog = ENDSETUP;
	} else if ( strcmp(buf, BEGINGLOBAL) == 0 )  {
	    moreprolog(ENDGLOBAL);
	} else if ( strcmp(buf, TRAILER) == 0 )  {
	    if ( sawendpage == FALSE )
		pages[next_page++].stop = ftell(fp_in) - strlen(buf);
	    endoff = ftell(fp_in);
	    break;
	}   /* End if */

}   /* End of readpages */

/*****************************************************************************/

writepages()

{

    int		i, j, k;		/* loop indices */

/*
 *
 * Goes through the pages[] array, usually from the bottom up, and writes out all
 * the pages. Documents that print more than one form per page cause things to get
 * a little more complicated. Each physical page has to have its subpages printed
 * in the correct order, and we have to build a few dummy subpages for the last
 * (and now first) sheet of paper, otherwise things will only occasionally work.
 *
 */

    fprintf(fp_out, "%s", endprolog);

    if ( noreverse == FALSE )		/* fill out the first page */
	for ( i = (forms - next_page % forms) % forms; i > 0; i--, next_page++ )
	    pages[next_page].empty = TRUE;
    else forms = next_page;		/* turns reversal off in next loop */

    for ( i = next_page - forms; i >= 0; i -= forms )
	for ( j = i, k = 0; k < forms; j++, k++ )
	    if ( pages[j].empty == TRUE ) {
		if ( ignoreversion == TRUE || version > 3.1 ) {
		    fprintf(fp_out, "%s 0 0\n", PAGE);
		    fprintf(fp_out, "/saveobj save def\n");
		    fprintf(fp_out, "showpage\n");
		    fprintf(fp_out, "saveobj restore\n");
		    fprintf(fp_out, "%s 0 0\n", ENDPAGE);
		} else {
		    fprintf(fp_out, "%s 0 0\n", PAGE);
		    fprintf(fp_out, "save showpage restore\n");
		    fprintf(fp_out, "%s 0 0\n", ENDPAGE);
		}   /* End else */
	    } else copypage(pages[j].start, pages[j].stop);

}   /* End of writepages */

/*****************************************************************************/

copypage(start, stop)

    long	start;			/* starting from this offset */
    long	stop;			/* and ending here */

{

/*
 *
 * Copies the page beginning at offset start and ending at stop to the output
 * file. Global definitions are skipped since they've already been added to the
 * prologue.
 *
 */

    fseek(fp_in, start, 0);

    while ( ftell(fp_in) < stop && fgets(buf, sizeof(buf), fp_in) != NULL )
	if ( buf[0] == '%' && strcmp(buf, BEGINGLOBAL) == 0 )
	    while ( fgets(buf, sizeof(buf), fp_in) != NULL && strcmp(buf, ENDGLOBAL) != 0 ) ;
	else fprintf(fp_out, "%s", buf);

}   /* End of copypage */

/*****************************************************************************/

trailer()

{

/*
 *
 * Makes sure everything from the TRAILER string to EOF is copied to the output
 * file.
 *
 */

    if ( endoff > 0 )  {
	fprintf(fp_out, "%s", TRAILER);
	fseek(fp_in, endoff, 0);
	while ( fgets(buf, sizeof(buf), fp_in) != NULL )
	    fprintf(fp_out, "%s", buf);
    }	/* End if */

}   /* End of trailer */

/*****************************************************************************/
