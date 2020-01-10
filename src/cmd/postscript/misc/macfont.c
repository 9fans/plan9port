/*
 *
 * Program that converts Macintosh font files to a format that works on Unix
 * systems. Essentially all the information needed came from the Adobe paper
 * "Supporting Downloadable PostScript Fonts". To use the program type,
 *
 *	macfont font.mac >font.unix
 *
 * where font.mac is the font file, exactly as it came over from a Macintosh,
 * and font.unix is equivalent host resident font file usable on Unix systems.
 *
 */

#include <stdio.h>
#include <signal.h>

#define OFF		0
#define ON		1

#define NON_FATAL	0
#define FATAL		1

#define FALSE		0
#define TRUE		1

char	**argv;
int	argc;

char	*prog_name;

int	x_stat;
int	debug = OFF;
int	ignore = OFF;

FILE	*fp_in;
FILE	*fp_out;

/*****************************************************************************/

main(agc, agv)

    int		agc;
    char	*agv[];

{

/*
 *
 * Macintosh to Unix font converter.
 *
 */

    argc = agc;
    argv = agv;
    prog_name = argv[0];

    fp_in = stdin;
    fp_out = stdout;

    options();
    arguments();
    exit(x_stat);

}   /* End of main */

/*****************************************************************************/

options()

{

    int		ch;
    char	*names = "DI";

    extern char	*optarg;
    extern int	optind;

/*
 *
 * Command line options.
 *
 */

    while ( (ch = getopt(argc, argv, names)) != EOF ) {
	switch ( ch ) {
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

    argc -= optind;
    argv += optind;

}   /* End of options */

/*****************************************************************************/

arguments()

{


/*
 *
 * Everything else is an input file. No arguments or '-' means stdin.
 *
 */

    if ( argc < 1 )
	conv();
    else
	while ( argc > 0 ) {
	    if ( strcmp(*argv, "-") == 0 )
		fp_in = stdin;
	    else if ( (fp_in = fopen(*argv, "r")) == NULL )
		error(FATAL, "can't open %s", *argv);
	    conv();
	    if ( fp_in != stdin )
		fclose(fp_in);
	    argc--;
	    argv++;
	}   /* End while */

}   /* End of arguments */

/*****************************************************************************/

conv()

{

    int		blocksize;
    int		blocktype;

/*
 *
 * The first four bytes (in a block) are the block size, the fifth is the block
 * type, and the sixth always appears to be NULL. Type 0 blocks are comments and
 * are always skipped. Type 1 blocks are ASCII text, type 2 is binary data that
 * should be converted to hex, while type 5 blocks represent the end of the font
 * file. Commment block lengths appear to be from the first byte, while other
 * lengths seem to be measured from block type byte (ie. the fifth byte). Type
 * four blocks aren't used, while type 3 blocks mean an end of file indication
 * should be sent to the printer. Haven't done anything with type 3 blocks.
 *
 */

    while ( 1 ) {
	blocksize = getint(fp_in);
	blocktype = getc(fp_in);
	getc(fp_in);
	if ( debug == ON )
	    fprintf(stderr, "blocktype = %d, blocksize = %d\n", blocktype, blocksize);
	switch ( blocktype ) {
	    case 0:			/* comment - skip blockcount bytes */
		fseek(fp_in, (long) blocksize - 6, 1);
		break;

	    case 1:
		asciitext(blocksize - 2);
		break;

	    case 2:
		hexdata(blocksize - 2);
		break;

	    case 3:
	    case 4:
		error(FATAL, "resource type %d not implemented", blocktype);
		break;

	    case 5:
		return;

	    default:
		error(FATAL, "unknown resource type %d", blocktype);
	}   /* End switch */
    }	/* End while */

}   /* End of conv */

/*****************************************************************************/

asciitext(count)

    int		count;			/* bytes left in the block */

{

    int		ch;
    int		i = 0;

/*
 *
 * Handles type 1 (ie. ASCII text) blocks. Changing carriage returns to newlines
 * is all I've done.
 *
 */

    for ( i = 0; i < count; i++ ) {
	if ( (ch = getc(fp_in)) == '\r' )
	    ch = '\n';
	putc(ch, fp_out);
    }	/* End for */

}   /* End of asciitext */

/*****************************************************************************/

hexdata(count)

    int		count;			/* bytes left in the block */

{

    int		i;
    int		n;

/*
 *
 * Reads the next count bytes and converts each byte to hex. Also starts a new
 * line every 80 hex characters.
 *
 */

    for ( i = 0, n = 0; i < count; i++ ) {
	fprintf(fp_out, "%.2X", getc(fp_in));
	if ( (++n % 40) == 0 )
	    putc('\n', fp_out);
    }	/* End for */

}   /* End of hexdata */

/*****************************************************************************/

getint()

{

    int		val;
    int		i;

/*
 *
 * Reads the next four bytes into an integer and returns the value to the caller.
 * First two bytes are probably always 0.
 *
 */

    for ( i = 0, val = (getc(fp_in) & 0377); i < 3; i++ )
	val = (val << 8) | (getc(fp_in) & 0377);

    return(val);

}   /* End of getint */

/*****************************************************************************/

error(kind, mesg, a1, a2, a3)


    int		kind;
    char	*mesg;
    unsigned	a1, a2, a3;

{

/*
 *
 * Print *mesg then quit if kind is FATAL.
 *
 */

    if ( mesg != NULL && *mesg != '\0' ) {
	fprintf(stderr, "%s: ", prog_name);
	fprintf(stderr, mesg, a1, a2, a3);
	putc('\n', stderr);
    }	/* End if */

    if ( kind == FATAL && ignore == OFF )
	exit(x_stat | 01);

}   /* End of error */

/*****************************************************************************/
