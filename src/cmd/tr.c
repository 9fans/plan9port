#include 	<u.h>
#include 	<libc.h>

typedef struct PCB	/* Control block controlling specification parse */
{
	char	*base;		/* start of specification */
	char	*current;	/* current parse point */
	long	last;		/* last Rune returned */
	long	final;		/* final Rune in a span */
} Pcb;

uchar	bits[] = { 1, 2, 4, 8, 16, 32, 64, 128 };

#define	SETBIT(a, c)		((a)[(c)/8] |= bits[(c)&07])
#define	CLEARBIT(a,c)		((a)[(c)/8] &= ~bits[(c)&07])
#define	BITSET(a,c)		((a)[(c)/8] & bits[(c)&07])

#define	MAXRUNE	Runemax

uchar	f[(MAXRUNE+1)/8];
uchar	t[(MAXRUNE+1)/8];
char 	wbuf[4096];
char	*wptr;

Pcb pfrom, pto;

int cflag;
int dflag;
int sflag;

void	complement(void);
void	delete(void);
void	squeeze(void);
void	translit(void);
void	error(char*);
long	canon(Pcb*);
char	*getrune(char*, Rune*);
void	Pinit(Pcb*, char*);
void	Prewind(Pcb *p);
int	readrune(int, long*);
void	wflush(int);
void	writerune(int, Rune);

void
main(int argc, char **argv)
{
	ARGBEGIN{
	case 's':	sflag++; break;
	case 'd':	dflag++; break;
	case 'c':	cflag++; break;
	default:	error("bad option");
	}ARGEND
	if(argc>0)
		Pinit(&pfrom, argv[0]);
	if(argc>1)
		Pinit(&pto, argv[1]);
	if(argc>2)
		error("arg count");
	if(dflag) {
		if ((sflag && argc != 2) || (!sflag && argc != 1))
			error("arg count");
		delete();
	} else {
		if (argc != 2)
			error("arg count");
		if (cflag)
			complement();
		else translit();
	}
	exits(0);
}

void
delete(void)
{
	long c, last;

	if (cflag) {
		memset((char *) f, 0xff, sizeof f);
		while ((c = canon(&pfrom)) >= 0)
			CLEARBIT(f, c);
	} else {
		while ((c = canon(&pfrom)) >= 0)
			SETBIT(f, c);
	}
	if (sflag) {
		while ((c = canon(&pto)) >= 0)
			SETBIT(t, c);
	}

	last = Runemax+1;
	while (readrune(0, &c) > 0) {
		if(!BITSET(f, c) && (c != last || !BITSET(t,c))) {
			last = c;
			writerune(1, (Rune) c);
		}
	}
	wflush(1);
}

void
complement(void)
{
	Rune *p;
	int i;
	long from, to, lastc, high;

	lastc = 0;
	high = 0;
	while ((from = canon(&pfrom)) >= 0) {
		if (from > high) high = from;
		SETBIT(f, from);
	}
	while ((to = canon(&pto)) > 0) {
		if (to > high) high = to;
		SETBIT(t,to);
	}
	Prewind(&pto);
	if ((p = (Rune *) malloc((high+1)*sizeof(Rune))) == 0)
		error("can't allocate memory");
	for (i = 0; i <= high; i++){
		if (!BITSET(f,i)) {
			if ((to = canon(&pto)) < 0)
				to = lastc;
			else lastc = to;
			p[i] = to;
		}
		else p[i] = i;
	}
	if (sflag){
		lastc = Runemax+1;
		while (readrune(0, &from) > 0) {
			if (from > high)
				from = to;
			else
				from = p[from];
			if (from != lastc || !BITSET(t,from)) {
				lastc = from;
				writerune(1, (Rune) from);
			}
		}

	} else {
		while (readrune(0, &from) > 0){
			if (from > high)
				from = to;
			else
				from = p[from];
			writerune(1, (Rune) from);
		}
	}
	wflush(1);
}

void
translit(void)
{
	Rune *p;
	int i;
	long from, to, lastc, high;

	lastc = 0;
	high = 0;
	while ((from = canon(&pfrom)) >= 0)
		if (from > high) high = from;
	Prewind(&pfrom);
	if ((p = (Rune *) malloc((high+1)*sizeof(Rune))) == 0)
		error("can't allocate memory");
	for (i = 0; i <= high; i++)
		p[i] = i;
	while ((from = canon(&pfrom)) >= 0) {
		if ((to = canon(&pto)) < 0)
			to = lastc;
		else lastc = to;
		if (BITSET(f,from) && p[from] != to)
			error("ambiguous translation");
		SETBIT(f,from);
		p[from] = to;
		SETBIT(t,to);
	}
	while ((to = canon(&pto)) >= 0) {
		SETBIT(t,to);
	}
	if (sflag){
		lastc = Runemax+1;
		while (readrune(0, &from) > 0) {
			if (from <= high)
				from = p[from];
			if (from != lastc || !BITSET(t,from)) {
				lastc = from;
				writerune(1, (Rune) from);
			}
		}

	} else {
		while (readrune(0, &from) > 0) {
			if (from <= high)
				from = p[from];
			writerune(1, (Rune) from);
		}
	}
	wflush(1);
}

int
readrune(int fd, long *rp)
{
	Rune r;
	int j;
	static int i, n;
	static char buf[4096];

	j = i;
	for (;;) {
		if (i >= n) {
			wflush(1);
			if (j != i)
				memcpy(buf, buf+j, n-j);
			i = n-j;
			n = read(fd, &buf[i], sizeof(buf)-i);
			if (n < 0)
				error("read error");
			if (n == 0)
				return 0;
			j = 0;
			n += i;
		}
		i++;
		if (fullrune(&buf[j], i-j))
			break;
	}
	chartorune(&r, &buf[j]);
	*rp = r;
	return 1;
}

void
writerune(int fd, Rune r)
{
	char buf[UTFmax];
	int n;

	if (!wptr)
		wptr = wbuf;
	n = runetochar(buf, (Rune*)&r);
	if (wptr+n >= wbuf+sizeof(wbuf))
		wflush(fd);
	memcpy(wptr, buf, n);
	wptr += n;
}

void
wflush(int fd)
{
	if (wptr && wptr > wbuf)
		if (write(fd, wbuf, wptr-wbuf) != wptr-wbuf)
			error("write error");
	wptr = wbuf;
}

char *
getrune(char *s, Rune *rp)
{
	Rune r;
	char *save;
	int i, n;

	s += chartorune(rp, s);
	if((r = *rp) == '\\' && *s){
		n = 0;
		if (*s == 'x') {
			s++;
			for (i = 0; i < 4; i++) {
				save = s;
				s += chartorune(&r, s);
				if ('0' <= r && r <= '9')
					n = 16*n + r - '0';
				else if ('a' <= r && r <= 'f')
					n = 16*n + r - 'a' + 10;
				else if ('A' <= r && r <= 'F')
					n = 16*n + r - 'A' + 10;
				else {
					if (i == 0)
						*rp = 'x';
					else *rp = n;
					return save;
				}
			}
		} else {
			for(i = 0; i < 3; i++) {
				save = s;
				s += chartorune(&r, s);
				if('0' <= r && r <= '7')
					n = 8*n + r - '0';
				else {
					if (i == 0)
					{
						*rp = r;
						return s;
					}
					*rp = n;
					return save;
				}
			}
			if(n > 0377)
				error("char>0377");
		}
		*rp = n;
	}
	return s;
}

long
canon(Pcb *p)
{
	Rune r;

	if (p->final >= 0) {
		if (p->last < p->final)
			return ++p->last;
		p->final = -1;
	}
	if (*p->current == '\0')
		return -1;
	if(*p->current == '-' && p->last >= 0 && p->current[1]){
		p->current = getrune(p->current+1, &r);
		if (r < p->last)
			error ("Invalid range specification");
		if (r > p->last) {
			p->final = r;
			return ++p->last;
		}
	}
	p->current = getrune(p->current, &r);
	p->last = r;
	return p->last;
}

void
Pinit(Pcb *p, char *cp)
{
	p->current = p->base = cp;
	p->last = p->final = -1;
}
void
Prewind(Pcb *p)
{
	p->current = p->base;
	p->last = p->final = -1;
}
void
error(char *s)
{
	fprint(2, "%s: %s\n", argv0, s);
	exits(s);
}
