/*
 * mc - columnate
 *
 * mc[-][-LINEWIDTH][-t][file...]
 *	- causes break on colon
 *	-LINEWIDTH sets width of line in which to columnate(default 80)
 *	-t suppresses expanding multiple blanks into tabs
 *
 */
#include	<u.h>
#include	<sys/ioctl.h>
#include	<sys/termios.h>
#include	<libc.h>
#include	<draw.h>
#include	<bio.h>

#define	WIDTH			80
#define	TAB	4
#define	WORD_ALLOC_QUANTA	1024
#define	ALLOC_QUANTA		4096

int linewidth=WIDTH;
int colonflag=0;
int tabflag=0;	/* -t flag turned off forever */
Rune *cbuf, *cbufp;
Rune **word;
int maxwidth=0;
int nalloc=ALLOC_QUANTA;
int nwalloc=WORD_ALLOC_QUANTA;
int nchars=0;
int nwords=0;
Biobuf	bin;
Biobuf	bout;

void getwidth(void), readbuf(int), error(char *);
void scanwords(void), columnate(void), morechars(void);

void
main(int argc, char *argv[])
{
	int i;
	int lineset;
	int ifd;

	lineset = 0;
	Binit(&bout, 1, OWRITE);
	while(argc > 1 && argv[1][0] == '-'){
		--argc; argv++;
		switch(argv[0][1]){
		case '\0':
			colonflag = 1;
			break;
		case 't':
			tabflag = 0;
			break;
		default:
			linewidth = atoi(&argv[0][1]);
			if(linewidth <= 1)
				linewidth = WIDTH;
			lineset = 1;
			break;
		}
	}
	if(lineset == 0)
		getwidth();
	cbuf = cbufp = malloc(ALLOC_QUANTA*(sizeof *cbuf));
	word = malloc(WORD_ALLOC_QUANTA*(sizeof *word));
	if(word == 0 || cbuf == 0)
		error("out of memory");
	if(argc == 1)
		readbuf(0);
	else{
		for(i = 1; i < argc; i++){
			if((ifd = open(*++argv, OREAD)) == -1)
				fprint(2, "mc: can't open %s (%r)\n", *argv);
			else{
				readbuf(ifd);
				Bflush(&bin);
				close(ifd);
			}
		}
	}
	columnate();
	exits(0);
}
void
error(char *s)
{
	fprint(2, "mc: %s\n", s);
	exits(s);
}
void
readbuf(int fd)
{
	int lastwascolon = 0;
	long c;
	int linesiz = 0;

	Binit(&bin, fd, OREAD);
	do{
		if(nchars++ >= nalloc)
			morechars();
		*cbufp++ = c = Bgetrune(&bin);
		linesiz++;
		if(c == '\t') {
			cbufp[-1] = L' ';
			while(linesiz%TAB != 0) {
				if(nchars++ >= nalloc)
					morechars();
				*cbufp++ = L' ';
				linesiz++;
			}
		}
		if(colonflag && c == ':')
			lastwascolon++;
		else if(lastwascolon){
			if(c == '\n'){
				--nchars; 	/* skip newline */
				*cbufp = L'\0';
				while(nchars > 0 && cbuf[--nchars] != '\n')
					;
				if(nchars)
					nchars++;
				columnate();
				if (nchars)
					Bputc(&bout, '\n');
				Bprint(&bout, "%S", cbuf+nchars);
				nchars = 0;
				cbufp = cbuf;
			}
			lastwascolon = 0;
		}
		if(c == '\n')
			linesiz = 0;
	}while(c >= 0);
}
void
scanwords(void)
{
	Rune *p, *q;
	int i;

	nwords=0;
	maxwidth=0;
	for(p = q = cbuf, i = 0; i < nchars; i++){
		if(*p++ == L'\n'){
			if(nwords >= nwalloc){
				nwalloc += WORD_ALLOC_QUANTA;
				if((word = realloc(word, nwalloc*sizeof(*word)))==0)
					error("out of memory");
			}
			word[nwords++] = q;
			p[-1] = L'\0';
			if(p-q > maxwidth)
				maxwidth = p-q;
			q = p;
		}
	}
}

void
columnate(void)
{
	int i, j;
	int words_per_line;
	int nlines;
	int col;
	int endcol;


	scanwords();
	if(nwords==0)
		return;
	words_per_line = linewidth/maxwidth;
	if(words_per_line <= 0)
		words_per_line = 1;
	nlines=(nwords+words_per_line-1)/words_per_line;
	for(i = 0; i < nlines; i++){
		col = endcol = 0;
		for(j = i; j < nwords; j += nlines){
			endcol += maxwidth;
			Bprint(&bout, "%S", word[j]);
			col += word[j+1]-word[j]-1;
			if(j+nlines < nwords){
				if(tabflag) {
					int tabcol = (col|(TAB-1))+1;
					while(tabcol <= endcol){
						Bputc(&bout, '\t');
						col = tabcol;
						tabcol += TAB;
					}
				}
				while(col < endcol){
					Bputc(&bout, ' ');
					col++;
				}
			}
		}
		Bputc(&bout, '\n');
	}
}

void
morechars(void)
{
	nalloc += ALLOC_QUANTA;
	if((cbuf = realloc(cbuf, nalloc*sizeof(*cbuf))) == 0)
		error("out of memory");
	cbufp = cbuf+nchars-1;
}

/*
 * These routines discover the width of the display.
 * It takes some work.  If we do the easy calls to the
 * draw library, the screen flashes due to repainting
 * when mc exits.
 */

int
windowrect(struct winsize *ws)
{
	int tty;

	if((tty = open("/dev/tty", OWRITE)) < 0)
		tty = 1;

	if(ioctl(tty, TIOCGWINSZ, ws) < 0){
		if(tty != 1)
			close(tty);
		return -1;
	}
	if(tty != 1)
		close(tty);
	return 0;
}

void
getwidth(void)
{
	struct winsize ws;

	if(windowrect(&ws) < 0)
		return;
	linewidth = ws.ws_col;
}
