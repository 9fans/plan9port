/*
output language from troff:
all numbers are character strings

sn	size in points
fn	font as number from 1-n
cx	ascii character x
Cxyz	funny char xyz. terminated by white space
Nn	absolute character number n on this font.  ditto
Hn	go to absolute horizontal position n
Vn	go to absolute vertical position n (down is positive)
hn	go n units horizontally (relative)
vn	ditto vertically
nnc	move right nn, then print c (exactly 2 digits!)
		(this wart is an optimization that shrinks output file size
		 about 35% and run-time about 15% while preserving ascii-ness)
Dt ...\n	draw operation 't':
	Dl x y		line from here by x,y
	Dc d		circle of diameter d with left side here
	De x y		ellipse of axes x,y with left side here
	Da dx dy dx dy	arc counter-clockwise, center at dx,dx, end at dx,dy
	D~ x y x y ...	wiggly line by x,y then x,y ...
nb a	end of line (information only -- no action needed)
w	paddable word space -- no action needed
	b = space before line, a = after
p	new page begins -- set v to 0
#...\n	comment
x ...\n	device control functions:
	x i	init
	x T s	name of device is s
	x r n h v	resolution is n/inch
		h = min horizontal motion, v = min vert
	x p	pause (can restart)
	x s	stop -- done for ever
	x t	generate trailer
	x f n s	font position n contains font s
	x H n	set character height to n
	x S n	set slant to N

	Subcommands like "i" are often spelled out like "init".
*/

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <bio.h>

#define hmot(n)	hpos += n
#define hgoto(n)	hpos = n
#define vmot(n)	vgoto(vpos + n)
#define vgoto(n)	vpos = n

#define	putchar(x)	Bprint(&bout, "%C", x)

int	hpos;	/* horizontal position where we are supposed to be next (left = 0) */
int	vpos;	/* current vertical position (down positive) */
char	*fontfile	= "/lib/font/bit/pelm/unicode.9x24.font";

char	*pschar(char *, char *hex, int *wid, int *ht);
int	kanji(char *);
void	Bgetstr(Biobuf *bp, char *s);
void	Bgetline(Biobuf *bp, char *s);
void	Bgetint(Biobuf *bp, int *n);

Biobuf bin, bout;

void
main(void)
{
	int c, n;
	char str[100], *args[10];
	int jfont, curfont;

	if(initdraw(0, fontfile, 0) < 0){
		fprint(2, "mnihongo: can't initialize display: %r\n");
		exits("open");
	}
	Binit(&bin, 0, OREAD);
	Binit(&bout, 1, OWRITE);

	jfont = -1;
	curfont = 1;
	while ((c = Bgetc(&bin)) >= 0) {
		switch (c) {
		case '\n':	/* when input is text */
		case ' ':
		case '\0':		/* occasional noise creeps in */
			putchar(c);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/* two motion digits plus a character */
			putchar(c);	/* digit 1 */
			n = (c-'0')*10;
			c = Bgetc(&bin);
			putchar(c);	/* digit 2 */
			n += c - '0';
			hmot(n);
			putchar(Bgetc(&bin));	/* char itself */
			break;
		case 'c':	/* single character */
			c = Bgetrune(&bin);
			if(c==' ')	/* why does this happen? it's troff - bwk */
				break;
			else if(jfont == curfont){
				Bungetrune(&bin);
				Bgetstr(&bin, str);
				kanji(str);
			}else{
				putchar('c');
				putchar(c);
			}
			break;
		case 'C':
			Bgetstr(&bin, str);
			Bprint(&bout, "C%s", str);
			break;
		case 'f':
			Bgetstr(&bin, str);
			curfont = atoi(str);
			if(curfont < 0 || curfont > 20)
				curfont = 1;	/* sanity */
			Bprint(&bout, "%c%s", c, str);
			break;
		case 'N':	/* absolute character number */
		case 's':
		case 'p':	/* new page */
			Bgetint(&bin, &n);
			Bprint(&bout, "%c%d", c, n);
			break;
		case 'H':	/* absolute horizontal motion */
			Bgetint(&bin, &n);
			Bprint(&bout, "%c%d", c, n);
			hgoto(n);
			break;
		case 'h':	/* relative horizontal motion */
			Bgetint(&bin, &n);
			Bprint(&bout, "%c%d", c, n);
			hmot(n);
			break;
		case 'V':
			Bgetint(&bin, &n);
			Bprint(&bout, "%c%d", c, n);
			vgoto(n);
			break;
		case 'v':
			Bgetint(&bin, &n);
			Bprint(&bout, "%c%d", c, n);
			vmot(n);
			break;

		case 'w':	/* word space */
			putchar(c);
			break;

		case 'x':	/* device control */
			Bgetline(&bin, str);
			Bprint(&bout, "%c%s", c, str);
			if(tokenize(str, args, 10)>2 && args[0][0]=='f' && ('0'<=args[1][0] && args[1][0]<='9')){
				if(strncmp(args[2], "Jp", 2) == 0)
					jfont = atoi(args[1]);
				else if(atoi(args[1]) == jfont)
					jfont = -1;
			}
			break;

		case 'D':	/* draw function */
		case 'n':	/* end of line */
		case '#':	/* comment */
			Bgetline(&bin, str);
			Bprint(&bout, "%c%s", c, str);
			break;
		default:
			fprint(2, "mnihongo: unknown input character %o %c\n", c, c);
			exits("error");
		}
	}
}

int kanji(char *s)	/* very special pleading */
{			/* dump as kanji char if looks like one */
	Rune r;
	char hex[500];
	int size = 10, ht, wid;

	chartorune(&r, s);
	pschar(s, hex, &wid, &ht);
	Bprint(&bout, "x X PS save %d %d m\n", hpos, vpos);
	Bprint(&bout, "x X PS currentpoint translate %d %d scale ptsize dup scale\n", size, size);
	Bprint(&bout, "x X PS %d %d true [%d 0 0 -%d 0 %d]\n",
		wid, ht, wid, wid, ht-2);	/* kludge; ought to use ->ascent */
	Bprint(&bout, "x X PS {<%s>}\n", hex);
	Bprint(&bout, "x X PS imagemask restore\n");
	return 1;
}

char *pschar(char *s, char *hex, int *wid, int *ht)
{
	Point chpt, spt;
	Image *b;
	uchar rowdata[100];
	char *hp = hex;
	int y, i;

	chpt = stringsize(font, s);		/* bounding box of char */
	*wid = ((chpt.x+7) / 8) * 8;
	*ht = chpt.y;
	/* postscript is backwards to video, so draw white (ones) on black (zeros) */
	b = allocimage(display, Rpt(ZP, chpt), GREY1, 0, DBlack);	/* place to put it */
	spt = string(b, Pt(0,0), display->white, ZP, font, s);	/* put it there */
/* Bprint(&bout, "chpt %P, spt %P, wid,ht %d,%d\n", chpt, spt, *wid, *ht);
/* Bflush(&bout); */
	for (y = 0; y < chpt.y; y++) {	/* read bits a row at a time */
		memset(rowdata, 0, sizeof rowdata);
		unloadimage(b, Rect(0, y, chpt.x, y+1), rowdata, sizeof rowdata);
		for (i = 0; i < spt.x; i += 8) {	/* 8 == byte */
			sprint(hp, "%2.2x", rowdata[i/8]);
			hp += 2;
		}
	}
	*hp = 0;
	freeimage(b);
	return hex;
}


void	Bgetstr(Biobuf *bp, char *s)	/* get a string */
{
	int c;

	while ((c = Bgetc(bp)) >= 0) {
		if (c == ' ' || c == '\t' || c == '\n') {
			Bungetc(bp);
			break;
		}
		*s++ = c;
	}
	*s = 0;
}

void	Bgetline(Biobuf *bp, char *s)	/* get a line, including newline */
{
	int c;

	while ((c = Bgetc(bp)) >= 0) {
		*s++ = c;
		if (c == '\n')
			break;
	}
	*s = 0;
}

void	Bgetint(Biobuf *bp, int *n)	/* get an integer */
{
	double d;

	Bgetd(bp, &d);
	*n = d;
}
