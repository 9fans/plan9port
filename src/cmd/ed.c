/*
 * Editor
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>

#undef EOF	/* stdio? */

enum
{
	FNSIZE	= 128,		/* file name */
	LBSIZE	= 4096,		/* max line size */
	BLKSIZE	= 4096,		/* block size in temp file */
	NBLK	= 32767,	/* max size of temp file */
	ESIZE	= 256,		/* max size of reg exp */
	GBSIZE	= 256,		/* max size of global command */
	MAXSUB	= 9,		/* max number of sub reg exp */
	ESCFLG	= 0xFFFF,	/* escape Rune - user defined code */
	EOF	= -1
};

enum
{
	LINELEN = 70,	/* max number of glyphs in a display line */
	BELL = 6	/* A char could require up to BELL glyphs to display */
};

void	(*oldhup)(int);
void	(*oldquit)(int);
int*	addr1;
int*	addr2;
int	anymarks;
int	col;
long	count;
int*	dol;
int*	dot;
int	fchange;
char	file[FNSIZE];
Rune	genbuf[LBSIZE];
int	given;
Rune*	globp;
int	iblock;
int	ichanged;
int	io;
Biobuf	iobuf;
int	lastc;
char	line[LINELEN];
Rune*	linebp;
Rune	linebuf[LBSIZE];
int	listf;
int	listn;
Rune*	loc1;
Rune*	loc2;
int	names[26];
int	nleft;
int	oblock;
int	oflag;
Reprog	*pattern;
int	peekc;
int	pflag;
int	rescuing;
Rune	rhsbuf[LBSIZE/sizeof(Rune)];
char	savedfile[FNSIZE];
jmp_buf	savej;
int	subnewa;
int	subolda;
Resub	subexp[MAXSUB];
char*	tfname;
int	tline;
int	waiting;
int	wrapp;
int*	zero;

char	Q[]	= "";
char	T[]	= "TMP";
char	WRERR[]	= "WRITE ERROR";
int	bpagesize = 20;
char	hex[]	= "0123456789abcdef";
char*	linp	= line;
ulong	nlall = 128;
int	tfile	= -1;
int	vflag	= 1;

void	add(int);
int*	address(void);
int	append(int(*)(void), int*);
void	browse(void);
void	callunix(void);
void	commands(void);
void	compile(int);
int	compsub(void);
void	dosub(void);
void	error(char*);
int	match(int*);
void	exfile(int);
void	filename(int);
Rune*	getblock(int, int);
int	getchr(void);
int	getcopy(void);
int	getfile(void);
Rune*	getline(int);
int	getnum(void);
int	getsub(void);
int	gettty(void);
void	global(int);
void	init(void);
void	join(void);
void	move(int);
void	newline(void);
void	nonzero(void);
void	notifyf(void*, char*);
Rune*	place(Rune*, Rune*, Rune*);
void	printcom(void);
void	putchr(int);
void	putd(void);
void	putfile(void);
int	putline(void);
void	putshst(Rune*);
void	putst(char*);
void	quit(void);
void	rdelete(int*, int*);
void	regerror(char *);
void	reverse(int*, int*);
void	setnoaddr(void);
void	setwide(void);
void	squeeze(int);
void	substitute(int);

Rune La[] = { 'a', 0 };
Rune Lr[] = { 'r', 0 };

char tmp[] = "/var/tmp/eXXXXX";

void
main(int argc, char *argv[])
{
	char *p1, *p2;

	notify(notifyf);
	ARGBEGIN {
	case 'o':
		oflag = 1;
		vflag = 0;
		break;
	} ARGEND

	USED(argc);
	if(*argv && (strcmp(*argv, "-") == 0)) {
		argv++;
		vflag = 0;
	}
	if(oflag) {
		p1 = "/dev/stdout";
		p2 = savedfile;
		while(*p2++ = *p1++)
			;
		globp = La;
	} else
	if(*argv) {
		p1 = *argv;
		p2 = savedfile;
		while(*p2++ = *p1++)
			if(p2 >= &savedfile[sizeof(savedfile)])
				p2--;
		globp = Lr;
	}
	zero = malloc((nlall+5)*sizeof(int*));
	tfname = mktemp(tmp);
	init();
	setjmp(savej);
	commands();
	quit();
}

void
commands(void)
{
	int *a1, c, temp;
	char lastsep;
	Dir *d;

	for(;;) {
		if(pflag) {
			pflag = 0;
			addr1 = addr2 = dot;
			printcom();
		}
		c = '\n';
		for(addr1 = 0;;) {
			lastsep = c;
			a1 = address();
			c = getchr();
			if(c != ',' && c != ';')
				break;
			if(lastsep == ',')
				error(Q);
			if(a1 == 0) {
				a1 = zero+1;
				if(a1 > dol)
					a1--;
			}
			addr1 = a1;
			if(c == ';')
				dot = a1;
		}
		if(lastsep != '\n' && a1 == 0)
			a1 = dol;
		if((addr2=a1) == 0) {
			given = 0;
			addr2 = dot;
		} else
			given = 1;
		if(addr1 == 0)
			addr1 = addr2;
		switch(c) {

		case 'a':
			add(0);
			continue;

		case 'b':
			nonzero();
			browse();
			continue;

		case 'c':
			nonzero();
			newline();
			rdelete(addr1, addr2);
			append(gettty, addr1-1);
			continue;

		case 'd':
			nonzero();
			newline();
			rdelete(addr1, addr2);
			continue;

		case 'E':
			fchange = 0;
			c = 'e';
		case 'e':
			setnoaddr();
			if(vflag && fchange) {
				fchange = 0;
				error(Q);
			}
			filename(c);
			init();
			addr2 = zero;
			goto caseread;

		case 'f':
			setnoaddr();
			filename(c);
			putst(savedfile);
			continue;

		case 'g':
			global(1);
			continue;

		case 'i':
			add(-1);
			continue;


		case 'j':
			if(!given)
				addr2++;
			newline();
			join();
			continue;

		case 'k':
			nonzero();
			c = getchr();
			if(c < 'a' || c > 'z')
				error(Q);
			newline();
			names[c-'a'] = *addr2 & ~01;
			anymarks |= 01;
			continue;

		case 'm':
			move(0);
			continue;

		case 'n':
			listn++;
			newline();
			printcom();
			continue;

		case '\n':
			if(a1==0) {
				a1 = dot+1;
				addr2 = a1;
				addr1 = a1;
			}
			if(lastsep==';')
				addr1 = a1;
			printcom();
			continue;

		case 'l':
			listf++;
		case 'p':
		case 'P':
			newline();
			printcom();
			continue;

		case 'Q':
			fchange = 0;
		case 'q':
			setnoaddr();
			newline();
			quit();

		case 'r':
			filename(c);
		caseread:
			if((io=open(file, OREAD)) < 0) {
				lastc = '\n';
				error(file);
			}
			if((d = dirfstat(io)) != nil){
				if(d->mode & DMAPPEND)
					print("warning: %s is append only\n", file);
				free(d);
			}
			Binit(&iobuf, io, OREAD);
			setwide();
			squeeze(0);
			c = zero != dol;
			append(getfile, addr2);
			exfile(OREAD);

			fchange = c;
			continue;

		case 's':
			nonzero();
			substitute(globp != 0);
			continue;

		case 't':
			move(1);
			continue;

		case 'u':
			nonzero();
			newline();
			if((*addr2&~01) != subnewa)
				error(Q);
			*addr2 = subolda;
			dot = addr2;
			continue;

		case 'v':
			global(0);
			continue;

		case 'W':
			wrapp++;
		case 'w':
			setwide();
			squeeze(dol>zero);
			temp = getchr();
			if(temp != 'q' && temp != 'Q') {
				peekc = temp;
				temp = 0;
			}
			filename(c);
			if(!wrapp ||
			  ((io = open(file, OWRITE)) == -1) ||
			  ((seek(io, 0L, 2)) == -1))
				if((io = create(file, OWRITE, 0666)) < 0)
					error(file);
			Binit(&iobuf, io, OWRITE);
			wrapp = 0;
			if(dol > zero)
				putfile();
			exfile(OWRITE);
			if(addr1<=zero+1 && addr2==dol)
				fchange = 0;
			if(temp == 'Q')
				fchange = 0;
			if(temp)
				quit();
			continue;

		case '=':
			setwide();
			squeeze(0);
			newline();
			count = addr2 - zero;
			putd();
			putchr('\n');
			continue;

		case '!':
			callunix();
			continue;

		case EOF:
			return;

		}
		error(Q);
	}
}

void
printcom(void)
{
	int *a1;

	nonzero();
	a1 = addr1;
	do {
		if(listn) {
			count = a1-zero;
			putd();
			putchr('\t');
		}
		putshst(getline(*a1++));
	} while(a1 <= addr2);
	dot = addr2;
	listf = 0;
	listn = 0;
	pflag = 0;
}

int*
address(void)
{
	int sign, *a, opcnt, nextopand, *b, c;

	nextopand = -1;
	sign = 1;
	opcnt = 0;
	a = dot;
	do {
		do {
			c = getchr();
		} while(c == ' ' || c == '\t');
		if(c >= '0' && c <= '9') {
			peekc = c;
			if(!opcnt)
				a = zero;
			a += sign*getnum();
		} else
		switch(c) {
		case '$':
			a = dol;
		case '.':
			if(opcnt)
				error(Q);
			break;
		case '\'':
			c = getchr();
			if(opcnt || c < 'a' || c > 'z')
				error(Q);
			a = zero;
			do {
				a++;
			} while(a <= dol && names[c-'a'] != (*a & ~01));
			break;
		case '?':
			sign = -sign;
		case '/':
			compile(c);
			b = a;
			for(;;) {
				a += sign;
				if(a <= zero)
					a = dol;
				if(a > dol)
					a = zero;
				if(match(a))
					break;
				if(a == b)
					error(Q);
			}
			break;
		default:
			if(nextopand == opcnt) {
				a += sign;
				if(a < zero || dol < a)
					continue;       /* error(Q); */
			}
			if(c != '+' && c != '-' && c != '^') {
				peekc = c;
				if(opcnt == 0)
					a = 0;
				return a;
			}
			sign = 1;
			if(c != '+')
				sign = -sign;
			nextopand = ++opcnt;
			continue;
		}
		sign = 1;
		opcnt++;
	} while(zero <= a && a <= dol);
	error(Q);
	return 0;
}

int
getnum(void)
{
	int r, c;

	r = 0;
	for(;;) {
		c = getchr();
		if(c < '0' || c > '9')
			break;
		r = r*10 + (c-'0');
	}
	peekc = c;
	return r;
}

void
setwide(void)
{
	if(!given) {
		addr1 = zero + (dol>zero);
		addr2 = dol;
	}
}

void
setnoaddr(void)
{
	if(given)
		error(Q);
}

void
nonzero(void)
{
	squeeze(1);
}

void
squeeze(int i)
{
	if(addr1 < zero+i || addr2 > dol || addr1 > addr2)
		error(Q);
}

void
newline(void)
{
	int c;

	c = getchr();
	if(c == '\n' || c == EOF)
		return;
	if(c == 'p' || c == 'l' || c == 'n') {
		pflag++;
		if(c == 'l')
			listf++;
		else
		if(c == 'n')
			listn++;
		c = getchr();
		if(c == '\n')
			return;
	}
	error(Q);
}

void
filename(int comm)
{
	char *p1, *p2;
	Rune rune;
	int c;

	count = 0;
	c = getchr();
	if(c == '\n' || c == EOF) {
		p1 = savedfile;
		if(*p1 == 0 && comm != 'f')
			error(Q);
		p2 = file;
		while(*p2++ = *p1++)
			;
		return;
	}
	if(c != ' ')
		error(Q);
	while((c=getchr()) == ' ')
		;
	if(c == '\n')
		error(Q);
	p1 = file;
	do {
		if(p1 >= &file[sizeof(file)-6] || c == ' ' || c == EOF)
			error(Q);
		rune = c;
		p1 += runetochar(p1, &rune);
	} while((c=getchr()) != '\n');
	*p1 = 0;
	if(savedfile[0] == 0 || comm == 'e' || comm == 'f') {
		p1 = savedfile;
		p2 = file;
		while(*p1++ = *p2++)
			;
	}
}

void
exfile(int om)
{

	if(om == OWRITE)
		if(Bflush(&iobuf) < 0)
			error(Q);
	close(io);
	io = -1;
	if(vflag) {
		putd();
		putchr('\n');
	}
}

void
error1(char *s)
{
	int c;

	wrapp = 0;
	listf = 0;
	listn = 0;
	count = 0;
	seek(0, 0, 2);
	pflag = 0;
	if(globp)
		lastc = '\n';
	globp = 0;
	peekc = lastc;
	if(lastc)
		for(;;) {
			c = getchr();
			if(c == '\n' || c == EOF)
				break;
		}
	if(io > 0) {
		close(io);
		io = -1;
	}
	putchr('?');
	putst(s);
}

void
error(char *s)
{
	error1(s);
	longjmp(savej, 1);
}

void
rescue(void)
{
	rescuing = 1;
	if(dol > zero) {
		addr1 = zero+1;
		addr2 = dol;
		io = create("ed.hup", OWRITE, 0666);
		if(io > 0){
			Binit(&iobuf, io, OWRITE);
			putfile();
		}
	}
	fchange = 0;
	quit();
}

void
notifyf(void *a, char *s)
{
	if(strcmp(s, "interrupt") == 0){
		if(rescuing || waiting)
			noted(NCONT);
		putchr('\n');
		lastc = '\n';
		error1(Q);
		notejmp(a, savej, 0);
	}
	if(strcmp(s, "hangup") == 0 || strcmp(s, "kill") == 0){
		if(rescuing)
			noted(NDFLT);
		rescue();
	}
	if(strstr(s, "child"))
		noted(NCONT);
	fprint(2, "ed: note: %s\n", s);
	abort();
}

int
getchr(void)
{
	char s[UTFmax];
	int i;
	Rune r;

	if(lastc = peekc) {
		peekc = 0;
		return lastc;
	}
	if(globp) {
		if((lastc=*globp++) != 0)
			return lastc;
		globp = 0;
		return EOF;
	}
	for(i=0;;) {
		if(read(0, s+i, 1) <= 0)
			return lastc = EOF;
		i++;
		if(fullrune(s, i))
			break;

	}
	chartorune(&r, s);
	lastc = r;
	return lastc;
}

int
gety(void)
{
	int c;
	Rune *gf, *p;

	p = linebuf;
	gf = globp;
	for(;;) {
		c = getchr();
		if(c == '\n') {
			*p = 0;
			return 0;
		}
		if(c == EOF) {
			if(gf)
				peekc = c;
			return c;
		}
		if(c == 0)
			continue;
		*p++ = c;
		if(p >= &linebuf[LBSIZE-2])
			error(Q);
	}
}

int
gettty(void)
{
	int rc;

	rc = gety();
	if(rc)
		return rc;
	if(linebuf[0] == '.' && linebuf[1] == 0)
		return EOF;
	return 0;
}

int
getfile(void)
{
	int c;
	Rune *lp;

	lp = linebuf;
	do {
		c = Bgetrune(&iobuf);
		if(c < 0) {
			if(lp > linebuf) {
				putst("'\\n' appended");
				c = '\n';
			} else
				return EOF;
		}
		if(lp >= &linebuf[LBSIZE]) {
			lastc = '\n';
			error(Q);
		}
		*lp++ = c;
		count++;
	} while(c != '\n');
	lp[-1] = 0;
	return 0;
}

void
putfile(void)
{
	int *a1;
	Rune *lp;
	long c;

	a1 = addr1;
	do {
		lp = getline(*a1++);
		for(;;) {
			count++;
			c = *lp++;
			if(c == 0) {
				if(Bputrune(&iobuf, '\n') < 0)
					error(Q);
				break;
			}
			if(Bputrune(&iobuf, c) < 0)
				error(Q);
		}
	} while(a1 <= addr2);
	if(Bflush(&iobuf) < 0)
		error(Q);
}

int
append(int (*f)(void), int *a)
{
	int *a1, *a2, *rdot, nline, d;

	nline = 0;
	dot = a;
	while((*f)() == 0) {
		if((dol-zero) >= nlall) {
			nlall += 512;
			a1 = realloc(zero, (nlall+50)*sizeof(int*));
			if(a1 == 0) {
				error("MEM?");
				rescue();
			}
			/* relocate pointers; avoid wraparound if sizeof(int) < sizeof(int*) */
			d = addr1 - zero;
			addr1 = a1 + d;
			d = addr2 - zero;
			addr2 = a1 + d;
			d = dol - zero;
			dol = a1 + d;
			d = dot - zero;
			dot = a1 + d;
			zero = a1;
		}
		d = putline();
		nline++;
		a1 = ++dol;
		a2 = a1+1;
		rdot = ++dot;
		while(a1 > rdot)
			*--a2 = *--a1;
		*rdot = d;
	}
	return nline;
}

void
add(int i)
{
	if(i && (given || dol > zero)) {
		addr1--;
		addr2--;
	}
	squeeze(0);
	newline();
	append(gettty, addr2);
}

void
browse(void)
{
	int forward, n;
	static int bformat, bnum; /* 0 */

	forward = 1;
	peekc = getchr();
	if(peekc != '\n'){
		if(peekc == '-' || peekc == '+') {
			if(peekc == '-')
				forward = 0;
			getchr();
		}
		n = getnum();
		if(n > 0)
			bpagesize = n;
	}
	newline();
	if(pflag) {
		bformat = listf;
		bnum = listn;
	} else {
		listf = bformat;
		listn = bnum;
	}
	if(forward) {
		addr1 = addr2;
		addr2 += bpagesize;
		if(addr2 > dol)
			addr2 = dol;
	} else {
		addr1 = addr2-bpagesize;
		if(addr1 <= zero)
			addr1 = zero+1;
	}
	printcom();
}

void
callunix(void)
{
	int c, pid;
	Rune rune;
	char buf[512];
	char *p;

	setnoaddr();
	p = buf;
	while((c=getchr()) != EOF && c != '\n')
		if(p < &buf[sizeof(buf) - 6]) {
			rune = c;
			p += runetochar(p, &rune);
		}
	*p = 0;
	pid = fork();
	if(pid == 0) {
		execlp("rc", "rc", "-c", buf, (char*)0);
		sysfatal("exec failed: %r");
		exits("execl failed");
	}
	waiting = 1;
	while(waitpid() != pid)
		;
	waiting = 0;
	if(vflag)
		putst("!");
}

void
quit(void)
{
	if(vflag && fchange && dol!=zero) {
		fchange = 0;
		error(Q);
	}
	remove(tfname);
	exits(0);
}

void
onquit(int sig)
{
	USED(sig);
	quit();
}

void
rdelete(int *ad1, int *ad2)
{
	int *a1, *a2, *a3;

	a1 = ad1;
	a2 = ad2+1;
	a3 = dol;
	dol -= a2 - a1;
	do {
		*a1++ = *a2++;
	} while(a2 <= a3);
	a1 = ad1;
	if(a1 > dol)
		a1 = dol;
	dot = a1;
	fchange = 1;
}

void
gdelete(void)
{
	int *a1, *a2, *a3;

	a3 = dol;
	for(a1=zero; (*a1&01)==0; a1++)
		if(a1>=a3)
			return;
	for(a2=a1+1; a2<=a3;) {
		if(*a2 & 01) {
			a2++;
			dot = a1;
		} else
			*a1++ = *a2++;
	}
	dol = a1-1;
	if(dot > dol)
		dot = dol;
	fchange = 1;
}

Rune*
getline(int tl)
{
	Rune *lp, *bp;
	int nl;

	lp = linebuf;
	bp = getblock(tl, OREAD);
	nl = nleft;
	tl &= ~((BLKSIZE/sizeof(Rune)) - 1);
	while(*lp++ = *bp++) {
		nl -= sizeof(Rune);
		if(nl == 0) {
			bp = getblock(tl += BLKSIZE/sizeof(Rune), OREAD);
			nl = nleft;
		}
	}
	return linebuf;
}

int
putline(void)
{
	Rune *lp, *bp;
	int nl, tl;

	fchange = 1;
	lp = linebuf;
	tl = tline;
	bp = getblock(tl, OWRITE);
	nl = nleft;
	tl &= ~((BLKSIZE/sizeof(Rune))-1);
	while(*bp = *lp++) {
		if(*bp++ == '\n') {
			bp[-1] = 0;
			linebp = lp;
			break;
		}
		nl -= sizeof(Rune);
		if(nl == 0) {
			tl += BLKSIZE/sizeof(Rune);
			bp = getblock(tl, OWRITE);
			nl = nleft;
		}
	}
	nl = tline;
	tline += ((lp-linebuf) + 03) & (NBLK-1);
	return nl;
}

void
blkio(int b, uchar *buf, int isread)
{
	int n;

	seek(tfile, b*BLKSIZE, 0);
	if(isread)
		n = read(tfile, buf, BLKSIZE);
	else
		n = write(tfile, buf, BLKSIZE);
	if(n != BLKSIZE)
		error(T);
}

Rune*
getblock(int atl, int iof)
{
	int bno, off;

	static uchar ibuff[BLKSIZE];
	static uchar obuff[BLKSIZE];

	bno = atl / (BLKSIZE/sizeof(Rune));
	off = (atl*sizeof(Rune)) & (BLKSIZE-1) & ~03;
	if(bno >= NBLK) {
		lastc = '\n';
		error(T);
	}
	nleft = BLKSIZE - off;
	if(bno == iblock) {
		ichanged |= iof;
		return (Rune*)(ibuff+off);
	}
	if(bno == oblock)
		return (Rune*)(obuff+off);
	if(iof == OREAD) {
		if(ichanged)
			blkio(iblock, ibuff, 0);
		ichanged = 0;
		iblock = bno;
		blkio(bno, ibuff, 1);
		return (Rune*)(ibuff+off);
	}
	if(oblock >= 0)
		blkio(oblock, obuff, 0);
	oblock = bno;
	return (Rune*)(obuff+off);
}

void
init(void)
{
	int *markp;

	close(tfile);
	tline = 2;
	for(markp = names; markp < &names[26]; )
		*markp++ = 0;
	subnewa = 0;
	anymarks = 0;
	iblock = -1;
	oblock = -1;
	ichanged = 0;
	if((tfile = create(tfname, ORDWR, 0600)) < 0){
		error1(T);
		exits(0);
	}
	dot = dol = zero;
}

void
global(int k)
{
	Rune *gp, globuf[GBSIZE];
	int c, *a1;

	if(globp)
		error(Q);
	setwide();
	squeeze(dol > zero);
	c = getchr();
	if(c == '\n')
		error(Q);
	compile(c);
	gp = globuf;
	while((c=getchr()) != '\n') {
		if(c == EOF)
			error(Q);
		if(c == '\\') {
			c = getchr();
			if(c != '\n')
				*gp++ = '\\';
		}
		*gp++ = c;
		if(gp >= &globuf[GBSIZE-2])
			error(Q);
	}
	if(gp == globuf)
		*gp++ = 'p';
	*gp++ = '\n';
	*gp = 0;
	for(a1=zero; a1<=dol; a1++) {
		*a1 &= ~01;
		if(a1 >= addr1 && a1 <= addr2 && match(a1) == k)
			*a1 |= 01;
	}

	/*
	 * Special case: g/.../d (avoid n^2 algorithm)
	 */
	if(globuf[0] == 'd' && globuf[1] == '\n' && globuf[2] == 0) {
		gdelete();
		return;
	}
	for(a1=zero; a1<=dol; a1++) {
		if(*a1 & 01) {
			*a1 &= ~01;
			dot = a1;
			globp = globuf;
			commands();
			a1 = zero;
		}
	}
}

void
join(void)
{
	Rune *gp, *lp;
	int *a1;

	nonzero();
	gp = genbuf;
	for(a1=addr1; a1<=addr2; a1++) {
		lp = getline(*a1);
		while(*gp = *lp++)
			if(gp++ >= &genbuf[LBSIZE-2])
				error(Q);
	}
	lp = linebuf;
	gp = genbuf;
	while(*lp++ = *gp++)
		;
	*addr1 = putline();
	if(addr1 < addr2)
		rdelete(addr1+1, addr2);
	dot = addr1;
}

void
substitute(int inglob)
{
	int *mp, *a1, nl, gsubf, n;

	n = getnum();	/* OK even if n==0 */
	gsubf = compsub();
	for(a1 = addr1; a1 <= addr2; a1++) {
		if(match(a1)){
			int *ozero;
			int m = n;

			do {
				int span = loc2-loc1;

				if(--m <= 0) {
					dosub();
					if(!gsubf)
						break;
					if(span == 0) {	/* null RE match */
						if(*loc2 == 0)
							break;
						loc2++;
					}
				}
			} while(match(0));
			if(m <= 0) {
				inglob |= 01;
				subnewa = putline();
				*a1 &= ~01;
				if(anymarks) {
					for(mp=names; mp<&names[26]; mp++)
						if(*mp == *a1)
							*mp = subnewa;
				}
				subolda = *a1;
				*a1 = subnewa;
				ozero = zero;
				nl = append(getsub, a1);
				addr2 += nl;
				nl += zero-ozero;
				a1 += nl;
			}
		}
	}
	if(inglob == 0)
		error(Q);
}

int
compsub(void)
{
	int seof, c;
	Rune *p;

	seof = getchr();
	if(seof == '\n' || seof == ' ')
		error(Q);
	compile(seof);
	p = rhsbuf;
	for(;;) {
		c = getchr();
		if(c == '\\') {
			c = getchr();
			*p++ = ESCFLG;
			if(p >= &rhsbuf[LBSIZE/sizeof(Rune)])
				error(Q);
		} else
		if(c == '\n' && (!globp || !globp[0])) {
			peekc = c;
			pflag++;
			break;
		} else
		if(c == seof)
			break;
		*p++ = c;
		if(p >= &rhsbuf[LBSIZE/sizeof(Rune)])
			error(Q);
	}
	*p = 0;
	peekc = getchr();
	if(peekc == 'g') {
		peekc = 0;
		newline();
		return 1;
	}
	newline();
	return 0;
}

int
getsub(void)
{
	Rune *p1, *p2;

	p1 = linebuf;
	if((p2 = linebp) == 0)
		return EOF;
	while(*p1++ = *p2++)
		;
	linebp = 0;
	return 0;
}

void
dosub(void)
{
	Rune *lp, *sp, *rp;
	int c, n;

	lp = linebuf;
	sp = genbuf;
	rp = rhsbuf;
	while(lp < loc1)
		*sp++ = *lp++;
	while(c = *rp++) {
		if(c == '&'){
			sp = place(sp, loc1, loc2);
			continue;
		}
		if(c == ESCFLG && (c = *rp++) >= '1' && c < MAXSUB+'0') {
			n = c-'0';
			if(subexp[n].s.rsp && subexp[n].e.rep) {
				sp = place(sp, subexp[n].s.rsp, subexp[n].e.rep);
				continue;
			}
			error(Q);
		}
		*sp++ = c;
		if(sp >= &genbuf[LBSIZE])
			error(Q);
	}
	lp = loc2;
	loc2 = sp - genbuf + linebuf;
	while(*sp++ = *lp++)
		if(sp >= &genbuf[LBSIZE])
			error(Q);
	lp = linebuf;
	sp = genbuf;
	while(*lp++ = *sp++)
		;
}

Rune*
place(Rune *sp, Rune *l1, Rune *l2)
{

	while(l1 < l2) {
		*sp++ = *l1++;
		if(sp >= &genbuf[LBSIZE])
			error(Q);
	}
	return sp;
}

void
move(int cflag)
{
	int *adt, *ad1, *ad2;

	nonzero();
	if((adt = address())==0)	/* address() guarantees addr is in range */
		error(Q);
	newline();
	if(cflag) {
		int *ozero, delta;
		ad1 = dol;
		ozero = zero;
		append(getcopy, ad1++);
		ad2 = dol;
		delta = zero - ozero;
		ad1 += delta;
		adt += delta;
	} else {
		ad2 = addr2;
		for(ad1 = addr1; ad1 <= ad2;)
			*ad1++ &= ~01;
		ad1 = addr1;
	}
	ad2++;
	if(adt<ad1) {
		dot = adt + (ad2-ad1);
		if((++adt)==ad1)
			return;
		reverse(adt, ad1);
		reverse(ad1, ad2);
		reverse(adt, ad2);
	} else
	if(adt >= ad2) {
		dot = adt++;
		reverse(ad1, ad2);
		reverse(ad2, adt);
		reverse(ad1, adt);
	} else
		error(Q);
	fchange = 1;
}

void
reverse(int *a1, int *a2)
{
	int t;

	for(;;) {
		t = *--a2;
		if(a2 <= a1)
			return;
		*a2 = *a1;
		*a1++ = t;
	}
}

int
getcopy(void)
{
	if(addr1 > addr2)
		return EOF;
	getline(*addr1++);
	return 0;
}

void
compile(int eof)
{
	Rune c;
	char *ep;
	char expbuf[ESIZE];

	if((c = getchr()) == '\n') {
		peekc = c;
		c = eof;
	}
	if(c == eof) {
		if(!pattern)
			error(Q);
		return;
	}
	if(pattern) {
		free(pattern);
		pattern = 0;
	}
	ep = expbuf;
	do {
		if(c == '\\') {
			if(ep >= expbuf+sizeof(expbuf)) {
				error(Q);
				return;
			}
			ep += runetochar(ep, &c);
			if((c = getchr()) == '\n') {
				error(Q);
				return;
			}
		}
		if(ep >= expbuf+sizeof(expbuf)) {
			error(Q);
			return;
		}
		ep += runetochar(ep, &c);
	} while((c = getchr()) != eof && c != '\n');
	if(c == '\n')
		peekc = c;
	*ep = 0;
	pattern = regcomp(expbuf);
}

int
match(int *addr)
{
	if(!pattern)
		return 0;
	if(addr){
		if(addr == zero)
			return 0;
		subexp[0].s.rsp = getline(*addr);
	} else
		subexp[0].s.rsp = loc2;
	subexp[0].e.rep = 0;
	if(rregexec(pattern, linebuf, subexp, MAXSUB)) {
		loc1 = subexp[0].s.rsp;
		loc2 = subexp[0].e.rep;
		return 1;
	}
	loc1 = loc2 = 0;
	return 0;

}

void
putd(void)
{
	int r;

	r = count%10;
	count /= 10;
	if(count)
		putd();
	putchr(r + '0');
}

void
putst(char *sp)
{
	Rune r;

	col = 0;
	for(;;) {
		sp += chartorune(&r, sp);
		if(r == 0)
			break;
		putchr(r);
	}
	putchr('\n');
}

void
putshst(Rune *sp)
{
	col = 0;
	while(*sp)
		putchr(*sp++);
	putchr('\n');
}

void
putchr(int ac)
{
	char *lp;
	int c;
	Rune rune;

	lp = linp;
	c = ac;
	if(listf) {
		if(c == '\n') {
			if(linp != line && linp[-1] == ' ') {
				*lp++ = '\\';
				*lp++ = 'n';
			}
		} else {
			if(col > (LINELEN-BELL)) {
				col = 8;
				*lp++ = '\\';
				*lp++ = '\n';
				*lp++ = '\t';
			}
			col++;
			if(c=='\b' || c=='\t' || c=='\\') {
				*lp++ = '\\';
				if(c == '\b')
					c = 'b';
				else
				if(c == '\t')
					c = 't';
				col++;
			} else if (c<' ' || c=='\177') {
				*lp++ = '\\';
				*lp++ = 'x';
				*lp++ = hex[(c>>4)&0xF];
				c     = hex[c&0xF];
				col += 3;
			} else if (c>'\177' && c<=0xFFFF) {
				*lp++ = '\\';
				*lp++ = 'u';
				*lp++ = hex[(c>>12)&0xF];
				*lp++ = hex[(c>>8)&0xF];
				*lp++ = hex[(c>>4)&0xF];
				c     = hex[c&0xF];
				col += 5;
			} else if (c>0xFFFF) {
				*lp++ = '\\';
				*lp++ = 'U';
				*lp++ = hex[(c>>28)&0xF];
				*lp++ = hex[(c>>24)&0xF];
				*lp++ = hex[(c>>20)&0xF];
				*lp++ = hex[(c>>16)&0xF];
				*lp++ = hex[(c>>12)&0xF];
				*lp++ = hex[(c>>8)&0xF];
				*lp++ = hex[(c>>4)&0xF];
				c     = hex[c&0xF];
				col += 9;
			}
		}
	}

	rune = c;
	lp += runetochar(lp, &rune);

	if(c == '\n' || lp >= &line[LINELEN-BELL]) {
		linp = line;
		write(oflag? 2: 1, line, lp-line);
		return;
	}
	linp = lp;
}

char*
mktemp(char *as)
{
	char *s;
	unsigned pid;
	int i;

	pid = getpid();
	s = as;
	while(*s++)
		;
	s--;
	while(*--s == 'X') {
		*s = pid % 10 + '0';
		pid /= 10;
	}
	s++;
	i = 'a';
	while(access(as, 0) != -1) {
		if(i == 'z')
			return "/";
		*s = i++;
	}
	return as;
}

void
regerror(char *s)
{
	USED(s);
	error(Q);
}
