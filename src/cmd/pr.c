#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

/*
 *	PR command (print files in pages and columns, with headings)
 *	2+head+2+page[56]+5
 */

#define	ISPRINT(c)	((c) >= ' ')
#define ESC		'\033'
#define LENGTH		66
#define LINEW		72
#define NUMW		5
#define MARGIN		10
#define DEFTAB		8
#define NFILES		10
#define HEAD		"%12.12s %4.4s  %s Page %d\n\n\n", date+4, date+24, head, Page
#define TOLOWER(c)	(isupper(c) ? tolower(c) : c)	/* ouch! */
#define cerror(S)	fprint(2, "pr: %s", S)
#define STDINNAME()	nulls
#define TTY		"/dev/cons", 0
#define PROMPT()	fprint(2, "\a") /* BEL */
#define TABS(N,C)	if((N = intopt(argv, &C)) < 0) N = DEFTAB
#define ETABS		(Inpos % Etabn)
#define ITABS		(Itabn > 0 && Nspace > 1 && Nspace >= (nc = Itabn - Outpos % Itabn))
#define NSEPC		'\t'
#define EMPTY		14	/* length of " -- empty file" */

typedef	struct	Fils	Fils;
typedef	struct	Colp*	Colp;
typedef	struct	Err	Err;

struct	Fils
{
	Biobuf*	f_f;
	char*	f_name;
	long	f_nextc;
};
struct	Colp
{
	Rune*	c_ptr;
	Rune*	c_ptr0;
	long	c_lno;
};
struct	Err
{
	Err*	e_nextp;
	char*	e_mess;
};

int	Balance = 0;
Biobuf	bout;
Rune*	Bufend;
Rune*	Buffer = 0;
int	C = '\0';
Colp	Colpts;
int	Colw;
int	Dblspace = 1;
Err*	err = 0;
int	error = 0;
int	Etabc = '\t';
int	Etabn = 0;
Fils*	Files;
int	Formfeed = 0;
int	Fpage = 1;
char*	Head = 0;
int	Inpos;
int	Itabc = '\t';
int	Itabn = 0;
Err*	Lasterr = (Err*)&err;
int	Lcolpos;
int	Len = LENGTH;
int	Line;
int	Linew = 0;
long	Lnumb = 0;
int	Margin = MARGIN;
int	Multi = 0;
int	Ncols = 1;
int	Nfiles = 0;
int	Nsepc = NSEPC;
int	Nspace;
char	nulls[] = "";
int	Numw;
int	Offset = 0;
int	Outpos;
int	Padodd;
int	Page;
int	Pcolpos;
int	Plength;
int	Sepc = 0;

extern	int	atoix(char**);
extern	void	balance(int);
extern	void	die(char*);
extern	void	errprint(void);
extern	char*	ffiler(char*);
extern	int	findopt(int, char**);
extern	int	get(int);
extern	void*	getspace(ulong);
extern	int	intopt(char**, int*);
extern	void	main(int, char**);
extern	Biobuf*	mustopen(char*, Fils*);
extern	void	nexbuf(void);
extern	int	pr(char*);
extern	void	put(long);
extern	void	putpage(void);
extern	void	putspace(void);

/*
 * return date file was last modified
 */
char*
getdate(void)
{
	static char *now = 0;
	static Dir *sbuf;
	ulong mtime;

	if(Nfiles > 1 || Files->f_name == nulls) {
		if(now == 0) {
			mtime = time(0);
			now = ctime(mtime);
		}
		return now;
	}
	mtime = 0;
	sbuf = dirstat(Files->f_name);
	if(sbuf){
		mtime = sbuf->mtime;
		free(sbuf);
	}
	return ctime(mtime);
}

char*
ffiler(char *s)
{
	return smprint("can't open %s\n", s);
}

void
main(int argc, char *argv[])
{
	Fils fstr[NFILES];
	int nfdone = 0;

	Binit(&bout, 1, OWRITE);
	Files = fstr;
	for(argc = findopt(argc, argv); argc > 0; --argc, ++argv)
		if(Multi == 'm') {
			if(Nfiles >= NFILES - 1)
				die("too many files");
			if(mustopen(*argv, &Files[Nfiles++]) == 0)
				nfdone++; /* suppress printing */
		} else {
			if(pr(*argv))
				Bterm(Files->f_f);
			nfdone++;
		}
	if(!nfdone)			/* no files named, use stdin */
		pr(nulls);		/* on GCOS, use current file, if any */
	errprint();			/* print accumulated error reports */
	exits(error? "error": 0);
}

int
findopt(int argc, char *argv[])
{
	char **eargv = argv;
	int eargc = 0, c;

	while(--argc > 0) {
		switch(c = **++argv) {
		case '-':
			if((c = *++*argv) == '\0')
				break;
		case '+':
			do {
				if(isdigit(c)) {
					--*argv;
					Ncols = atoix(argv);
				} else
				switch(c = TOLOWER(c)) {
				case '+':
					if((Fpage = atoix(argv)) < 1)
						Fpage = 1;
					continue;
				case 'd':
					Dblspace = 2;
					continue;
				case 'e':
					TABS(Etabn, Etabc);
					continue;
				case 'f':
					Formfeed++;
					continue;
				case 'h':
					if(--argc > 0)
						Head = argv[1];
					continue;
				case 'i':
					TABS(Itabn, Itabc);
					continue;
				case 'l':
					Len = atoix(argv);
					continue;
				case 'a':
				case 'm':
					Multi = c;
					continue;
				case 'o':
					Offset = atoix(argv);
					continue;
				case 's':
					if((Sepc = (*argv)[1]) != '\0')
						++*argv;
					else
						Sepc = '\t';
					continue;
				case 't':
					Margin = 0;
					continue;
				case 'w':
					Linew = atoix(argv);
					continue;
				case 'n':
					Lnumb++;
					if((Numw = intopt(argv, &Nsepc)) <= 0)
						Numw = NUMW;
				case 'b':
					Balance = 1;
					continue;
				case 'p':
					Padodd = 1;
					continue;
				default:
					die("bad option");
				}
			} while((c = *++*argv) != '\0');
			if(Head == argv[1])
				argv++;
			continue;
		}
		*eargv++ = *argv;
		eargc++;
	}
	if(Len == 0)
		Len = LENGTH;
	if(Len <= Margin)
		Margin = 0;
	Plength = Len - Margin/2;
	if(Multi == 'm')
		Ncols = eargc;
	switch(Ncols) {
	case 0:
		Ncols = 1;
	case 1:
		break;
	default:
		if(Etabn == 0)		/* respect explicit tab specification */
			Etabn = DEFTAB;
	}
	if(Linew == 0)
		Linew = Ncols != 1 && Sepc == 0? LINEW: 512;
	if(Lnumb)
		Linew -= Multi == 'm'? Numw: Numw*Ncols;
	if((Colw = (Linew - Ncols + 1)/Ncols) < 1)
		die("width too small");
	if(Ncols != 1 && Multi == 0) {
		ulong buflen = ((ulong)(Plength/Dblspace + 1))*(Linew+1)*sizeof(char);
		Buffer = getspace(buflen*sizeof(*Buffer));
		Bufend = &Buffer[buflen];
		Colpts = getspace((Ncols+1)*sizeof(*Colpts));
	}
	return eargc;
}

int
intopt(char *argv[], int *optp)
{
	int c;

	if((c = (*argv)[1]) != '\0' && !isdigit(c)) {
		*optp = c;
		(*argv)++;
	}
	c = atoix(argv);
	return c != 0? c: -1;
}

int
pr(char *name)
{
	char *date = 0, *head = 0;

	if(Multi != 'm' && mustopen(name, &Files[0]) == 0)
		return 0;
	if(Buffer)
		Bungetc(Files->f_f);
	if(Lnumb)
		Lnumb = 1;
	for(Page = 0;; putpage()) {
		if(C == -1)
			break;
		if(Buffer)
			nexbuf();
		Inpos = 0;
		if(get(0) == -1)
			break;
		Bflush(&bout);
		Page++;
		if(Page >= Fpage) {
			if(Margin == 0)
				continue;
			if(date == 0)
				date = getdate();
			if(head == 0)
				head = Head != 0 ? Head :
					Nfiles < 2? Files->f_name: nulls;
			Bprint(&bout, "\n\n");
			Nspace = Offset;
			putspace();
			Bprint(&bout, HEAD);
		}
	}
	if(Padodd && (Page&1) == 1) {
		Line = 0;
		if(Formfeed)
			put('\f');
		else
			while(Line < Len)
				put('\n');
	}
	C = '\0';
	return 1;
}

void
putpage(void)
{
	int colno;

	for(Line = Margin/2;; get(0)) {
		for(Nspace = Offset, colno = 0, Outpos = 0; C != '\f';) {
			if(Lnumb && C != -1 && (colno == 0 || Multi == 'a')) {
				if(Page >= Fpage) {
					putspace();
					Bprint(&bout, "%*ld", Numw, Buffer?
						Colpts[colno].c_lno++: Lnumb);
					Outpos += Numw;
					put(Nsepc);
				}
				Lnumb++;
			}
			for(Lcolpos=0, Pcolpos=0; C!='\n' && C!='\f' && C!=-1; get(colno))
					put(C);
			if(C==-1 || ++colno==Ncols || C=='\n' && get(colno)==-1)
				break;
			if(Sepc)
				put(Sepc);
			else
				if((Nspace += Colw - Lcolpos + 1) < 1)
					Nspace = 1;
		}
	/*
		if(C == -1) {
			if(Margin != 0)
				break;
			if(colno != 0)
				put('\n');
			return;
		}
	*/
		if(C == -1 && colno == 0) {
			if(Margin != 0)
				break;
			return;
		}
		if(C == '\f')
			break;
		put('\n');
		if(Dblspace == 2 && Line < Plength)
			put('\n');
		if(Line >= Plength)
			break;
	}
	if(Formfeed)
		put('\f');
	else
		while(Line < Len)
			put('\n');
}

void
nexbuf(void)
{
	Rune *s = Buffer;
	Colp p = Colpts;
	int j, c, bline = 0;

	for(;;) {
		p->c_ptr0 = p->c_ptr = s;
		if(p == &Colpts[Ncols])
			return;
		(p++)->c_lno = Lnumb + bline;
		for(j = (Len - Margin)/Dblspace; --j >= 0; bline++)
			for(Inpos = 0;;) {
				if((c = Bgetrune(Files->f_f)) == -1) {
					for(*s = -1; p <= &Colpts[Ncols]; p++)
						p->c_ptr0 = p->c_ptr = s;
					if(Balance)
						balance(bline);
					return;
				}
				if(ISPRINT(c))
					Inpos++;
				if(Inpos <= Colw || c == '\n') {
					*s = c;
					if(++s >= Bufend)
						die("page-buffer overflow");
				}
				if(c == '\n')
					break;
				switch(c) {
				case '\b':
					if(Inpos == 0)
						s--;
				case ESC:
					if(Inpos > 0)
						Inpos--;
				}
			}
	}
}

/*
 * line balancing for last page
 */
void
balance(int bline)
{
	Rune *s = Buffer;
	Colp p = Colpts;
	int colno = 0, j, c, l;

	c = bline % Ncols;
	l = (bline + Ncols - 1)/Ncols;
	bline = 0;
	do {
		for(j = 0; j < l; ++j)
			while(*s++ != '\n')
				;
		(++p)->c_lno = Lnumb + (bline += l);
		p->c_ptr0 = p->c_ptr = s;
		if(++colno == c)
			l--;
	} while(colno < Ncols - 1);
}

int
get(int colno)
{
	static int peekc = 0;
	Colp p;
	Fils *q;
	long c;

	if(peekc) {
		peekc = 0;
		c = Etabc;
	} else
	if(Buffer) {
		p = &Colpts[colno];
		if(p->c_ptr >= (p+1)->c_ptr0)
			c = -1;
		else
			if((c = *p->c_ptr) != -1)
				p->c_ptr++;
	} else
	if((c = (q = &Files[Multi == 'a'? 0: colno])->f_nextc) == -1) {
		for(q = &Files[Nfiles]; --q >= Files && q->f_nextc == -1;)
			;
		if(q >= Files)
			c = '\n';
	} else
		q->f_nextc = Bgetrune(q->f_f);
	if(Etabn != 0 && c == Etabc) {
		Inpos++;
		peekc = ETABS;
		c = ' ';
	} else
	if(ISPRINT(c))
		Inpos++;
	else
		switch(c) {
		case '\b':
		case ESC:
			if(Inpos > 0)
				Inpos--;
			break;
		case '\f':
			if(Ncols == 1)
				break;
			c = '\n';
		case '\n':
		case '\r':
			Inpos = 0;
		}
	return C = c;
}

void
put(long c)
{
	int move;

	switch(c) {
	case ' ':
		Nspace++;
		Lcolpos++;
		return;
	case '\b':
		if(Lcolpos == 0)
			return;
		if(Nspace > 0) {
			Nspace--;
			Lcolpos--;
			return;
		}
		if(Lcolpos > Pcolpos) {
			Lcolpos--;
			return;
		}
	case ESC:
		move = -1;
		break;
	case '\n':
		Line++;
	case '\r':
	case '\f':
		Pcolpos = 0;
		Lcolpos = 0;
		Nspace = 0;
		Outpos = 0;
	default:
		move = (ISPRINT(c) != 0);
	}
	if(Page < Fpage)
		return;
	if(Lcolpos > 0 || move > 0)
		Lcolpos += move;
	if(Lcolpos <= Colw) {
		putspace();
		Bputrune(&bout, c);
		Pcolpos = Lcolpos;
		Outpos += move;
	}
}

void
putspace(void)
{
	int nc;

	for(; Nspace > 0; Outpos += nc, Nspace -= nc)
		if(ITABS)
			Bputc(&bout, Itabc);
		else {
			nc = 1;
			Bputc(&bout, ' ');
		}
}

int
atoix(char **p)
{
	int n = 0, c;

	while(isdigit(c = *++*p))
		n = 10*n + c - '0';
	(*p)--;
	return n;
}

/*
 * Defer message about failure to open file to prevent messing up
 * alignment of page with tear perforations or form markers.
 * Treat empty file as special case and report as diagnostic.
 */
Biobuf*
mustopen(char *s, Fils *f)
{
	char *tmp;

	if(*s == '\0') {
		f->f_name = STDINNAME();
		f->f_f = malloc(sizeof(Biobuf));
		if(f->f_f == 0)
			cerror("no memory");
		Binit(f->f_f, 0, OREAD);
	} else
	if((f->f_f = Bopen(f->f_name = s, OREAD)) == 0) {
		tmp = ffiler(f->f_name);
		s = strcpy((char*)getspace(strlen(tmp) + 1), tmp);
		free(tmp);
	}
	if(f->f_f != 0) {
		if((f->f_nextc = Bgetrune(f->f_f)) >= 0 || Multi == 'm')
			return f->f_f;
		sprint(s = (char*)getspace(strlen(f->f_name) + 1 + EMPTY),
			"%s -- empty file\n", f->f_name);
		Bterm(f->f_f);
	}
	error = 1;
	cerror(s);
	fprint(2, "\n");
	return 0;
}

void*
getspace(ulong n)
{
	void *t;

	if((t = malloc(n)) == 0)
		die("out of space");
	return t;
}

void
die(char *s)
{
	error++;
	errprint();
	cerror(s);
	Bputc(&bout, '\n');
	exits("error");
}

/*
void
onintr(void)
{
	error++;
	errprint();
	exits("error");
}
/**/

/*
 * print accumulated error reports
 */
void
errprint(void)
{
	Bflush(&bout);
	for(; err != 0; err = err->e_nextp) {
		cerror(err->e_mess);
		fprint(2, "\n");
	}
}
