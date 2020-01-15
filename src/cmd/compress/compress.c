/*
 * compress - File compression ala IEEE Computer, June 1984.
 *
 * Algorithm from "A Technique for High Performance Data Compression",
 * Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 * Usage: compress [-dfvc] [-b bits] [file ...]
 * Inputs:
 *	-b:	limit the max number of bits/code.
 *	-c:	write output on stdout, don't remove original.
 *	-d:	decompress instead.
 *	-f:	Forces output file to be generated, even if one already
 *		exists, and even if no space is saved by compressing.
 *		If -f is not used, the user will be prompted if stdin is
 *		a tty, otherwise, the output file will not be overwritten.
 *	-v:	Write compression statistics
 *
 * 	file ...: Files to be compressed.  If none specified, stdin is used.
 * Outputs:
 *	file.Z:	Compressed form of file with same mode, owner, and utimes
 * 		or stdout (if stdin used as input)
 *
 * Assumptions:
 *	When filenames are given, replaces with the compressed version
 *	(.Z suffix) only if the file decreases in size.
 * Algorithm:
 * 	Modified Lempel-Ziv method (LZW).  Basically finds common
 * substrings and replaces them with a variable size code.  This is
 * deterministic, and can be done on the fly.  Thus, the decompression
 * procedure needs no input table, but tracks the way the table was built.

 * Authors:	Spencer W. Thomas	(decvax!harpo!utah-cs!utah-gr!thomas)
 *		Jim McKie		(decvax!mcvax!jim)
 *		Steve Davies		(decvax!vax135!petsd!peora!srd)
 *		Ken Turkowski		(decvax!decwrl!turtlevax!ken)
 *		James A. Woods		(decvax!ihnp4!ames!jaw)
 *		Joe Orost		(decvax!vax135!petsd!joe)
 */

#ifndef USED
#  define USED(x) if(x);else
#endif

#define _PLAN9_SOURCE
#define _BSD_EXTENSION
#define _POSIX_SOURCE

#include <u.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>

#define	min(a,b)	((a>b) ? b : a)

#define BITS	16
#define HSIZE	69001		/* 95% occupancy */

/*
 * a code_int must be able to hold 2**BITS values of type int, and also -1
 */
typedef long	code_int;
typedef long	count_int;

static char rcs_ident[] = "$Header: compress.c,v 4.0 85/07/30 12:50:00 joe Release $";

uchar magic_header[] = { 0x1F, 0x9D };	/* 1F 9D */

/* Defines for third byte of header */
#define BIT_MASK	0x1f
#define BLOCK_MASK	0x80
/* Masks 0x40 and 0x20 are free.  I think 0x20 should mean that there is
	a fourth header byte (for expansion).
*/
#define INIT_BITS 9			/* initial number of bits/code */

#define ARGVAL() (*++(*argv) || (--argc && *++argv))

int n_bits;				/* number of bits/code */
int maxbits = BITS;			/* user settable max # bits/code */
code_int maxcode;			/* maximum code, given n_bits */
code_int maxmaxcode = 1 << BITS;	/* should NEVER generate this code */

#define MAXCODE(n_bits)	((1 << (n_bits)) - 1)

count_int htab[HSIZE];
ushort codetab[HSIZE];

#define htabof(i)	htab[i]
#define codetabof(i)	codetab[i]

code_int hsize = HSIZE;			/* for dynamic table sizing */
count_int fsize;

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**BITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define tab_prefixof(i)	codetabof(i)
#define tab_suffixof(i)	((uchar *)(htab))[i]
#define de_stack		((uchar *)&tab_suffixof(1<<BITS))

code_int free_ent = 0;			/* first unused entry */
int exit_stat = 0;

void	cl_block(void);
void	cl_hash(count_int);
void	compress(void);
void	copystat(char *, char *);
void	decompress(void);
int	foreground(void);
code_int getcode(void);
void	onintr(int);
void	oops(int);
void	output(code_int);
void	prratio(FILE *, long, long);
void	version(void);
void	writeerr(void);

void
Usage(void)
{
#ifdef DEBUG
	fprintf(stderr,"Usage: compress [-cdfDV] [-b maxbits] [file ...]\n");
#else
	fprintf(stderr,"Usage: compress [-cdfvV] [-b maxbits] [file ...]\n");
#endif /* DEBUG */
}

int debug = 0;
int nomagic = 0;	/* Use a 3-byte magic number header, unless old file */
int zcat_flg = 0;	/* Write output on stdout, suppress messages */
int quiet = 1;		/* don't tell me about compression */

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
int block_compress = BLOCK_MASK;
int clear_flg = 0;
long ratio = 0;
#define CHECK_GAP 10000	/* ratio check interval */
count_int checkpoint = CHECK_GAP;
/*
 * the next two codes should not be changed lightly, as they must not
 * lie within the contiguous general code space.
 */
#define FIRST	257	/* first free entry */
#define	CLEAR	256	/* table clear output code */

int force = 0;
char ofname [100];
#ifdef DEBUG
int verbose = 0;
#endif /* DEBUG */
void (*bgnd_flag)(int);

int do_decomp = 0;

int
main(argc, argv)
int argc;
char **argv;
{
	int overwrite = 0;	/* Do not overwrite unless given -f flag */
	char tempname[512];
	char **filelist, **fileptr;
	char *cp;
	struct stat statbuf;

	if ( (bgnd_flag = signal ( SIGINT, SIG_IGN )) != SIG_IGN ) {
		signal(SIGINT, onintr);
		signal(SIGSEGV, oops);
	}

	filelist = fileptr = (char **)(malloc(argc * sizeof(*argv)));
	*filelist = NULL;

	if((cp = strrchr(argv[0], '/')) != 0)
		cp++;
	else
		cp = argv[0];
	if(strcmp(cp, "uncompress") == 0)
		do_decomp = 1;
	else if(strcmp(cp, "zcat") == 0) {
		do_decomp = 1;
		zcat_flg = 1;
	}

	/*
	 * Argument Processing
	 * All flags are optional.
	 * -C	generate output compatible with compress 2.0.
	 * -D	debug
	 * -V	print Version; debug verbose
	 * -b maxbits	maxbits.  If -b is specified, then maxbits MUST be
	 *		given also.
	 * -c	cat all output to stdout
	 * -d	do_decomp
	 * -f	force overwrite of output file
	 * -n	no header: useful to uncompress old files
	 * -v	unquiet
	 * if a string is left, must be an input filename.
	 */
	for (argc--, argv++; argc > 0; argc--, argv++) {
	if (**argv == '-') {	/* A flag argument */
		while (*++(*argv)) {	/* Process all flags in this arg */
		switch (**argv) {
		case 'C':
			block_compress = 0;
			break;
#ifdef DEBUG
		case 'D':
			debug = 1;
			break;
		case 'V':
			verbose = 1;
			version();
			break;
#else
		case 'V':
			version();
			break;
#endif
		case 'b':
			if (!ARGVAL()) {
				fprintf(stderr, "Missing maxbits\n");
				Usage();
				exit(1);
			}
			maxbits = atoi(*argv);
			goto nextarg;
		case 'c':
			zcat_flg = 1;
			break;
		case 'd':
			do_decomp = 1;
			break;
		case 'f':
		case 'F':
			overwrite = 1;
			force = 1;
			break;
		case 'n':
			nomagic = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'v':
			quiet = 0;
			break;
		default:
			fprintf(stderr, "Unknown flag: '%c'; ", **argv);
			Usage();
			exit(1);
		}
		}
	} else {			/* Input file name */
		*fileptr++ = *argv;	/* Build input file list */
		*fileptr = NULL;
		/* process nextarg; */
	}
nextarg:
		continue;
	}

	if(maxbits < INIT_BITS) maxbits = INIT_BITS;
	if (maxbits > BITS) maxbits = BITS;
	maxmaxcode = 1 << maxbits;

	if (*filelist != NULL) {
	for (fileptr = filelist; *fileptr; fileptr++) {
		exit_stat = 0;
		if (do_decomp != 0) {			/* DECOMPRESSION */
		/* Check for .Z suffix */
		if (strcmp(*fileptr + strlen(*fileptr) - 2, ".Z") != 0) {
			/* No .Z: tack one on */
			strcpy(tempname, *fileptr);
			strcat(tempname, ".Z");
			*fileptr = tempname;
		}
		/* Open input file */
		if ((freopen(*fileptr, "r", stdin)) == NULL) {
			perror(*fileptr);
			continue;
		}
		/* Check the magic number */
		if (nomagic == 0) {
			if ((getchar() != (magic_header[0] & 0xFF))
			|| (getchar() != (magic_header[1] & 0xFF))) {
				fprintf(stderr, "%s: not in compressed format\n",
					*fileptr);
				continue;
			}
			maxbits = getchar();	/* set -b from file */
			block_compress = maxbits & BLOCK_MASK;
			maxbits &= BIT_MASK;
			maxmaxcode = 1 << maxbits;
			if(maxbits > BITS) {
				fprintf(stderr,
		"%s: compressed with %d bits, can only handle %d bits\n",
					*fileptr, maxbits, BITS);
				continue;
			}
		}
		/* Generate output filename */
		strcpy(ofname, *fileptr);
		ofname[strlen(*fileptr) - 2] = '\0';  /* Strip off .Z */
		} else {				/* COMPRESSION */
		if (strcmp(*fileptr + strlen(*fileptr) - 2, ".Z") == 0) {
			fprintf(stderr,
				"%s: already has .Z suffix -- no change\n",
				*fileptr);
			continue;
		}
		/* Open input file */
		if ((freopen(*fileptr, "r", stdin)) == NULL) {
			perror(*fileptr);
			continue;
		}
		(void) stat(*fileptr, &statbuf);
		fsize = (long) statbuf.st_size;
		/*
		 * tune hash table size for small files -- ad hoc,
		 * but the sizes match earlier #defines, which
		 * serve as upper bounds on the number of output codes.
		 */
		hsize = HSIZE;
		if (fsize < (1 << 12))
			hsize = min(5003, HSIZE);
		else if (fsize < (1 << 13))
			hsize = min(9001, HSIZE);
		else if (fsize < (1 << 14))
			hsize = min (18013, HSIZE);
		else if (fsize < (1 << 15))
			hsize = min (35023, HSIZE);
		else if (fsize < 47000)
			hsize = min (50021, HSIZE);

		/* Generate output filename */
		strcpy(ofname, *fileptr);
#ifndef BSD4_2
		if ((cp=strrchr(ofname,'/')) != NULL)
			cp++;
		else
			cp = ofname;
		/*
		 *** changed 12 to 25;  should be NAMELEN-3, but I don't want
		 * to fight the headers.	ehg 5 Nov 92 **
		 */
		if (strlen(cp) > 25) {
			fprintf(stderr, "%s: filename too long to tack on .Z\n",
				cp);
			continue;
		}
#endif
		strcat(ofname, ".Z");
		}
		/* Check for overwrite of existing file */
		if (overwrite == 0 && zcat_flg == 0 &&
		   stat(ofname, &statbuf) == 0) {
			char response[2];

			response[0] = 'n';
			fprintf(stderr, "%s already exists;", ofname);
			if (foreground()) {
				fprintf(stderr,
				    " do you wish to overwrite %s (y or n)? ",
					ofname);
				fflush(stderr);
				(void) read(2, response, 2);
				while (response[1] != '\n')
					if (read(2, response+1, 1) < 0) {
						/* Ack! */
						perror("stderr");
						break;
					}
			}
			if (response[0] != 'y') {
				fprintf(stderr, "\tnot overwritten\n");
				continue;
			}
		}
		if(zcat_flg == 0) {		/* Open output file */
			if (freopen(ofname, "w", stdout) == NULL) {
				perror(ofname);
				continue;
			}
			if(!quiet)
				fprintf(stderr, "%s: ", *fileptr);
		}

		/* Actually do the compression/decompression */
		if (do_decomp == 0)
			compress();
#ifndef DEBUG
		else
			decompress();
#else
		else if (debug == 0)
			decompress();
		else
			printcodes();
		if (verbose)
			dump_tab();
#endif						/* DEBUG */
		if(zcat_flg == 0) {
			copystat(*fileptr, ofname);	/* Copy stats */
			if (exit_stat == 1 || !quiet)
				putc('\n', stderr);
		}
	}
	} else {		/* Standard input */
	if (do_decomp == 0) {
		compress();
#ifdef DEBUG
		if(verbose)
			dump_tab();
#endif
		if(!quiet)
			putc('\n', stderr);
	} else {
		/* Check the magic number */
		if (nomagic == 0) {
		if ((getchar()!=(magic_header[0] & 0xFF))
		 || (getchar()!=(magic_header[1] & 0xFF))) {
			fprintf(stderr, "stdin: not in compressed format\n");
			exit(1);
		}
		maxbits = getchar();	/* set -b from file */
		block_compress = maxbits & BLOCK_MASK;
		maxbits &= BIT_MASK;
		maxmaxcode = 1 << maxbits;
		fsize = 100000;		/* assume stdin large for USERMEM */
		if(maxbits > BITS) {
			fprintf(stderr,
			"stdin: compressed with %d bits, can only handle %d bits\n",
			maxbits, BITS);
			exit(1);
		}
		}
#ifndef DEBUG
		decompress();
#else
		if (debug == 0)
			decompress();
		else
			printcodes();
		if (verbose)
			dump_tab();
#endif						/* DEBUG */
	}
	}
	exit(exit_stat);
	return 0;
}

static int offset;
long in_count = 1;		/* length of input */
long bytes_out;			/* length of compressed output */
long out_count = 0;		/* # of codes output (for debugging) */

/*
 * compress stdin to stdout
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */
void
compress(void)
{
	code_int ent, hsize_reg;
	code_int i;
	int c, disp, hshift;
	long fcode;

	if (nomagic == 0) {
		putchar(magic_header[0]);
		putchar(magic_header[1]);
		putchar((char)(maxbits | block_compress));
		if(ferror(stdout))
			writeerr();
	}
	offset = 0;
	bytes_out = 3;		/* includes 3-byte header mojo */
	out_count = 0;
	clear_flg = 0;
	ratio = 0;
	in_count = 1;
	checkpoint = CHECK_GAP;
	maxcode = MAXCODE(n_bits = INIT_BITS);
	free_ent = (block_compress? FIRST: 256);

	ent = getchar ();

	hshift = 0;
	for (fcode = (long)hsize;  fcode < 65536L; fcode *= 2)
		hshift++;
	hshift = 8 - hshift;		/* set hash code range bound */

	hsize_reg = hsize;
	cl_hash( (count_int) hsize_reg);		/* clear hash table */

	while ((c = getchar()) != EOF) {
		in_count++;
		fcode = (long) (((long) c << maxbits) + ent);
	 	i = ((c << hshift) ^ ent);	/* xor hashing */

		if (htabof (i) == fcode) {
			ent = codetabof(i);
			continue;
		} else if ((long)htabof(i) < 0 )	/* empty slot */
			goto nomatch;
	 	disp = hsize_reg - i;	/* secondary hash (after G. Knott) */
		if (i == 0)
			disp = 1;
probe:
		if ((i -= disp) < 0)
			i += hsize_reg;

		if (htabof (i) == fcode) {
			ent = codetabof(i);
			continue;
		}
		if ((long)htabof(i) > 0)
			goto probe;
nomatch:
		output((code_int)ent);
		out_count++;
	 	ent = c;
		if (free_ent < maxmaxcode) {
	 		codetabof(i) = free_ent++;	/* code -> hashtable */
			htabof(i) = fcode;
		} else if ((count_int)in_count >= checkpoint && block_compress)
			cl_block ();
	}
	/*
	* Put out the final code.
	*/
	output( (code_int)ent );
	out_count++;
	output( (code_int)-1 );

	/*
	* Print out stats on stderr
	*/
	if(zcat_flg == 0 && !quiet) {
#ifdef DEBUG
	fprintf( stderr,
		"%ld chars in, %ld codes (%ld bytes) out, compression factor: ",
		in_count, out_count, bytes_out );
	prratio( stderr, in_count, bytes_out );
	fprintf( stderr, "\n");
	fprintf( stderr, "\tCompression as in compact: " );
	prratio( stderr, in_count-bytes_out, in_count );
	fprintf( stderr, "\n");
	fprintf( stderr, "\tLargest code (of last block) was %d (%d bits)\n",
		free_ent - 1, n_bits );
#else /* !DEBUG */
	fprintf( stderr, "Compression: " );
	prratio( stderr, in_count-bytes_out, in_count );
#endif /* DEBUG */
	}
	if(bytes_out > in_count)	/* exit(2) if no savings */
		exit_stat = 2;
}

/*
 * TAG( output )
 *
 * Output the given code.
 * Inputs:
 * 	code:	A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *		that n_bits =< (long)wordsize - 1.
 * Outputs:
 * 	Outputs code to the file.
 * Assumptions:
 *	Chars are 8 bits long.
 * Algorithm:
 * 	Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  When the buffer fills up empty it and start over.
 */

static char buf[BITS];

uchar lmask[9] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00};
uchar rmask[9] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

void
output( code )
code_int  code;
{
#ifdef DEBUG
	static int col = 0;
#endif
	int r_off = offset, bits= n_bits;
	char *bp = buf;

#ifdef DEBUG
	if (verbose)
		fprintf(stderr, "%5d%c", code,
			(col+=6) >= 74? (col = 0, '\n'): ' ');
#endif
	if (code >= 0) {
		/*
		 * byte/bit numbering on the VAX is simulated by the
		 * following code
		 */
		/*
		 * Get to the first byte.
		 */
		bp += (r_off >> 3);
		r_off &= 7;
		/*
		 * Since code is always >= 8 bits, only need to mask the first
		 * hunk on the left.
		 */
		*bp = (*bp & rmask[r_off]) | (code << r_off) & lmask[r_off];
		bp++;
		bits -=  8 - r_off;
		code >>= 8 - r_off;
		/* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
		if ( bits >= 8 ) {
			*bp++ = code;
			code >>= 8;
			bits -= 8;
		}
		/* Last bits. */
		if(bits)
			*bp = code;

		offset += n_bits;
		if ( offset == (n_bits << 3) ) {
			bp = buf;
			bits = n_bits;
			bytes_out += bits;
			do {
				putchar(*bp++);
			} while(--bits);
			offset = 0;
		}

		/*
		 * If the next entry is going to be too big for the code size,
		 * then increase it, if possible.
		 */
		if ( free_ent > maxcode || (clear_flg > 0)) {
			/*
			* Write the whole buffer, because the input side won't
			* discover the size increase until after it has read it.
			*/
			if ( offset > 0 ) {
				if( fwrite( buf, 1, n_bits, stdout ) != n_bits)
					writeerr();
				bytes_out += n_bits;
			}
			offset = 0;

			if ( clear_flg ) {
				maxcode = MAXCODE (n_bits = INIT_BITS);
				clear_flg = 0;
			} else {
				n_bits++;
				if ( n_bits == maxbits )
					maxcode = maxmaxcode;
				else
					maxcode = MAXCODE(n_bits);
			}
#ifdef DEBUG
			if ( debug ) {
				fprintf(stderr,
					"\nChange to %d bits\n", n_bits);
				col = 0;
			}
#endif
		}
	} else {
		/*
		 * At EOF, write the rest of the buffer.
		 */
		if ( offset > 0 )
			fwrite( buf, 1, (offset + 7) / 8, stdout );
		bytes_out += (offset + 7) / 8;
		offset = 0;
		fflush( stdout );
#ifdef DEBUG
		if ( verbose )
			fprintf( stderr, "\n" );
#endif
		if( ferror( stdout ) )
			writeerr();
	}
}

/*
 * Decompress stdin to stdout.  This routine adapts to the codes in the
 * file building the "string" table on-the-fly; requiring no table to
 * be stored in the compressed file.  The tables used herein are shared
 * with those of the compress() routine.  See the definitions above.
 */
void
decompress(void)
{
	int finchar;
	code_int code, oldcode, incode;
	uchar *stackp;

	/*
	* As above, initialize the first 256 entries in the table.
	*/
	maxcode = MAXCODE(n_bits = INIT_BITS);
	for (code = 255; code >= 0; code--) {
		tab_prefixof(code) = 0;
		tab_suffixof(code) = (uchar)code;
	}
	free_ent = (block_compress? FIRST: 256);

	finchar = oldcode = getcode();
	if(oldcode == -1)		/* EOF already? */
		return;			/* Get out of here */
	putchar((char)finchar);		/* first code must be 8 bits = char */
	if(ferror(stdout))		/* Crash if can't write */
		writeerr();
	stackp = de_stack;

	while ((code = getcode()) > -1) {
		if ((code == CLEAR) && block_compress) {
			for (code = 255; code >= 0; code--)
				tab_prefixof(code) = 0;
			clear_flg = 1;
			free_ent = FIRST - 1;
			if ((code = getcode()) == -1)	/* O, untimely death! */
				break;
		}
		incode = code;
		/*
		 * Special case for KwKwK string.
		 */
		if (code >= free_ent) {
			*stackp++ = finchar;
			code = oldcode;
		}

		/*
		 * Generate output characters in reverse order
		 */
		while (code >= 256) {
			*stackp++ = tab_suffixof(code);
			code = tab_prefixof(code);
		}
		*stackp++ = finchar = tab_suffixof(code);

		/*
		 * And put them out in forward order
		 */
		do {
			putchar(*--stackp);
		} while (stackp > de_stack);

		/*
		 * Generate the new entry.
		 */
		if ( (code=free_ent) < maxmaxcode ) {
			tab_prefixof(code) = (ushort)oldcode;
			tab_suffixof(code) = finchar;
			free_ent = code+1;
		}
		/*
		 * Remember previous code.
		 */
		oldcode = incode;
	}
	fflush(stdout);
	if(ferror(stdout))
		writeerr();
}

/*
 * TAG( getcode )
 *
 * Read one code from the standard input.  If EOF, return -1.
 * Inputs:
 * 	stdin
 * Outputs:
 * 	code or -1 is returned.
 */
code_int
getcode()
{
	int r_off, bits;
	code_int code;
	static int offset = 0, size = 0;
	static uchar buf[BITS];
	uchar *bp = buf;

	if ( clear_flg > 0 || offset >= size || free_ent > maxcode ) {
		/*
		 * If the next entry will be too big for the current code
		 * size, then we must increase the size.  This implies reading
		 * a new buffer full, too.
		 */
		if ( free_ent > maxcode ) {
			n_bits++;
			if ( n_bits == maxbits )
				maxcode = maxmaxcode; /* won't get any bigger now */
			else
				maxcode = MAXCODE(n_bits);
		}
		if ( clear_flg > 0) {
			maxcode = MAXCODE(n_bits = INIT_BITS);
			clear_flg = 0;
		}
		size = fread(buf, 1, n_bits, stdin);
		if (size <= 0)
			return -1;			/* end of file */
		offset = 0;
		/* Round size down to integral number of codes */
		size = (size << 3) - (n_bits - 1);
	}
	r_off = offset;
	bits = n_bits;
	/*
	 * Get to the first byte.
	 */
	bp += (r_off >> 3);
	r_off &= 7;
	/* Get first part (low order bits) */
	code = (*bp++ >> r_off);
	bits -= (8 - r_off);
	r_off = 8 - r_off;		/* now, offset into code word */
	/* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
	if (bits >= 8) {
		code |= *bp++ << r_off;
		r_off += 8;
		bits -= 8;
	}
	/* high order bits. */
	code |= (*bp & rmask[bits]) << r_off;
	offset += n_bits;
	return code;
}

#ifdef DEBUG
printcodes()
{
	/*
	* Just print out codes from input file.  For debugging.
	*/
	code_int code;
	int col = 0, bits;

	bits = n_bits = INIT_BITS;
	maxcode = MAXCODE(n_bits);
	free_ent = ((block_compress) ? FIRST : 256 );
	while ( ( code = getcode() ) >= 0 ) {
	if ( (code == CLEAR) && block_compress ) {
			free_ent = FIRST - 1;
			clear_flg = 1;
	}
	else if ( free_ent < maxmaxcode )
		free_ent++;
	if ( bits != n_bits ) {
		fprintf(stderr, "\nChange to %d bits\n", n_bits );
		bits = n_bits;
		col = 0;
	}
	fprintf(stderr, "%5d%c", code, (col+=6) >= 74 ? (col = 0, '\n') : ' ' );
	}
	putc( '\n', stderr );
	exit( 0 );
}

code_int sorttab[1<<BITS];	/* sorted pointers into htab */

#define STACK_SIZE	15000

dump_tab()	/* dump string table */
{
	int i, first, c, ent;
	int stack_top = STACK_SIZE;

	if(do_decomp == 0) {	/* compressing */
	int flag = 1;

	for(i=0; i<hsize; i++) {	/* build sort pointers */
		if((long)htabof(i) >= 0) {
			sorttab[codetabof(i)] = i;
		}
	}
	first = block_compress ? FIRST : 256;
	for(i = first; i < free_ent; i++) {
		fprintf(stderr, "%5d: \"", i);
		de_stack[--stack_top] = '\n';
		de_stack[--stack_top] = '"';
		stack_top = in_stack((htabof(sorttab[i])>>maxbits)&0xff,
	stack_top);
		for(ent=htabof(sorttab[i]) & ((1<<maxbits)-1);
			ent > 256;
			ent=htabof(sorttab[ent]) & ((1<<maxbits)-1)) {
			stack_top = in_stack(htabof(sorttab[ent]) >> maxbits,
						stack_top);
		}
		stack_top = in_stack(ent, stack_top);
		fwrite( &de_stack[stack_top], 1, STACK_SIZE-stack_top, stderr);
			stack_top = STACK_SIZE;
	}
	} else if(!debug) {	/* decompressing */

	for ( i = 0; i < free_ent; i++ ) {
		ent = i;
		c = tab_suffixof(ent);
		if ( isascii(c) && isprint(c) )
		fprintf( stderr, "%5d: %5d/'%c'  \"",
				ent, tab_prefixof(ent), c );
		else
		fprintf( stderr, "%5d: %5d/\\%03o \"",
				ent, tab_prefixof(ent), c );
		de_stack[--stack_top] = '\n';
		de_stack[--stack_top] = '"';
		for ( ; ent != NULL;
			ent = (ent >= FIRST ? tab_prefixof(ent) : NULL) ) {
		stack_top = in_stack(tab_suffixof(ent), stack_top);
		}
		fwrite( &de_stack[stack_top], 1, STACK_SIZE - stack_top, stderr );
		stack_top = STACK_SIZE;
	}
	}
}

int
in_stack(int c, int stack_top)
{
	if ( (isascii(c) && isprint(c) && c != '\\') || c == ' ' ) {
		de_stack[--stack_top] = c;
	} else {
		switch( c ) {
		case '\n': de_stack[--stack_top] = 'n'; break;
		case '\t': de_stack[--stack_top] = 't'; break;
		case '\b': de_stack[--stack_top] = 'b'; break;
		case '\f': de_stack[--stack_top] = 'f'; break;
		case '\r': de_stack[--stack_top] = 'r'; break;
		case '\\': de_stack[--stack_top] = '\\'; break;
		default:
	 	de_stack[--stack_top] = '0' + c % 8;
	 	de_stack[--stack_top] = '0' + (c / 8) % 8;
	 	de_stack[--stack_top] = '0' + c / 64;
	 	break;
		}
		de_stack[--stack_top] = '\\';
	}
	return stack_top;
}
#endif /* DEBUG */

void
writeerr(void)
{
	perror(ofname);
	unlink(ofname);
	exit(1);
}

void
copystat(ifname, ofname)
char *ifname, *ofname;
{
	int mode;
	time_t timep[2];			/* should be struct utimbuf */
	struct stat statbuf;

	fclose(stdout);
	if (stat(ifname, &statbuf)) {		/* Get stat on input file */
		perror(ifname);
		return;
	}
	if (!S_ISREG(statbuf.st_mode)) {
		if (quiet)
			fprintf(stderr, "%s: ", ifname);
		fprintf(stderr, " -- not a regular file: unchanged");
		exit_stat = 1;
	} else if (exit_stat == 2 && !force) {
		/* No compression: remove file.Z */
		if (!quiet)
			fprintf(stderr, " -- file unchanged");
	} else {			/* Successful Compression */
		exit_stat = 0;
		mode = statbuf.st_mode & 0777;
		if (chmod(ofname, mode))		/* Copy modes */
			perror(ofname);
		/* Copy ownership */
		chown(ofname, statbuf.st_uid, statbuf.st_gid);
		timep[0] = statbuf.st_atime;
		timep[1] = statbuf.st_mtime;
		/* Update last accessed and modified times */
		utime(ofname, (struct utimbuf *)timep);
//		if (unlink(ifname))	/* Remove input file */
//			perror(ifname);
		return;			/* success */
	}

	/* Unsuccessful return -- one of the tests failed */
	if (unlink(ofname))
		perror(ofname);
}

/*
 * This routine returns 1 if we are running in the foreground and stderr
 * is a tty.
 */
int
foreground(void)
{
	if(bgnd_flag)			/* background? */
		return 0;
	else				/* foreground */
		return isatty(2);	/* and stderr is a tty */
}

void
onintr(int x)
{
	USED(x);
	unlink(ofname);
	exit(1);
}

void
oops(int x)		/* wild pointer -- assume bad input */
{
	USED(x);
	if (do_decomp == 1)
		fprintf(stderr, "uncompress: corrupt input\n");
	unlink(ofname);
	exit(1);
}

void
cl_block(void)		/* table clear for block compress */
{
	long rat;

	checkpoint = in_count + CHECK_GAP;
#ifdef DEBUG
	if ( debug ) {
		fprintf ( stderr, "count: %ld, ratio: ", in_count );
		prratio ( stderr, in_count, bytes_out );
		fprintf ( stderr, "\n");
	}
#endif /* DEBUG */

	if (in_count > 0x007fffff) {	/* shift will overflow */
		rat = bytes_out >> 8;
		if (rat == 0)		/* Don't divide by zero */
			rat = 0x7fffffff;
		else
			rat = in_count / rat;
	} else
		rat = (in_count << 8) / bytes_out;	/* 8 fractional bits */
	if (rat > ratio)
		ratio = rat;
	else {
		ratio = 0;
#ifdef DEBUG
		if (verbose)
			dump_tab();	/* dump string table */
#endif
		cl_hash((count_int)hsize);
		free_ent = FIRST;
		clear_flg = 1;
		output((code_int)CLEAR);
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "clear\n");
#endif /* DEBUG */
	}
}

void
cl_hash(count_int hsize)		/* reset code table */
{
	count_int *htab_p = htab+hsize;
	long i;
	long m1 = -1;

	i = hsize - 16;
 	do {				/* might use Sys V memset(3) here */
		*(htab_p-16) = m1;
		*(htab_p-15) = m1;
		*(htab_p-14) = m1;
		*(htab_p-13) = m1;
		*(htab_p-12) = m1;
		*(htab_p-11) = m1;
		*(htab_p-10) = m1;
		*(htab_p-9) = m1;
		*(htab_p-8) = m1;
		*(htab_p-7) = m1;
		*(htab_p-6) = m1;
		*(htab_p-5) = m1;
		*(htab_p-4) = m1;
		*(htab_p-3) = m1;
		*(htab_p-2) = m1;
		*(htab_p-1) = m1;
		htab_p -= 16;
	} while ((i -= 16) >= 0);
	for ( i += 16; i > 0; i-- )
		*--htab_p = m1;
}

void
prratio(stream, num, den)
FILE *stream;
long num, den;
{
	int q;				/* Doesn't need to be long */

	if(num > 214748L)		/* 2147483647/10000 */
		q = num / (den / 10000L);
	else
		q = 10000L * num / den;	/* Long calculations, though */
	if (q < 0) {
		putc('-', stream);
		q = -q;
	}
	fprintf(stream, "%d.%02d%%", q / 100, q % 100);
}

void
version(void)
{
	fprintf(stderr, "%s\n", rcs_ident);
	fprintf(stderr, "Options: ");
#ifdef DEBUG
	fprintf(stderr, "DEBUG, ");
#endif
#ifdef BSD4_2
	fprintf(stderr, "BSD4_2, ");
#endif
	fprintf(stderr, "BITS = %d\n", BITS);
}

/*
 * The revision-history novel:
 *
 * $Header: compress.c,v 4.0 85/07/30 12:50:00 joe Release $
 * $Log:	compress.c,v $
 * Revision 4.0  85/07/30  12:50:00  joe
 * Removed ferror() calls in output routine on every output except first.
 * Prepared for release to the world.
 *
 * Revision 3.6  85/07/04  01:22:21  joe
 * Remove much wasted storage by overlaying hash table with the tables
 * used by decompress: tab_suffix[1<<BITS], stack[8000].  Updated USERMEM
 * computations.  Fixed dump_tab() DEBUG routine.
 *
 * Revision 3.5  85/06/30  20:47:21  jaw
 * Change hash function to use exclusive-or.  Rip out hash cache.  These
 * speedups render the megamemory version defunct, for now.  Make decoder
 * stack global.  Parts of the RCS trunks 2.7, 2.6, and 2.1 no longer apply.
 *
 * Revision 3.4  85/06/27  12:00:00  ken
 * Get rid of all floating-point calculations by doing all compression ratio
 * calculations in fixed point.
 *
 * Revision 3.3  85/06/24  21:53:24  joe
 * Incorporate portability suggestion for M_XENIX.  Got rid of text on #else
 * and #endif lines.  Cleaned up #ifdefs for vax and interdata.
 *
 * Revision 3.2  85/06/06  21:53:24  jaw
 * Incorporate portability suggestions for Z8000, IBM PC/XT from mailing list.
 * Default to "quiet" output (no compression statistics).
 *
 * Revision 3.1  85/05/12  18:56:13  jaw
 * Integrate decompress() stack speedups (from early pointer mods by McKie).
 * Repair multi-file USERMEM gaffe.  Unify 'force' flags to mimic semantics
 * of SVR2 'pack'.  Streamline block-compress table clear logic.  Increase
 * output byte count by magic number size.
 *
 * Revision 3.0	84/11/27  11:50:00  petsd!joe
 * Set HSIZE depending on BITS.  Set BITS depending on USERMEM.  Unrolled
 * loops in clear routines.  Added "-C" flag for 2.0 compatibility.  Used
 * unsigned compares on Perkin-Elmer.  Fixed foreground check.
 *
 * Revision 2.7	84/11/16  19:35:39  ames!jaw
 * Cache common hash codes based on input statistics; this improves
 * performance for low-density raster images.  Pass on #ifdef bundle
 * from Turkowski.
 *
 * Revision 2.6	84/11/05  19:18:21  ames!jaw
 * Vary size of hash tables to reduce time for small files.
 * Tune PDP-11 hash function.
 *
 * Revision 2.5	84/10/30  20:15:14  ames!jaw
 * Junk chaining; replace with the simpler (and, on the VAX, faster)
 * double hashing, discussed within.  Make block compression standard.
 *
 * Revision 2.4	84/10/16  11:11:11  ames!jaw
 * Introduce adaptive reset for block compression, to boost the rate
 * another several percent.  (See mailing list notes.)
 *
 * Revision 2.3	84/09/22  22:00:00  petsd!joe
 * Implemented "-B" block compress.  Implemented REVERSE sorting of tab_next.
 * Bug fix for last bits.  Changed fwrite to putchar loop everywhere.
 *
 * Revision 2.2	84/09/18  14:12:21  ames!jaw
 * Fold in news changes, small machine typedef from thomas,
 * #ifdef interdata from joe.
 *
 * Revision 2.1	84/09/10  12:34:56  ames!jaw
 * Configured fast table lookup for 32-bit machines.
 * This cuts user time in half for b <= FBITS, and is useful for news batching
 * from VAX to PDP sites.  Also sped up decompress() [fwrite->putc] and
 * added signal catcher [plus beef in writeerr()] to delete effluvia.
 *
 * Revision 2.0	84/08/28  22:00:00  petsd!joe
 * Add check for foreground before prompting user.  Insert maxbits into
 * compressed file.  Force file being uncompressed to end with ".Z".
 * Added "-c" flag and "zcat".  Prepared for release.
 *
 * Revision 1.10  84/08/24  18:28:00  turtlevax!ken
 * Will only compress regular files (no directories), added a magic number
 * header (plus an undocumented -n flag to handle old files without headers),
 * added -f flag to force overwriting of possibly existing destination file,
 * otherwise the user is prompted for a response.  Will tack on a .Z to a
 * filename if it doesn't have one when decompressing.  Will only replace
 * file if it was compressed.
 *
 * Revision 1.9  84/08/16  17:28:00  turtlevax!ken
 * Removed scanargs(), getopt(), added .Z extension and unlimited number of
 * filenames to compress.  Flags may be clustered (-Ddvb12) or separated
 * (-D -d -v -b 12), or combination thereof.  Modes and other status is
 * copied with copystat().  -O bug for 4.2 seems to have disappeared with
 * 1.8.
 *
 * Revision 1.8  84/08/09  23:15:00  joe
 * Made it compatible with vax version, installed jim's fixes/enhancements
 *
 * Revision 1.6  84/08/01  22:08:00  joe
 * Sped up algorithm significantly by sorting the compress chain.
 *
 * Revision 1.5  84/07/13  13:11:00  srd
 * Added C version of vax asm routines.  Changed structure to arrays to
 * save much memory.  Do unsigned compares where possible (faster on
 * Perkin-Elmer)
 *
 * Revision 1.4  84/07/05  03:11:11  thomas
 * Clean up the code a little and lint it.  (Lint complains about all
 * the regs used in the asm, but I'm not going to "fix" this.)
 *
 * Revision 1.3  84/07/05  02:06:54  thomas
 * Minor fixes.
 *
 * Revision 1.2  84/07/05  00:27:27  thomas
 * Add variable bit length output.
 */
