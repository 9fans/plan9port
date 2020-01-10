/*
 *
 * Adobe's encryption/decryption algorithm for eexec and show. Runs in
 * eexec mode unless told otherwise. Use,
 *
 *		pscrypt file.cypher > file.clear
 *
 * to decrypt eexec input. Assumes file.cypher is hex with the key as the
 * first four bytes, and writes file.clear as binary (omitting the key).
 * Use
 *
 *		pscrypt -e12ab34ef file.clear >file.cypher
 *
 * to encrypt file.clear (for eexec) using 12ab34ef as the key. Input is
 * binary and output is hex. The key must be given as a hex number. Use
 * -sshow to encrypt or decrypt a CharString or Subr,
 *
 *		pscrypt -sshow file.cypher > file.clear
 *
 * Use -b or -x to read binary or hex input, and -B or -X to output binary
 * or hex.
 *
 */

#include <stdio.h>
#include <ctype.h>

#define ENCRYPT		0
#define DECRYPT		1

#define NOTSET		-1
#define BINARY		0
#define HEX		1
#define LINELENGTH	40

#define CHARSTRING	4330
#define EEXEC		55665
#define MAGIC1		52845
#define MAGIC2		22719

int	argc;
char	**argv;

int	mode = DECRYPT;
int	input = NOTSET;
int	output = NOTSET;
int	outoffset = NOTSET;
int	inoffset = NOTSET;

int	cryptkey = 0;			/* encryption key set with -e */
int	linelength = LINELENGTH;	/* only for hex output */
int	lastchar = 0;

unsigned long	seed = EEXEC;
unsigned long	key;

FILE	*fp_in;

/*****************************************************************************/

main(agc, agv)

    int		agc;
    char	*agv[];

{

/*
 *
 * Implementation of the encryption/decryption used by eexec and show.
 *
 */

    argc = agc;
    argv = agv;

    fp_in = stdin;

    options();
    initialize();
    arguments();

    exit(0);

}   /* End of main */

/*****************************************************************************/

options()

{

    int		ch;
    char	*names = "bde:l:os:xBSX";

    extern char	*optarg;
    extern int	optind;

/*
 *
 * Command line options.
 *
 */

    while ( (ch = getopt(argc, argv, names)) != EOF )
	switch ( ch ) {
	    case 'b':			/* binary input */
		    input = BINARY;
		    break;

	    case 'd':			/* decrypt */
		    mode = DECRYPT;
		    break;

	    case 'e':			/* encrypt */
		    mode = ENCRYPT;
		    if ( *optarg == '0' && *optarg == 'x' )
			optarg += 2;
		    sscanf(optarg, "%8x", &cryptkey);
		    break;

	    case 'l':			/* line length hex output */
		    linelength = atoi(optarg);
		    break;

	    case 'o':			/* output all bytes - debugging */
		    outoffset = 0;
		    break;

	    case 's':			/* seed */
		    if ( *optarg == 'e' )
			seed = EEXEC;
		    else if ( *optarg == 's' )
			seed = CHARSTRING;
		    else if ( *optarg == '0' && *(optarg+1) == 'x' )
			sscanf(optarg+2, "%x", &seed);
		    else if ( *optarg == '0' )
			sscanf(optarg, "%o", &seed);
		    else sscanf(optarg, "%d", &seed);
		    break;

	    case 'x':			/* hex input */
		    input = HEX;
		    break;

	    case 'B':			/* binary output */
		    output = BINARY;
		    break;

	    case 'X':			/* hex output */
		    output = HEX;
		    break;

	    case '?':			/* don't understand the option */
		    fprintf(stderr, "bad option -%c\n", ch);
		    exit(1);
		    break;

	    default:			/* don't know what to do for ch */
		    fprintf(stderr, "missing case for option -%c\n", ch);
		    exit(1);
		    break;
	}   /* End switch */

    argc -= optind;			/* get ready for non-option args */
    argv += optind;

}   /* End of options */

/*****************************************************************************/

initialize()

{

/*
 *
 * Initialization that has to be done after the options.
 *
 */

    key = seed;

    if ( mode == DECRYPT ) {
	input = (input == NOTSET) ? HEX : input;
	output = (output == NOTSET) ? BINARY : output;
	inoffset = (inoffset == NOTSET) ? 0 : inoffset;
	outoffset = (outoffset == NOTSET) ? -4 : outoffset;
    } else {
	input = (input == NOTSET) ? BINARY : input;
	output = (output == NOTSET) ? HEX : output;
	inoffset = (inoffset == NOTSET) ? 4 : inoffset;
	outoffset = (outoffset == NOTSET) ? 0 : outoffset;
    }	/* End else */

    if ( linelength <= 0 )
	linelength = LINELENGTH;

}   /* End of initialize */

/*****************************************************************************/

arguments()

{

/*
 *
 * Everything left is an input file. No arguments or '-' means stdin.
 *
 */

    if ( argc < 1 )
	crypt();
    else
	while ( argc > 0 ) {
	    if ( strcmp(*argv, "-") == 0 )
		fp_in = stdin;
	    else if ( (fp_in = fopen(*argv, "r")) == NULL ) {
		fprintf(stderr, "can't open %s\n", *argv);
		exit(1);
	    }	/* End if */
	    crypt();
	    if ( fp_in != stdin )
		fclose(fp_in);
	    argc--;
	    argv++;
	}   /* End while */

}   /* End of arguments */

/*****************************************************************************/

crypt()

{

    unsigned int	cypher;
    unsigned int	clear;

/*
 *
 * Runs the encryption/decryption algorithm.
 *
 */

    while ( lastchar != EOF ) {
	cypher = nextbyte();
	clear = ((key >> 8) ^ cypher) & 0xFF;
	key = (key + (mode == DECRYPT ? cypher : clear)) * MAGIC1 + MAGIC2;
	if ( ++outoffset > 0 && lastchar != EOF ) {
	    if ( output == HEX ) {
		printf("%.2X", clear);
		if ( linelength > 0 && (outoffset % linelength) == 0 )
		    putchar('\n');
	    } else putchar(clear);
	}   /* End if */
    }	/* End while */

}   /* End of crypt */

/*****************************************************************************/

nextbyte()

{

    int		val = EOF;

/*
 *
 * Returns the next byte. Uses cryptkey (i.e. what followed -e) while inoffset is
 * positive, otherwise reads (hex or binary) from fp_in.
 *
 */

    if ( inoffset-- > 0 )
	val = (cryptkey >> (inoffset*8)) & 0xFF;
    else if ( input == HEX ) {
	if ( (val = nexthexchar()) != EOF )
	    val = (val << 4) | nexthexchar();
    } else if ( input == BINARY )
	val = Getc(fp_in);

    return(val);

}   /* End of nextbyte */

/*****************************************************************************/

nexthexchar()

{

    int		ch;

/*
 *
 * Reads the next hex character.
 *
 */

    while ( (ch = Getc(fp_in)) != EOF && ! isxdigit(ch) ) ;

    if ( isdigit(ch) )
	ch -= '0';
    else if ( isupper(ch) )
	ch -= 'A' - 10;
    else if ( islower(ch) )
	ch -= 'a' - 10;

    return(ch);

}   /* End of nexthexchar */

/*****************************************************************************/

Getc(fp)

    FILE	*fp;

{

/*
 *
 * Reads the next byte from *fp, sets lastchar, and returns the character.
 *
 */

    return(lastchar = getc(fp));

}   /* End of Getc */

/*****************************************************************************/
