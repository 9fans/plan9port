/* jpeg parser by tom szymanski */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

/* subroutines done by macros */
#define min(A,B)	((A)<(B) ? (A) : (B))
#define max(A,B)	((A)>(B) ? (A) : (B))
#define maxeql(A,B)	if (A < (B)) A = (B);
#define mineql(A,B)	if (A > (B)) A = (B);
#define eatarg0		(argc--, argv++)
#define arrayLength(A) ((sizeof A)/ (sizeof A[0]))

FILE *infile;
char *fname;

/* Routines to print error messages of varying severity */

/* externally visible variables */
int   warncnt;
char *myname;

void getname (char *arg) {
	/* Save name of invoking program for use by error routines */
	register char *p;
	p = strrchr (arg, '/');
	if (p == NULL)
		myname = arg;
	else
		myname = ++p;
}

static void introduction (void) {
	warncnt++;
	fflush (stdout);
	if (myname != NULL)
		fprintf (stderr, "%s: ", myname);
}

void warn (char *fmt, ...) {
	va_list args;
	introduction ();
	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);
	fputc ('\n', stderr);
	fflush (stderr);
}

void quit (char *fmt, ...) {
	va_list args;
	introduction ();
	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);
	fputc ('\n', stderr);
	fflush (stderr);
	exit (1);
}

void fatal (char *fmt, ...) {
	va_list args;
	introduction ();
	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);
	fprintf (stderr, "\nbetter get help!\n");
	fflush (stderr);
	abort ();
}

int toption = 0;
int dqt[16][64];

int get1 (void) {
	unsigned char x;
	if (fread(&x, 1, 1, infile) == 0)
		quit ("unexpected EOF");
	return x;
}

int get2 (void) {
	int x;

	x = get1() << 8;
	return x | get1();
}

void eatmarker (int kind) {
	int l;
	l = get2();
	printf ("%02x len=%d\n", kind, l);
	for (l -= 2; l > 0; l--)
		get1();
}

char *sofName[16] = {
	"Baseline sequential DCT - Huffman coding",
	"Extended sequential DCT - Huffman coding",
	"Progressive DCT - Huffman coding",
	"Lossless - Huffman coding",
	"4 is otherwise used",
	"Sequential DCT - differential Huffman coding",
	"Progressive DCT - differential Huffman coding",
	"Lossless - differential Huffman coding",
	"8 is reserved",
	"Extended Sequential DCT - arithmetic coding",
	"Progressive DCT - arithmetic coding",
	"Lossless - arithmetic coding",
	"c is otherwise used",
	"Sequential DCT - differential arithmetic coding",
	"Progressive DCT - differential arithmetic coding",
	"Lossless - differential arithmetic coding"
};

void get_sof (int kind) {
	int i, length, height, width, precision, ncomponents;
	int id, sf, tab;
	length = get2();
	precision = get1();
	height = get2();
	width = get2();
	ncomponents = get1();
	printf ("SOF%d:\t%s\n", kind - 0xc0, sofName[kind - 0xc0]);
	printf ("\t%d wide, %d high, %d deep, %d components\n",
		width, height, precision, ncomponents);
	for (i = 0; i < ncomponents; i++) {
		id = get1();
		sf = get1();
		tab = get1();
		printf ("\tcomponent %d: %d hsample, %d vsample, quantization table %d\n",
			id, sf >> 4, sf & 0xf, tab);
	}
}

void get_com (int kind) {
	int l, c;
	l = get2();
	printf ("COM len=%d '", l);
	for (l -= 2; l > 0; l--)
		putchar (c = get1());
	printf ("'\n");
}

void get_app (int kind) {
	int l, c, first;
	char buf[6];
	int nbuf, nok;
	l = get2();
	printf ("APP%d len=%d\n", kind - 0xe0, l);
	nbuf = 0;
	nok = 0;
	first = 1;
	/* dump printable strings in comment */
	for (l -= 2; l > 0; l--){
		c = get1();
		if(isprint(c)){
			if(nbuf >= sizeof buf){
				if(!first && nbuf == nok)
					printf(" ");
				printf("%.*s", nbuf, buf);
				nbuf = 0;
				first = 0;
			}
			buf[nbuf++] = c;
			nok++;
		}else{
			if(nok >= sizeof buf)
				if(nbuf > 0)
					printf("%.*s", nbuf, buf);
			nbuf = 0;
			nok = 0;
		}
	}
	if(nok >= sizeof buf)
		if(nbuf > 0){
			if(!first && nbuf == nok)
				printf(" ");
			printf("%.*s", nbuf, buf);
		}
}

void get_dac (int kind) {
	eatmarker (kind);
}

int get1dqt (void) {
	int t, p, i, *tab;
	t = get1();
	p = t >> 4;
	t = t & 0xf;
	printf ("DQT:\tp = %d, table = %d\n", p, t);
	tab = &dqt[t][0];
	for (i = 0; i < 64; i++)
		tab[i] = p ? get2() : get1();
	if (toption) {
		for (i = 0; i < 64; i++)
			printf ("\t%%q[%02d] = %d\n", i, tab[i]);
	}
	return p ? 65 : 129;
}

void get_dqt (int kind) {
	int length;
	length = get2() - 2;
	while (length > 0)
		length -= get1dqt();
}

int get1dht (void) {
	int l, tcth, i, j, v[16], vv[16][256];
	tcth = get1();
	printf ("DHT:\tclass = %d, table = %d\n", tcth >> 4, tcth & 0xf);
	for (i = 0; i < 16; i++)
		v[i] = get1();
	l = 17;
	for (i = 0; i < 16; i++)
		for (j = 0; j < v[i]; j++) {
			vv[i][j] = get1();
			l += 1;
		}
	if (toption) {
		for (i = 0; i < 16; i++)
			printf ("\t%%l[%02d] = %d\n", i+1, v[i]);
		for (i = 0; i < 16; i++)
			for (j = 0; j < v[i]; j++)
				printf ("\t%%v[%02d,%02d] = %d\n", i+1, j+1, vv[i][j]);
	}
	return l;
}

void get_dht (int kind) {
	int length;
	length = get2() - 2;
	while (length > 0)
		length -= get1dht();
}

void get_sos (int kind) {
	int i, length, ncomponents, id, dcac, ahal;
	length = get2();
	ncomponents = get1();
	printf ("SOS:\t%d components\n", ncomponents);
	for (i = 0; i < ncomponents; i++) {
		id = get1();
		dcac = get1();
		printf ("\tcomponent %d: %d DC, %d AC\n", id, dcac >> 4, dcac & 0xf);
	}
	printf ("\tstart spectral %d\n", get1());
	printf ("\tend spectral %d\n", get1());
	ahal = get1();
	printf ("\tah = %d, al = %d\n", ahal >> 4, ahal &0xf);
}

int main (int argc, char *argv[]) {
	int l, stuff, c;
	while (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {
		case 't':
			toption = 1;
			break;
		default:
			warn ("bad option '%c'", argv[1][1]);
		}
		eatarg0;
	}
	fname = argv[1];
	infile = fopen (fname, "r");
	if (infile == NULL)
		quit ("can't open %s\n", fname);
    Start:
/*	if (get1() != 0xff || get1() != 0xd8) */
/*		quit ("not JFIF"); */
/*	printf ("SOI\n"); */
/*	get_app (0xe0); */
	for (;;) {
		c = get1();
		if (c != 0xff)
			quit ("expected marker, got %2x", c);
		do {
			c = get1();
		} while (c == 0xff);
marker:
		switch (c) {
		case 0xc0: case 0xc1: case 0xc2: case 0xc3:
		case 0xc5: case 0xc6: case 0xc7:
		case 0xc8: case 0xc9: case 0xca: case 0xcb:
		case 0xcd: case 0xce: case 0xcf:
			get_sof (c);
			break;
		case 0xc4:
			get_dht (c);
			break;
		case 0xcc:
			get_dac (c);
			break;
		case 0xd8:
			printf ("SOI\n");
			break;
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb:
		case 0xec: case 0xed: case 0xee: case 0xef:
			get_app(c);
			break;
		case 0xda:
			get_sos (c);
			goto newentropy;
		case 0xdb:
			get_dqt (c);
			break;
		case 0xfe:
			get_com (c);
			break;
		case 0xd9:
			printf ("EOI\n");
			if((c=getc(infile)) == EOF)
				exit(0);
			ungetc(c, infile);
			goto Start;
		default:
			eatmarker (c);
		}
		continue;
newentropy:
		l = stuff = 0;
entropy:
		while ((c = get1()) != 0xff)
			l += 1;
		while (c == 0xff)
			c = get1();
		if (c == 0) {
			stuff += 1;
			goto entropy;
		}
		printf ("sequence length %d with %d stuffs\n", l, stuff);
		if (0xd0 <= c && c <= 0xd7) {
			printf ("restart %d\n", c - 0xd0);
			goto newentropy;
		}
		goto marker;
	}
}
