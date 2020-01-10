#include <u.h>
#include <libc.h>
#include <bio.h>

/*
 * Deroff command -- strip troff, eqn, and tbl sequences from
 * a file.  Has three flags argument, -w, to cause output one word per line
 * rather than in the original format.
 * -mm (or -ms) causes the corresponding macro's to be interpreted
 * so that just sentences are output
 * -ml  also gets rid of lists.
 * -i causes deroff to ignore .so and .nx commands.
 * Deroff follows .so and .nx commands, removes contents of macro
 * definitions, equations (both .EQ ... .EN and $...$),
 * Tbl command sequences, and Troff backslash vconstructions.
 *
 * All input is through the C macro; the most recently read character is in c.
 */

/*
#define	C	((c = Bgetrune(infile)) < 0?\
			eof():\
			((c == ldelim) && (filesp == files)?\
				skeqn():\
				(c == '\n'?\
					(linect++,c):\
						c)))

#define	C1	((c = Bgetrune(infile)) == Beof?\
			eof():\
			(c == '\n'?\
				(linect++,c):\
				c))
*/

/* lose those macros! */
#define	C	fC()
#define	C1	fC1()

#define	SKIP	while(C != '\n')
#define SKIP1	while(C1 != '\n')
#define SKIP_TO_COM		SKIP;\
				SKIP;\
				pc=c;\
				while(C != '.' || pc != '\n' || C > 'Z')\
						pc=c

#define YES		1
#define NO		0
#define MS		0
#define MM		1
#define ONE		1
#define TWO		2

#define NOCHAR		-2
#define	EXTENDED	-1		/* All runes above 0x7F */
#define SPECIAL		0
#define APOS		1
#define PUNCT		2
#define DIGIT		3
#define LETTER		4


int	linect	= 0;
int	wordflag= NO;
int	underscoreflag = NO;
int	msflag	= NO;
int	iflag	= NO;
int	mac	= MM;
int	disp	= 0;
int	inmacro	= NO;
int	intable	= NO;
int	eqnflag	= 0;

#define	MAX_ASCII	0X80

char	chars[MAX_ASCII];	/* SPECIAL, PUNCT, APOS, DIGIT, or LETTER */

Rune	line[30000];
Rune*	lp;

long	c;
long	pc;
int	ldelim	= NOCHAR;
int	rdelim	= NOCHAR;


char**	argv;

char	fname[50];
Biobuf*	files[15];
Biobuf**filesp;
Biobuf*	infile;
char*	devnull	= "/dev/null";
Biobuf	*infile;
Biobuf	bout;

long	skeqn(void);
Biobuf*	opn(char *p);
int	eof(void);
int	charclass(int);
void	getfname(void);
void	fatal(char *s, char *p);
void	usage(void);
void	work(void);
void	putmac(Rune *rp, int vconst);
void	regline(int macline, int vconst);
void	putwords(void);
void	comline(void);
void	macro(void);
void	eqn(void);
void	tbl(void);
void	stbl(void);
void	sdis(char a1, char a2);
void	sce(void);
void	backsl(void);
char*	copys(char *s);
void	refer(int c1);
void	inpic(void);

int
fC(void)
{
	c = Bgetrune(infile);
	if(c < 0)
		return eof();
	if(c == ldelim && filesp == files)
		return skeqn();
	if(c == '\n')
		linect++;
	return c;
}

int
fC1(void)
{
	c = Bgetrune(infile);
	if(c == Beof)
		return eof();
	if(c == '\n')
		linect++;
	return c;
}

void
main(int argc, char *av[])
{
	int i;
	char *f;

	argv = av;
	Binit(&bout, 1, OWRITE);
	ARGBEGIN{
	case 'w':
		wordflag = YES;
		break;
	case '_':
		wordflag = YES;
		underscoreflag = YES;
		break;
	case 'm':
		msflag = YES;
		if(f = ARGF())
			switch(*f)
			{
			case 'm':	mac = MM; break;
			case 's':	mac = MS; break;
			case 'l':	disp = 1; break;
			default:	usage();
			}
		else
			usage();
		break;
	case 'i':
		iflag = YES;
		break;
	default:
		usage();
	}ARGEND
	if(*argv)
		infile = opn(*argv++);
	else{
		infile = malloc(sizeof(Biobuf));
		Binit(infile, 0, OREAD);
	}
	files[0] = infile;
	filesp = &files[0];

	for(i='a'; i<='z' ; ++i)
		chars[i] = LETTER;
	for(i='A'; i<='Z'; ++i)
		chars[i] = LETTER;
	for(i='0'; i<='9'; ++i)
		chars[i] = DIGIT;
	chars['\''] = APOS;
	chars['&'] = APOS;
	chars['\b'] = APOS;
	chars['.'] = PUNCT;
	chars[','] = PUNCT;
	chars[';'] = PUNCT;
	chars['?'] = PUNCT;
	chars[':'] = PUNCT;
	work();
}

long
skeqn(void)
{
	while(C1 != rdelim)
		if(c == '\\')
			c = C1;
		else if(c == '"')
			while(C1 != '"')
				if(c == '\\')
					C1;
	if (msflag)
		eqnflag = 1;
	return(c = ' ');
}

Biobuf*
opn(char *p)
{
	Biobuf *fd;

	while ((fd = Bopen(p, OREAD)) == 0) {
		if(msflag || p == devnull)
			fatal("Cannot open file %s - quitting\n", p);
		else {
			fprint(2, "Deroff: Cannot open file %s - continuing\n", p);
			p = devnull;
		}
	}
	linect = 0;
	return(fd);
}

int
eof(void)
{
	if(Bfildes(infile) != 0)
		Bterm(infile);
	if(filesp > files)
		infile = *--filesp;
	else
	if(*argv)
		infile = opn(*argv++);
	else
		exits(0);
	return(C);
}

void
getfname(void)
{
	char *p;
	Rune r;
	Dir *dir;
	struct chain
	{
		struct	chain*	nextp;
		char*	datap;
	} *q;

	static struct chain *namechain= 0;

	while(C == ' ')
		;
	for(p = fname; (r=c) != '\n' && r != ' ' && r != '\t' && r != '\\'; C)
		p += runetochar(p, &r);
	*p = '\0';
	while(c != '\n')
		C;
	if(!strcmp(fname, "/sys/lib/tmac/tmac.cs")
			|| !strcmp(fname, "/sys/lib/tmac/tmac.s")) {
		fname[0] = '\0';
		return;
	}
	dir = dirstat(fname);
	if(dir!=nil && ((dir->mode & DMDIR) || dir->type != 'M')) {
		free(dir);
		fname[0] = '\0';
		return;
	}
	free(dir);
	/*
	 * see if this name has already been used
	 */

	for(q = namechain; q; q = q->nextp)
		if( !strcmp(fname, q->datap)) {
			fname[0] = '\0';
			return;
		}
	q = (struct chain*)malloc(sizeof(struct chain));
	q->nextp = namechain;
	q->datap = copys(fname);
	namechain = q;
}

void
usage(void)
{
	fprint(2,"usage: deroff [-nw_pi] [-m (m s l)] [file ...] \n");
	exits("usage");
}

void
fatal(char *s, char *p)
{
	fprint(2, "deroff: ");
	fprint(2, s, p);
	exits(s);
}

void
work(void)
{

	for(;;) {
		eqnflag = 0;
		if(C == '.'  ||  c == '\'')
			comline();
		else
			regline(NO, TWO);
	}
}

void
regline(int macline, int vconst)
{
	line[0] = c;
	lp = line;
	for(;;) {
		if(c == '\\') {
			*lp = ' ';
			backsl();
			if(c == '%')	/* no blank for hyphenation char */
				lp--;
		}
		if(c == '\n')
			break;
		if(intable && c=='T') {
			*++lp = C;
			if(c=='{' || c=='}') {
				lp[-1] = ' ';
				*lp = C;
			}
		} else {
			if(msflag == 1 && eqnflag == 1) {
				eqnflag = 0;
				*++lp = 'x';
			}
			*++lp = C;
		}
	}
	*lp = '\0';
	if(lp != line) {
		if(wordflag)
			putwords();
		else
		if(macline)
			putmac(line,vconst);
		else
			Bprint(&bout, "%S\n", line);
	}
}

void
putmac(Rune *rp, int vconst)
{
	Rune *t;
	int found;
	Rune last;

	found = 0;
	last = 0;
	while(*rp) {
		while(*rp == ' ' || *rp == '\t')
			Bputrune(&bout, *rp++);
		for(t = rp; *t != ' ' && *t != '\t' && *t != '\0'; t++)
			;
		if(*rp == '\"')
			rp++;
		if(t > rp+vconst && charclass(*rp) == LETTER
				&& charclass(rp[1]) == LETTER) {
			while(rp < t)
				if(*rp == '\"')
					rp++;
				else
					Bputrune(&bout, *rp++);
			last = t[-1];
			found++;
		} else
		if(found && charclass(*rp) == PUNCT && rp[1] == '\0')
			Bputrune(&bout, *rp++);
		else {
			last = t[-1];
			rp = t;
		}
	}
	Bputc(&bout, '\n');
	if(msflag && charclass(last) == PUNCT)
		Bprint(&bout, " %C\n", last);
}

/*
 * break into words for -w option
 */
void
putwords(void)
{
	Rune *p, *p1;
	int i, nlet;


	for(p1 = line;;) {
		/*
		 * skip initial specials ampersands and apostrophes
		 */
		while((i = charclass(*p1)) != EXTENDED && i < DIGIT)
			if(*p1++ == '\0')
				return;
		nlet = 0;
		for(p = p1; (i = charclass(*p)) != SPECIAL || (underscoreflag && *p=='_'); p++)
			if(i == LETTER || (underscoreflag && *p == '_'))
				nlet++;
		/*
		 * MDM definition of word
		 */
		if(nlet > 1) {
			/*
			 * delete trailing ampersands and apostrophes
			 */
			while(*--p == '\'' || *p == '&'
					   || charclass(*p) == PUNCT)
				;
			while(p1 <= p)
				Bputrune(&bout, *p1++);
			Bputc(&bout, '\n');
		} else
			p1 = p;
	}
}

void
comline(void)
{
	long c1, c2;

	while(C==' ' || c=='\t')
		;
comx:
	if((c1=c) == '\n')
		return;
	c2 = C;
	if(c1=='.' && c2!='.')
		inmacro = NO;
	if(msflag && c1 == '['){
		refer(c2);
		return;
	}
	if(c2 == '\n')
		return;
	if(c1 == '\\' && c2 == '\"')
		SKIP;
	else
	if (filesp==files && c1=='E' && c2=='Q')
			eqn();
	else
	if(filesp==files && c1=='T' && (c2=='S' || c2=='C' || c2=='&')) {
		if(msflag)
			stbl();
		else
			tbl();
	}
	else
	if(c1=='T' && c2=='E')
		intable = NO;
	else if (!inmacro &&
			((c1 == 'd' && c2 == 'e') ||
		   	 (c1 == 'i' && c2 == 'g') ||
		   	 (c1 == 'a' && c2 == 'm')))
				macro();
	else
	if(c1=='s' && c2=='o') {
		if(iflag)
			SKIP;
		else {
			getfname();
			if(fname[0]) {
				if(infile = opn(fname))
					*++filesp = infile;
				else infile = *filesp;
			}
		}
	}
	else
	if(c1=='n' && c2=='x')
		if(iflag)
			SKIP;
		else {
			getfname();
			if(fname[0] == '\0')
				exits(0);
			if(Bfildes(infile) != 0)
				Bterm(infile);
			infile = *filesp = opn(fname);
		}
	else
	if(c1 == 't' && c2 == 'm')
		SKIP;
	else
	if(c1=='h' && c2=='w')
		SKIP;
	else
	if(msflag && c1 == 'T' && c2 == 'L') {
		SKIP_TO_COM;
		goto comx;
	}
	else
	if(msflag && c1=='N' && c2 == 'R')
		SKIP;
	else
	if(msflag && c1 == 'A' && (c2 == 'U' || c2 == 'I')){
		if(mac==MM)SKIP;
		else {
			SKIP_TO_COM;
			goto comx;
		}
	} else
	if(msflag && c1=='F' && c2=='S') {
		SKIP_TO_COM;
		goto comx;
	}
	else
	if(msflag && (c1=='S' || c1=='N') && c2=='H') {
		SKIP_TO_COM;
		goto comx;
	} else
	if(c1 == 'U' && c2 == 'X') {
		if(wordflag)
			Bprint(&bout, "UNIX\n");
		else
			Bprint(&bout, "UNIX ");
	} else
	if(msflag && c1=='O' && c2=='K') {
		SKIP_TO_COM;
		goto comx;
	} else
	if(msflag && c1=='N' && c2=='D')
		SKIP;
	else
	if(msflag && mac==MM && c1=='H' && (c2==' '||c2=='U'))
		SKIP;
	else
	if(msflag && mac==MM && c2=='L') {
		if(disp || c1=='R')
			sdis('L', 'E');
		else {
			SKIP;
			Bprint(&bout, " .");
		}
	} else
	if(!msflag && c1=='P' && c2=='S') {
		inpic();
	} else
	if(msflag && (c1=='D' || c1=='N' || c1=='K'|| c1=='P') && c2=='S') {
		sdis(c1, 'E');
	} else
	if(msflag && (c1 == 'K' && c2 == 'F')) {
		sdis(c1,'E');
	} else
	if(msflag && c1=='n' && c2=='f')
		sdis('f','i');
	else
	if(msflag && c1=='c' && c2=='e')
		sce();
	else {
		if(c1=='.' && c2=='.') {
			if(msflag) {
				SKIP;
				return;
			}
			while(C == '.')
				;
		}
		inmacro++;
		if(c1 <= 'Z' && msflag)
			regline(YES,ONE);
		else {
			if(wordflag)
				C;
			regline(YES,TWO);
		}
		inmacro--;
	}
}

void
macro(void)
{
	if(msflag) {
		do {
			SKIP1;
		} while(C1 != '.' || C1 != '.' || C1 == '.');
		if(c != '\n')
			SKIP;
		return;
	}
	SKIP;
	inmacro = YES;
}

void
sdis(char a1, char a2)
{
	int c1, c2;
	int eqnf;
	int lct;

	if(a1 == 'P'){
		while(C1 == ' ')
			;
		if(c == '<') {
			SKIP1;
			return;
		}
	}
	lct = 0;
	eqnf = 1;
	if(c != '\n')
		SKIP1;
	for(;;) {
		while(C1 != '.')
			if(c == '\n')
				continue;
			else
				SKIP1;
		if((c1=C1) == '\n')
			continue;
		if((c2=C1) == '\n') {
			if(a1 == 'f' && (c1 == 'P' || c1 == 'H'))
				return;
			continue;
		}
		if(c1==a1 && c2 == a2) {
			SKIP1;
			if(lct != 0){
				lct--;
				continue;
			}
			if(eqnf)
				Bprint(&bout, " .");
			Bputc(&bout, '\n');
			return;
		} else
		if(a1 == 'L' && c2 == 'L') {
			lct++;
			SKIP1;
		} else
		if(a1 == 'D' && c1 == 'E' && c2 == 'Q') {
			eqn();
			eqnf = 0;
		} else
		if(a1 == 'f') {
			if((mac == MS && c2 == 'P') ||
				(mac == MM && c1 == 'H' && c2 == 'U')){
				SKIP1;
				return;
			}
			SKIP1;
		}
		else
			SKIP1;
	}
}

void
tbl(void)
{
	while(C != '.')
		;
	SKIP;
	intable = YES;
}

void
stbl(void)
{
	while(C != '.')
		;
	SKIP_TO_COM;
	if(c != 'T' || C != 'E') {
		SKIP;
		pc = c;
		while(C != '.' || pc != '\n' || C != 'T' || C != 'E')
			pc = c;
	}
}

void
eqn(void)
{
	long c1, c2;
	int dflg;
	char last;

	last = 0;
	dflg = 1;
	SKIP;

	for(;;) {
		if(C1 == '.'  || c == '\'') {
			while(C1==' ' || c=='\t')
				;
			if(c=='E' && C1=='N') {
				SKIP;
				if(msflag && dflg) {
					Bputc(&bout, 'x');
					Bputc(&bout, ' ');
					if(last) {
						Bputc(&bout, last);
						Bputc(&bout, '\n');
					}
				}
				return;
			}
		} else
		if(c == 'd') {
			if(C1=='e' && C1=='l')
				if(C1=='i' && C1=='m') {
					while(C1 == ' ')
						;
					if((c1=c)=='\n' || (c2=C1)=='\n' ||
					  (c1=='o' && c2=='f' && C1=='f')) {
						ldelim = NOCHAR;
						rdelim = NOCHAR;
					} else {
						ldelim = c1;
						rdelim = c2;
					}
				}
			dflg = 0;
		}
		if(c != '\n')
			while(C1 != '\n') {
				if(chars[c] == PUNCT)
					last = c;
				else
				if(c != ' ')
					last = 0;
			}
	}
}

/*
 * skip over a complete backslash vconstruction
 */
void
backsl(void)
{
	int bdelim;

sw:
	switch(C1)
	{
	case '"':
		SKIP1;
		return;

	case 's':
		if(C1 == '\\')
			backsl();
		else {
			while(C1>='0' && c<='9')
				;
			Bungetrune(infile);
			c = '0';
		}
		lp--;
		return;

	case 'f':
	case 'n':
	case '*':
		if(C1 != '(')
			return;

	case '(':
		if(msflag) {
			if(C == 'e') {
				if(C1 == 'm') {
					*lp = '-';
					return;
				}
			} else
			if(c != '\n')
				C1;
			return;
		}
		if(C1 != '\n')
			C1;
		return;

	case '$':
		C1;	/* discard argument number */
		return;

	case 'b':
	case 'x':
	case 'v':
	case 'h':
	case 'w':
	case 'o':
	case 'l':
	case 'L':
		if((bdelim=C1) == '\n')
			return;
		while(C1!='\n' && c!=bdelim)
			if(c == '\\')
				backsl();
		return;

	case '\\':
		if(inmacro)
			goto sw;
	default:
		return;
	}
}

char*
copys(char *s)
{
	char *t, *t0;

	if((t0 = t = malloc((strlen(s)+1))) == 0)
		fatal("Cannot allocate memory", (char*)0);
	while(*t++ = *s++)
		;
	return(t0);
}

void
sce(void)
{
	int n = 1;

	while (C != '\n' && !('0' <= c && c <= '9'))
		;
	if (c != '\n') {
		for (n = c-'0';'0' <= C && c <= '9';)
			n = n*10 + c-'0';
	}
	while(n) {
		if(C == '.') {
			if(C == 'c') {
				if(C == 'e') {
					while(C == ' ')
						;
					if(c == '0') {
						SKIP;
						break;
					} else
						SKIP;
				} else
					SKIP;
			} else
			if(c == 'P' || C == 'P') {
				if(c != '\n')
					SKIP;
				break;
			} else
				if(c != '\n')
					SKIP;
		} else {
			SKIP;
			n--;
		}
	}
}

void
refer(int c1)
{
	int c2;

	if(c1 != '\n')
		SKIP;
	c2 = 0;
	for(;;) {
		if(C != '.')
			SKIP;
		else {
			if(C != ']')
				SKIP;
			else {
				while(C != '\n')
					c2 = c;
				if(charclass(c2) == PUNCT)
					Bprint(&bout, " %C",c2);
				return;
			}
		}
	}
}

void
inpic(void)
{
	int c1;
	Rune *p1;

/*	SKIP1;*/
	while(C1 != '\n')
		if(c == '<'){
			SKIP1;
			return;
		}
	p1 = line;
	c = '\n';
	for(;;) {
		c1 = c;
		if(C1 == '.' && c1 == '\n') {
			if(C1 != 'P' || C1 != 'E') {
				if(c != '\n'){
					SKIP1;
					c = '\n';
				}
				continue;
			}
			SKIP1;
			return;
		} else
		if(c == '\"') {
			while(C1 != '\"') {
				if(c == '\\') {
					if(C1 == '\"')
						continue;
					Bungetrune(infile);
					backsl();
				} else
					*p1++ = c;
			}
			*p1++ = ' ';
		} else
		if(c == '\n' && p1 != line) {
			*p1 = '\0';
			if(wordflag)
				putwords();
			else
				Bprint(&bout, "%S\n\n", line);
			p1 = line;
		}
	}
}

int
charclass(int c)
{
	if(c < MAX_ASCII)
		return chars[c];
	switch(c){
	case 0x2013: case 0x2014:	/* en dash, em dash */
		return SPECIAL;
	}
	return EXTENDED;
}
