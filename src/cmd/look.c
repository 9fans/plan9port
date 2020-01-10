#include <u.h>
#include <libc.h>
#include <bio.h>
	/* Macros for Rune support of ctype.h-like functions */

#undef isupper
#undef islower
#undef isalpha
#undef isdigit
#undef isalnum
#undef isspace
#undef tolower
#define	isupper(r)	('A' <= (r) && (r) <= 'Z')
#define	islower(r)	('a' <= (r) && (r) <= 'z')
#define	isalpha(r)	(isupper(r) || islower(r))
#define	islatin1(r)	(0xC0 <= (r) && (r) <= 0xFF)

#define	isdigit(r)	('0' <= (r) && (r) <= '9')

#define	isalnum(r)	(isalpha(r) || isdigit(r))

#define	isspace(r)	((r) == ' ' || (r) == '\t' \
			|| (0x0A <= (r) && (r) <= 0x0D))

#define	tolower(r)	((r)-'A'+'a')

#define	sgn(v)		((v) < 0 ? -1 : ((v) > 0 ? 1 : 0))

#define	WORDSIZ	4000
char	*filename = "#9/lib/words";
Biobuf	*dfile;
Biobuf	bout;
Biobuf	bin;

int	fold;
int	direc;
int	exact;
int	iflag;
int	rev = 1;	/*-1 for reverse-ordered file, not implemented*/
int	(*compare)(Rune*, Rune*);
Rune	tab = '\t';
Rune	entry[WORDSIZ];
Rune	word[WORDSIZ];
Rune	key[50], orig[50];
Rune	latin_fold_tab[] =
{
/*	Table to fold latin 1 characters to ASCII equivalents
			based at Rune value 0xc0

	 À    Á    Â    Ã    Ä    Å    Æ    Ç
	 È    É    Ê    Ë    Ì    Í    Î    Ï
	 Ð    Ñ    Ò    Ó    Ô    Õ    Ö    ×
	 Ø    Ù    Ú    Û    Ü    Ý    Þ    ß
	 à    á    â    ã    ä    å    æ    ç
	 è    é    ê    ë    ì    í    î    ï
	 ð    ñ    ò    ó    ô    õ    ö    ÷
	 ø    ù    ú    û    ü    ý    þ    ÿ
*/
	'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c',
	'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
	'd', 'n', 'o', 'o', 'o', 'o', 'o',  0 ,
	'o', 'u', 'u', 'u', 'u', 'y',  0 ,  0 ,
	'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c',
	'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
	'd', 'n', 'o', 'o', 'o', 'o', 'o',  0 ,
	'o', 'u', 'u', 'u', 'u', 'y',  0 , 'y',
};

int	locate(void);
int	acomp(Rune*, Rune*);
int	getword(Biobuf*, Rune *rp, int n);
void	torune(char*, Rune*);
void	rcanon(Rune*, Rune*);
int	ncomp(Rune*, Rune*);

void
main(int argc, char *argv[])
{
	int n;

	filename = unsharp(filename);

	Binit(&bin, 0, OREAD);
	Binit(&bout, 1, OWRITE);
	compare = acomp;
	ARGBEGIN{
	case 'd':
		direc++;
		break;
	case 'f':
		fold++;
		break;
	case 'i':
		iflag++;
		break;
	case 'n':
		compare = ncomp;
		break;
	case 't':
		chartorune(&tab,ARGF());
		break;
	case 'x':
		exact++;
		break;
	default:
		fprint(2, "%s: bad option %c\n", argv0, ARGC());
		fprint(2, "usage: %s -[dfinx] [-t c] [string] [file]\n", argv0);
		exits("usage");
	} ARGEND
	if(!iflag){
		if(argc >= 1) {
			torune(argv[0], orig);
			argv++;
			argc--;
		} else
			iflag++;
	}
	if(argc < 1) {
		direc++;
		fold++;
	} else
		filename = argv[0];
	if (!iflag)
		rcanon(orig, key);
	dfile = Bopen(filename, OREAD);
	if(dfile == 0) {
		fprint(2, "look: can't open %s\n", filename);
		exits("no dictionary");
	}
	if(!iflag)
		if(!locate())
			exits("not found");
	do {
		if(iflag) {
			Bflush(&bout);
			if(!getword(&bin, orig, sizeof(orig)/sizeof(orig[0])))
				exits(0);
			rcanon(orig, key);
			if(!locate())
				continue;
		}
		if (!exact || !acomp(word, key))
			Bprint(&bout, "%S\n", entry);
		while(getword(dfile, entry, sizeof(entry)/sizeof(entry[0]))) {
			rcanon(entry, word);
			n = compare(key, word);
			switch(n) {
			case -1:
				if(exact)
					break;
			case 0:
				if (!exact || !acomp(word, orig))
					Bprint(&bout, "%S\n", entry);
				continue;
			}
			break;
		}
	} while(iflag);
	exits(0);
}

int
locate(void)
{
	vlong top, bot, mid;
	int c;
	int n;

	bot = 0;
	top = Bseek(dfile, 0L, 2);
	for(;;) {
		mid = (top+bot) / 2;
		Bseek(dfile, mid, 0);
		do
			c = Bgetrune(dfile);
		while(c>=0 && c!='\n');
		mid = Boffset(dfile);
		if(!getword(dfile, entry, sizeof(entry)/sizeof(entry[0])))
			break;
		rcanon(entry, word);
		n = compare(key, word);
		switch(n) {
		case -2:
		case -1:
		case 0:
			if(top <= mid)
				break;
			top = mid;
			continue;
		case 1:
		case 2:
			bot = mid;
			continue;
		}
		break;
	}
	Bseek(dfile, bot, 0);
	while(getword(dfile, entry, sizeof(entry)/sizeof(entry[0]))) {
		rcanon(entry, word);
		n = compare(key, word);
		switch(n) {
		case -2:
			return 0;
		case -1:
			if(exact)
				return 0;
		case 0:
			return 1;
		case 1:
		case 2:
			continue;
		}
	}
	return 0;
}

/*
 *	acomp(s, t) returns:
 *		-2 if s strictly precedes t
 *		-1 if s is a prefix of t
 *		0 if s is the same as t
 *		1 if t is a prefix of s
 *		2 if t strictly precedes s
 */

int
acomp(Rune *s, Rune *t)
{
	int cs, ct;

	for(;;) {
		cs = *s;
		ct = *t;
		if(cs != ct)
			break;
		if(cs == 0)
			return 0;
		s++;
		t++;
	}
	if(cs == 0)
		return -1;
	if(ct == 0)
		return 1;
	if(cs < ct)
		return -2;
	return 2;
}

void
torune(char *old, Rune *new)
{
	do old += chartorune(new, old);
	while(*new++);
}

void
rcanon(Rune *old, Rune *new)
{
	Rune r;

	while((r = *old++) && r != tab) {
		if (islatin1(r) && latin_fold_tab[r-0xc0])
				r = latin_fold_tab[r-0xc0];
		if(direc)
			if(!(isalnum(r) || r == ' ' || r == '\t'))
				continue;
		if(fold)
			if(isupper(r))
				r = tolower(r);
		*new++ = r;
	}
	*new = 0;
}

int
ncomp(Rune *s, Rune *t)
{
	Rune *is, *it, *js, *jt;
	int a, b;
	int ssgn, tsgn;

	while(isspace(*s))
		s++;
	while(isspace(*t))
		t++;
	ssgn = tsgn = -2*rev;
	if(*s == '-') {
		s++;
		ssgn = -ssgn;
	}
	if(*t == '-') {
		t++;
		tsgn = -tsgn;
	}
	for(is = s; isdigit(*is); is++)
		;
	for(it = t; isdigit(*it); it++)
		;
	js = is;
	jt = it;
	a = 0;
	if(ssgn == tsgn)
		while(it>t && is>s)
			if(b = *--it - *--is)
				a = b;
	while(is > s)
		if(*--is != '0')
			return -ssgn;
	while(it > t)
		if(*--it != '0')
			return tsgn;
	if(a)
		return sgn(a)*ssgn;
	if(*(s=js) == '.')
		s++;
	if(*(t=jt) == '.')
		t++;
	if(ssgn == tsgn)
		while(isdigit(*s) && isdigit(*t))
			if(a = *t++ - *s++)
				return sgn(a)*ssgn;
	while(isdigit(*s))
		if(*s++ != '0')
			return -ssgn;
	while(isdigit(*t))
		if(*t++ != '0')
			return tsgn;
	return 0;
}

int
getword(Biobuf *f, Rune *rp, int n)
{
	long c;

	while(n-- > 0) {
		c = Bgetrune(f);
		if(c < 0)
			return 0;
		if(c == '\n') {
			*rp = '\0';
			return 1;
		}
		*rp++ = c;
	}
	fprint(2, "Look: word too long.  Bailing out.\n");
	return 0;
}
