#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mach.h>

/*
 * file - determine type of file
 */
#define	LENDIAN(p)	((p)[0] | ((p)[1]<<8) | ((p)[2]<<16) | ((p)[3]<<24))

uchar	buf[6001];
short	cfreq[140];
short	wfreq[50];
int	nbuf;
Dir*	mbuf;
int	fd;
char 	*fname;
char	*slash;

enum
{
	Cword,
	Fword,
	Aword,
	Alword,
	Lword,
	I1,
	I2,
	I3,
	Clatin	= 128,
	Cbinary,
	Cnull,
	Ceascii,
	Cutf,
};
struct
{
	char*	word;
	int	class;
} dict[] =
{
	"PATH",		Lword,
	"TEXT",		Aword,
	"adt",		Alword,
	"aggr",		Alword,
	"alef",		Alword,
	"array",	Lword,
	"block",	Fword,
	"chan",		Alword,
	"char",		Cword,
	"common",	Fword,
	"con",		Lword,
	"data",		Fword,
	"dimension",	Fword,
	"double",	Cword,
	"extern",	Cword,
	"bio",		I2,
	"float",	Cword,
	"fn",		Lword,
	"function",	Fword,
	"h",		I3,
	"implement",	Lword,
	"import",	Lword,
	"include",	I1,
	"int",		Cword,
	"integer",	Fword,
	"iota",		Lword,
	"libc",		I2,
	"long",		Cword,
	"module",	Lword,
	"real",		Fword,
	"ref",		Lword,
	"register",	Cword,
	"self",		Lword,
	"short",	Cword,
	"static",	Cword,
	"stdio",	I2,
	"struct",	Cword,
	"subroutine",	Fword,
	"u",		I2,
	"void",		Cword,
};

/* codes for 'mode' field in language structure */
enum	{
		Normal	= 0,
		First,		/* first entry for language spanning several ranges */
		Multi,		/* later entries "   "       "  ... */
		Shared,		/* codes used in several languages */
	};

struct
{
	int	mode;		/* see enum above */
	int 	count;
	int	low;
	int	high;
	char	*name;

} language[] =
{
	Normal, 0,	0x0080, 0x0080,	"Extended Latin",
	Normal,	0,	0x0100,	0x01FF,	"Extended Latin",
	Normal,	0,	0x0370,	0x03FF,	"Greek",
	Normal,	0,	0x0400,	0x04FF,	"Cyrillic",
	Normal,	0,	0x0530,	0x058F,	"Armenian",
	Normal,	0,	0x0590,	0x05FF,	"Hebrew",
	Normal,	0,	0x0600,	0x06FF,	"Arabic",
	Normal,	0,	0x0900,	0x097F,	"Devanagari",
	Normal,	0,	0x0980,	0x09FF,	"Bengali",
	Normal,	0,	0x0A00,	0x0A7F,	"Gurmukhi",
	Normal,	0,	0x0A80,	0x0AFF,	"Gujarati",
	Normal,	0,	0x0B00,	0x0B7F,	"Oriya",
	Normal,	0,	0x0B80,	0x0BFF,	"Tamil",
	Normal,	0,	0x0C00,	0x0C7F,	"Telugu",
	Normal,	0,	0x0C80,	0x0CFF,	"Kannada",
	Normal,	0,	0x0D00,	0x0D7F,	"Malayalam",
	Normal,	0,	0x0E00,	0x0E7F,	"Thai",
	Normal,	0,	0x0E80,	0x0EFF,	"Lao",
	Normal,	0,	0x1000,	0x105F,	"Tibetan",
	Normal,	0,	0x10A0,	0x10FF,	"Georgian",
	Normal,	0,	0x3040,	0x30FF,	"Japanese",
	Normal,	0,	0x3100,	0x312F,	"Chinese",
	First,	0,	0x3130,	0x318F,	"Korean",
	Multi,	0,	0x3400,	0x3D2F,	"Korean",
	Shared,	0,	0x4e00,	0x9fff,	"CJK",
	Normal,	0,	0,	0,	0,		/* terminal entry */
};


enum
{
	Fascii,		/* printable ascii */
	Flatin,		/* latin 1*/
	Futf,		/* UTf character set */
	Fbinary,	/* binary */
	Feascii,	/* ASCII with control chars */
	Fnull,		/* NULL in file */
} guess;

void	bump_utf_count(Rune);
int	cistrncmp(char*, char*, int);
void	filetype(int);
int	getfontnum(uchar*, uchar**);
int	isas(void);
int	isc(void);
int	isenglish(void);
int	ishp(void);
int	ishtml(void);
int	isrfc822(void);
int	ismbox(void);
int	islimbo(void);
int	ismung(void);
int	isp9bit(void);
int	isp9font(void);
int	isrtf(void);
int	ismsdos(void);
int	iself(void);
int	istring(void);
int	iff(void);
int	long0(void);
int	istar(void);
int	p9bitnum(uchar*);
int	p9subfont(uchar*);
void	print_utf(void);
void	type(char*, int);
int	utf_count(void);
void	wordfreq(void);

int	(*call[])(void) =
{
	long0,		/* recognizable by first 4 bytes */
	istring,	/* recognizable by first string */
	iff,		/* interchange file format (strings) */
	isrfc822,	/* email file */
	ismbox,		/* mail box */
	istar,		/* recognizable by tar checksum */
	ishtml,		/* html keywords */
/*	iscint,		/* compiler/assembler intermediate */
	islimbo,	/* limbo source */
	isc,		/* c & alef compiler key words */
	isas,		/* assembler key words */
	ismung,		/* entropy compressed/encrypted */
	isp9font,	/* plan 9 font */
	isp9bit,	/* plan 9 image (as from /dev/window) */
	isenglish,	/* char frequency English */
	isrtf,		/* rich text format */
	ismsdos,	/* msdos exe (virus file attachement) */
	iself,		/* ELF (foreign) executable */
	0
};

int mime;

#define OCTET	"application/octet-stream\n"
#define PLAIN	"text/plain\n"

void
main(int argc, char *argv[])
{
	int i, j, maxlen;
	char *cp;
	Rune r;

	ARGBEGIN{
	case 'm':
		mime = 1;
		break;
	default:
		fprint(2, "usage: file [-m] [file...]\n");
		exits("usage");
	}ARGEND;

	maxlen = 0;
	if(mime == 0 || argc > 1){
		for(i = 0; i < argc; i++) {
			for (j = 0, cp = argv[i]; *cp; j++, cp += chartorune(&r, cp))
					;
			if(j > maxlen)
				maxlen = j;
		}
	}
	if (argc <= 0) {
		if(!mime)
			print ("stdin: ");
		filetype(0);
	}
	else {
		for(i = 0; i < argc; i++)
			type(argv[i], maxlen);
	}
	exits(0);
}

void
type(char *file, int nlen)
{
	Rune r;
	int i;
	char *p;

	if(nlen > 0){
		slash = 0;
		for (i = 0, p = file; *p; i++) {
			if (*p == '/')			/* find rightmost slash */
				slash = p;
			p += chartorune(&r, p);		/* count runes */
		}
		print("%s:%*s",file, nlen-i+1, "");
	}
	fname = file;
	if ((fd = open(file, OREAD)) < 0) {
		print("cannot open\n");
		return;
	}
	filetype(fd);
	close(fd);
}

void
filetype(int fd)
{
	Rune r;
	int i, f, n;
	char *p, *eob;

	free(mbuf);
	mbuf = dirfstat(fd);
	if(mbuf == nil){
		print("cannot stat: %r\n");
		return;
	}
	if(mbuf->mode & DMDIR) {
		print(mime ? "text/directory\n" : "directory\n");
		return;
	}
	if(mbuf->type != 'M' && mbuf->type != '|') {
		print(mime ? OCTET : "special file #%c/%s\n",
			mbuf->type, mbuf->name);
		return;
	}
	nbuf = read(fd, buf, sizeof(buf)-1);

	if(nbuf < 0) {
		print("cannot read\n");
		return;
	}
	if(nbuf == 0) {
		print(mime ? PLAIN : "empty file\n");
		return;
	}
	buf[nbuf] = 0;

	/*
	 * build histogram table
	 */
	memset(cfreq, 0, sizeof(cfreq));
	for (i = 0; language[i].name; i++)
		language[i].count = 0;
	eob = (char *)buf+nbuf;
	for(n = 0, p = (char *)buf; p < eob; n++) {
		if (!fullrune(p, eob-p) && eob-p < UTFmax)
			break;
		p += chartorune(&r, p);
		if (r == 0)
			f = Cnull;
		else if (r <= 0x7f) {
			if (!isprint(r) && !isspace(r))
				f = Ceascii;	/* ASCII control char */
			else f = r;
		} else if (r == 0x080) {
			bump_utf_count(r);
			f = Cutf;
		} else if (r < 0xA0)
				f = Cbinary;	/* Invalid Runes */
		else if (r <= 0xff)
				f = Clatin;	/* Latin 1 */
		else {
			bump_utf_count(r);
			f = Cutf;		/* UTF extension */
		}
		cfreq[f]++;			/* ASCII chars peg directly */
	}
	/*
	 * gross classify
	 */
	if (cfreq[Cbinary])
		guess = Fbinary;
	else if (cfreq[Cutf])
		guess = Futf;
	else if (cfreq[Clatin])
		guess = Flatin;
	else if (cfreq[Ceascii])
		guess = Feascii;
	else if (cfreq[Cnull] == n) {
		print(mime ? OCTET : "first block all null bytes\n");
		return;
	}
	else guess = Fascii;
	/*
	 * lookup dictionary words
	 */
	memset(wfreq, 0, sizeof(wfreq));
	if(guess == Fascii || guess == Flatin || guess == Futf)
		wordfreq();
	/*
	 * call individual classify routines
	 */
	for(i=0; call[i]; i++)
		if((*call[i])())
			return;

	/*
	 * if all else fails,
	 * print out gross classification
	 */
	if (nbuf < 100 && !mime)
		print(mime ? PLAIN : "short ");
	if (guess == Fascii)
		print(mime ? PLAIN : "Ascii\n");
	else if (guess == Feascii)
		print(mime ? PLAIN : "extended ascii\n");
	else if (guess == Flatin)
		print(mime ? PLAIN : "latin ascii\n");
	else if (guess == Futf && utf_count() < 4)
		print_utf();
	else print(mime ? OCTET : "binary\n");
}

void
bump_utf_count(Rune r)
{
	int low, high, mid;

	high = sizeof(language)/sizeof(language[0])-1;
	for (low = 0; low < high;) {
		mid = (low+high)/2;
		if (r >=language[mid].low) {
			if (r <= language[mid].high) {
				language[mid].count++;
				break;
			} else low = mid+1;
		} else high = mid;
	}
}

int
utf_count(void)
{
	int i, count;

	count = 0;
	for (i = 0; language[i].name; i++)
		if (language[i].count > 0)
			switch (language[i].mode) {
			case Normal:
			case First:
				count++;
				break;
			default:
				break;
			}
	return count;
}

int
chkascii(void)
{
	int i;

	for (i = 'a'; i < 'z'; i++)
		if (cfreq[i])
			return 1;
	for (i = 'A'; i < 'Z'; i++)
		if (cfreq[i])
			return 1;
	return 0;
}

int
find_first(char *name)
{
	int i;

	for (i = 0; language[i].name != 0; i++)
		if (language[i].mode == First
			&& strcmp(language[i].name, name) == 0)
			return i;
	return -1;
}

void
print_utf(void)
{
	int i, printed, j;

	if(mime){
		print(PLAIN);
		return;
	}
	if (chkascii()) {
		printed = 1;
		print("Ascii");
	} else
		printed = 0;
	for (i = 0; language[i].name; i++)
		if (language[i].count) {
			switch(language[i].mode) {
			case Multi:
				j = find_first(language[i].name);
				if (j < 0)
					break;
				if (language[j].count > 0)
					break;
				/* Fall through */
			case Normal:
			case First:
				if (printed)
					print(" & ");
				else printed = 1;
				print("%s", language[i].name);
				break;
			case Shared:
			default:
				break;
			}
		}
	if(!printed)
		print("UTF");
	print(" text\n");
}

void
wordfreq(void)
{
	int low, high, mid, r;
	uchar *p, *p2, c;

	p = buf;
	for(;;) {
		while (p < buf+nbuf && !isalpha(*p))
			p++;
		if (p >= buf+nbuf)
			return;
		p2 = p;
		while(p < buf+nbuf && isalpha(*p))
			p++;
		c = *p;
		*p = 0;
		high = sizeof(dict)/sizeof(dict[0]);
		for(low = 0;low < high;) {
			mid = (low+high)/2;
			r = strcmp(dict[mid].word, (char*)p2);
			if(r == 0) {
				wfreq[dict[mid].class]++;
				break;
			}
			if(r < 0)
				low = mid+1;
			else
				high = mid;
		}
		*p++ = c;
	}
}

typedef struct Filemagic Filemagic;
struct Filemagic {
	ulong x;
	ulong mask;
	char *desc;
	char *mime;
};

Filemagic long0tab[] = {
	0xF16DF16D,	0xFFFFFFFF,	"pac1 audio file\n",	OCTET,
	0x31636170,	0xFFFFFFFF,	"pac3 audio file\n",	OCTET,
	0x32636170,	0xFFFF00FF,	"pac4 audio file\n",	OCTET,
	0xBA010000,	0xFFFFFFFF,	"mpeg system stream\n",	OCTET,
	0x30800CC0,	0xFFFFFFFF,	"inferno .dis executable\n", OCTET,
	0x04034B50,	0xFFFFFFFF,	"zip archive\n", "application/zip\n",
	070707,		0xFFFF,		"cpio archive\n", OCTET,
	0x2F7,		0xFFFF,		"tex dvi\n", "application/dvi\n",
	0xfffa0000,	0xfffe0000,	"mp3 audio\n",	"audio/mpeg\n",
	0xcafebabe,	0xFFFFFFFF,	"Mach-O fat executable\n",	"application/x-mach-binary\n",
	0xfeedface,	0xFFFFFFFE,	"Mach-O executable\n",	"application/x-mach-binary\n",
	0xbebafeca,	0xFFFFFFFF,	"Java class\n",	"application/x-java-applet\n",
};

int
filemagic(Filemagic *tab, int ntab, ulong x)
{
	int i;

	for(i=0; i<ntab; i++)
		if((x&tab[i].mask) == tab[i].x){
			print(mime ? tab[i].mime : tab[i].desc);
			return 1;
		}
	return 0;
}

int
long0(void)
{
/*	Fhdr *f; */
	long x;

	seek(fd, 0, 0);		/* reposition to start of file */
/*
	if(crackhdr(fd, &f)) {
		print(mime ? OCTET : "%s\n", f.name);
		return 1;
	}
*/
	x = LENDIAN(buf);
	if(filemagic(long0tab, nelem(long0tab), x))
		return 1;
	return 0;
}

/* from tar.c */
enum { NAMSIZ = 100, TBLOCK = 512 };

union	hblock
{
	char	dummy[TBLOCK];
	struct	header
	{
		char	name[NAMSIZ];
		char	mode[8];
		char	uid[8];
		char	gid[8];
		char	size[12];
		char	mtime[12];
		char	chksum[8];
		char	linkflag;
		char	linkname[NAMSIZ];
		/* rest are defined by POSIX's ustar format; see p1003.2b */
		char	magic[6];	/* "ustar" */
		char	version[2];
		char	uname[32];
		char	gname[32];
		char	devmajor[8];
		char	devminor[8];
		char	prefix[155];  /* if non-null, path = prefix "/" name */
	} dbuf;
};

int
checksum(union hblock *hp)
{
	int i;
	char *cp;
	struct header *hdr = &hp->dbuf;

	for (cp = hdr->chksum; cp < &hdr->chksum[sizeof hdr->chksum]; cp++)
		*cp = ' ';
	i = 0;
	for (cp = hp->dummy; cp < &hp->dummy[TBLOCK]; cp++)
		i += *cp & 0xff;
	return i;
}

int
istar(void)
{
	int chksum;
	char tblock[TBLOCK];
	union hblock *hp = (union hblock *)tblock;
	struct header *hdr = &hp->dbuf;

	seek(fd, 0, 0);		/* reposition to start of file */
	if (readn(fd, tblock, sizeof tblock) != sizeof tblock)
		return 0;
	chksum = strtol(hdr->chksum, 0, 8);
	if (hdr->name[0] != '\0' && checksum(hp) == chksum) {
		if (strcmp(hdr->magic, "ustar") == 0)
			print(mime? "application/x-ustar\n":
				"posix tar archive\n");
		else
			print(mime? "application/x-tar\n": "tar archive\n");
		return 1;
	}
	return 0;
}

/*
 * initial words to classify file
 */
struct	FILE_STRING
{
	char 	*key;
	char	*filetype;
	int	length;
	char	*mime;
} file_string[] =
{
	"!<arch>\n__.SYMDEF",	"archive random library",	16,	"application/octet-stream",
	"!<arch>\n",		"archive",			8,	"application/octet-stream",
	"070707",		"cpio archive - ascii header",	6,	"application/octet-stream",
	"%!",			"postscript",			2,	"application/postscript",
	"\004%!",		"postscript",			3,	"application/postscript",
	"x T post",		"troff output for post",	8,	"application/troff",
	"x T Latin1",		"troff output for Latin1",	10,	"application/troff",
	"x T utf",		"troff output for UTF",		7,	"application/troff",
	"x T 202",		"troff output for 202",		7,	"application/troff",
	"x T aps",		"troff output for aps",		7,	"application/troff",
	"GIF",			"GIF image", 			3,	"image/gif",
	"\0PC Research, Inc\0",	"ghostscript fax file",		18,	"application/ghostscript",
	"%PDF",			"PDF",				4,	"application/pdf",
	"<html>\n",		"HTML file",			7,	"text/html",
	"<HTML>\n",		"HTML file",			7,	"text/html",
	"compressed\n",		"Compressed image or subfont",	11,	"application/octet-stream",
	"\111\111\052\000",	"tiff",				4,	"image/tiff",
	"\115\115\000\052",	"tiff",				4,	"image/tiff",
	"\377\330\377\340",	"jpeg",				4,	"image/jpeg",
	"\377\330\377\341",	"jpeg",				4,	"image/jpeg",
	"\377\330\377\333",	"jpeg",				4,	"image/jpeg",
	"\106\117\126\142",	"x3f",				4,	"image/x3f",
	"BM",			"bmp",				2,	"image/bmp",
	"\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1",	"microsoft office document",	8,	"application/octet-stream",
	"<MakerFile ",		"FrameMaker file",		11,	"application/framemaker",
	"\033%-12345X",	"HPJCL file",		9,	"application/hpjcl",
	"ID3",			"mp3 audio with id3",	3,	"audio/mpeg",
	0,0,0,0
};

int
istring(void)
{
	int i, j;
	struct FILE_STRING *p;

	for(p = file_string; p->key; p++) {
		if(nbuf >= p->length && !memcmp(buf, p->key, p->length)) {
			if(mime)
				print("%s\n", p->mime);
			else
				print("%s\n", p->filetype);
			return 1;
		}
	}
	if(strncmp((char*)buf, "TYPE=", 5) == 0) {	/* td */
		for(i = 5; i < nbuf; i++)
			if(buf[i] == '\n')
				break;
		if(mime)
			print(OCTET);
		else
			print("%.*s picture\n", utfnlen((char*)buf+5, i-5), (char*)buf+5);
		return 1;
	}
	if(buf[0]=='#' && buf[1]=='!'){
		i=2;
		for(j=2; j < nbuf && buf[j] != ' ' && buf[j] != '\n' && buf[j] != '\r'; j++)
			if(buf[j] == '/')
				i = j+1;
		if(mime)
			print(PLAIN);
		else
			print("%.*s executable file script\n", utfnlen((char*)buf+i, j-i), (char*)buf+i);
		return 1;
	}
	return 0;
}

int
iff(void)
{
	if (strncmp((char*)buf, "FORM", 4) == 0 &&
	    strncmp((char*)buf+8, "AIFF", 4) == 0) {
		print("%s\n", mime? "audio/x-aiff": "aiff audio");
		return 1;
	}
	return 0;
}

char*	html_string[] =
{
	"title",
	"body",
	"head",
	"strong",
	"h1",
	"h2",
	"h3",
	"h4",
	"h5",
	"h6",
	"ul",
	"li",
	"dl",
	"br",
	"em",
	0,
};

int
ishtml(void)
{
	uchar *p, *q;
	int i, count;

		/* compare strings between '<' and '>' to html table */
	count = 0;
	p = buf;
	for(;;) {
		while (p < buf+nbuf && *p != '<')
			p++;
		p++;
		if (p >= buf+nbuf)
			break;
		if(*p == '/')
			p++;
		q = p;
		while(p < buf+nbuf && *p != '>')
			p++;
		if (p >= buf+nbuf)
			break;
		for(i = 0; html_string[i]; i++) {
			if(cistrncmp(html_string[i], (char*)q, p-q) == 0) {
				if(count++ > 4) {
					print(mime ? "text/html\n" : "HTML file\n");
					return 1;
				}
				break;
			}
		}
		p++;
	}
	return 0;
}

char*	rfc822_string[] =
{
	"from:",
	"date:",
	"to:",
	"subject:",
	"received:",
	"reply to:",
	"sender:",
	0,
};

int
isrfc822(void)
{

	char *p, *q, *r;
	int i, count;

	count = 0;
	p = (char*)buf;
	for(;;) {
		q = strchr(p, '\n');
		if(q == nil)
			break;
		*q = 0;
		if(p == (char*)buf && strncmp(p, "From ", 5) == 0 && strstr(p, " remote from ")){
			count++;
			*q = '\n';
			p = q+1;
			continue;
		}
		*q = '\n';
		if(*p != '\t' && *p != ' '){
			r = strchr(p, ':');
			if(r == 0 || r > q)
				break;
			for(i = 0; rfc822_string[i]; i++) {
				if(cistrncmp(p, rfc822_string[i], strlen(rfc822_string[i])) == 0){
					count++;
					break;
				}
			}
		}
		p = q+1;
	}
	if(count >= 3){
		print(mime ? "message/rfc822\n" : "email file\n");
		return 1;
	}
	return 0;
}

int
ismbox(void)
{
	char *p, *q;

	p = (char*)buf;
	q = strchr(p, '\n');
	if(q == nil)
		return 0;
	*q = 0;
	if(strncmp(p, "From ", 5) == 0 && strstr(p, " remote from ") == nil){
		print(mime ? "text/plain\n" : "mail box\n");
		return 1;
	}
	*q = '\n';
	return 0;
}

int
isc(void)
{
	int n;

	n = wfreq[I1];
	/*
	 * includes
	 */
	if(n >= 2 && wfreq[I2] >= n && wfreq[I3] >= n && cfreq['.'] >= n)
		goto yes;
	if(n >= 1 && wfreq[Alword] >= n && wfreq[I3] >= n && cfreq['.'] >= n)
		goto yes;
	/*
	 * declarations
	 */
	if(wfreq[Cword] >= 5 && cfreq[';'] >= 5)
		goto yes;
	/*
	 * assignments
	 */
	if(cfreq[';'] >= 10 && cfreq['='] >= 10 && wfreq[Cword] >= 1)
		goto yes;
	return 0;

yes:
	if(mime){
		print(PLAIN);
		return 1;
	}
	if(wfreq[Alword] > 0)
		print("alef program\n");
	else
		print("c program\n");
	return 1;
}

int
islimbo(void)
{

	/*
	 * includes
	 */
	if(wfreq[Lword] < 4)
		return 0;
	print(mime ? PLAIN : "limbo program\n");
	return 1;
}

int
isas(void)
{

	/*
	 * includes
	 */
	if(wfreq[Aword] < 2)
		return 0;
	print(mime ? PLAIN : "as program\n");
	return 1;
}

/*
 * low entropy means encrypted
 */
int
ismung(void)
{
	int i, bucket[8];
	float cs;

	if(nbuf < 64)
		return 0;
	memset(bucket, 0, sizeof(bucket));
	for(i=0; i<64; i++)
		bucket[(buf[i]>>5)&07] += 1;

	cs = 0.;
	for(i=0; i<8; i++)
		cs += (bucket[i]-8)*(bucket[i]-8);
	cs /= 8.;
	if(cs <= 24.322) {
		if(buf[0]==0x1f && (buf[1]==0x8b || buf[1]==0x9d))
			print(mime ? OCTET : "compressed\n");
		else
			print(mime ? OCTET : "encrypted\n");
		return 1;
	}
	return 0;
}

/*
 * english by punctuation and frequencies
 */
int
isenglish(void)
{
	int vow, comm, rare, badpun, punct;
	char *p;

	if(guess != Fascii && guess != Feascii)
		return 0;
	badpun = 0;
	punct = 0;
	for(p = (char *)buf; p < (char *)buf+nbuf-1; p++)
		switch(*p) {
		case '.':
		case ',':
		case ')':
		case '%':
		case ';':
		case ':':
		case '?':
			punct++;
			if(p[1] != ' ' && p[1] != '\n')
				badpun++;
		}
	if(badpun*5 > punct)
		return 0;
	if(cfreq['>']+cfreq['<']+cfreq['/'] > cfreq['e'])	/* shell file test */
		return 0;
	if(2*cfreq[';'] > cfreq['e'])
		return 0;

	vow = 0;
	for(p="AEIOU"; *p; p++) {
		vow += cfreq[(uchar)*p];
		vow += cfreq[tolower((uchar)*p)];
	}
	comm = 0;
	for(p="ETAION"; *p; p++) {
		comm += cfreq[(uchar)*p];
		comm += cfreq[tolower((uchar)*p)];
	}
	rare = 0;
	for(p="VJKQXZ"; *p; p++) {
		rare += cfreq[(uchar)*p];
		rare += cfreq[tolower((uchar)*p)];
	}
	if(vow*5 >= nbuf-cfreq[' '] && comm >= 10*rare) {
		print(mime ? PLAIN : "English text\n");
		return 1;
	}
	return 0;
}

/*
 * pick up a number with
 * syntax _*[0-9]+_
 */
#define	P9BITLEN	12
int
p9bitnum(uchar *bp)
{
	int n, c, len;

	len = P9BITLEN;
	while(*bp == ' ') {
		bp++;
		len--;
		if(len <= 0)
			return -1;
	}
	n = 0;
	while(len > 1) {
		c = *bp++;
		if(!isdigit(c))
			return -1;
		n = n*10 + c-'0';
		len--;
	}
	if(*bp != ' ')
		return -1;
	return n;
}

int
depthof(char *s, int *newp)
{
	char *es;
	int d;

	*newp = 0;
	es = s+12;
	while(s<es && *s==' ')
		s++;
	if(s == es)
		return -1;
	if('0'<=*s && *s<='9')
		return 1<<atoi(s);

	*newp = 1;
	d = 0;
	while(s<es && *s!=' '){
		s++;	/* skip letter */
		d += strtoul(s, &s, 10);
	}

	switch(d){
	case 32:
	case 24:
	case 16:
	case 8:
		return d;
	}
	return -1;
}

int
isp9bit(void)
{
	int dep, lox, loy, hix, hiy, px, new;
	ulong t;
	long len;
	char *newlabel;

	newlabel = "old ";

	dep = depthof((char*)buf + 0*P9BITLEN, &new);
	if(new)
		newlabel = "";
	lox = p9bitnum(buf + 1*P9BITLEN);
	loy = p9bitnum(buf + 2*P9BITLEN);
	hix = p9bitnum(buf + 3*P9BITLEN);
	hiy = p9bitnum(buf + 4*P9BITLEN);
	if(dep < 0 || lox < 0 || loy < 0 || hix < 0 || hiy < 0)
		return 0;

	if(dep < 8){
		px = 8/dep;	/* pixels per byte */
		/* set l to number of bytes of data per scan line */
		if(lox >= 0)
			len = (hix+px-1)/px - lox/px;
		else{	/* make positive before divide */
			t = (-lox)+px-1;
			t = (t/px)*px;
			len = (t+hix+px-1)/px;
		}
	}else
		len = (hix-lox)*dep/8;
	len *= (hiy-loy);		/* col length */
	len += 5*P9BITLEN;		/* size of initial ascii */

	/*
	 * for image file, length is non-zero and must match calculation above
	 * for /dev/window and /dev/screen the length is always zero
	 * for subfont, the subfont header should follow immediately.
	 */
	if (len != 0 && mbuf->length == 0) {
		print("%splan 9 image\n", newlabel);
		return 1;
	}
	if (mbuf->length == len) {
		print("%splan 9 image\n", newlabel);
		return 1;
	}
	/* Ghostscript sometimes produces a little extra on the end */
	if (mbuf->length < len+P9BITLEN) {
		print("%splan 9 image\n", newlabel);
		return 1;
	}
	if (p9subfont(buf+len)) {
		print("%ssubfont file\n", newlabel);
		return 1;
	}
	return 0;
}

int
p9subfont(uchar *p)
{
	int n, h, a;

		/* if image too big, assume it's a subfont */
	if (p+3*P9BITLEN > buf+sizeof(buf))
		return 1;

	n = p9bitnum(p + 0*P9BITLEN);	/* char count */
	if (n < 0)
		return 0;
	h = p9bitnum(p + 1*P9BITLEN);	/* height */
	if (h < 0)
		return 0;
	a = p9bitnum(p + 2*P9BITLEN);	/* ascent */
	if (a < 0)
		return 0;
	return 1;
}

#define	WHITESPACE(c)		((c) == ' ' || (c) == '\t' || (c) == '\n')

int
isp9font(void)
{
	uchar *cp, *p;
	int i, n;
	char pathname[1024];

	cp = buf;
	if (!getfontnum(cp, &cp))	/* height */
		return 0;
	if (!getfontnum(cp, &cp))	/* ascent */
		return 0;
	for (i = 0; 1; i++) {
		if (!getfontnum(cp, &cp))	/* min */
			break;
		if (!getfontnum(cp, &cp))	/* max */
			return 0;
		while (WHITESPACE(*cp))
			cp++;
		for (p = cp; *cp && !WHITESPACE(*cp); cp++)
				;
			/* construct a path name, if needed */
		n = 0;
		if (*p != '/' && slash) {
			n = slash-fname+1;
			if (n < sizeof(pathname))
				memcpy(pathname, fname, n);
			else n = 0;
		}
		if (n+cp-p < sizeof(pathname)) {
			memcpy(pathname+n, p, cp-p);
			n += cp-p;
			pathname[n] = 0;
			if (access(pathname, AEXIST) < 0)
				return 0;
		}
	}
	if (i) {
		print(mime ? "text/plain\n" : "font file\n");
		return 1;
	}
	return 0;
}

int
getfontnum(uchar *cp, uchar **rp)
{
	while (WHITESPACE(*cp))		/* extract ulong delimited by whitespace */
		cp++;
	if (*cp < '0' || *cp > '9')
		return 0;
	strtoul((char *)cp, (char **)rp, 0);
	if (!WHITESPACE(**rp))
		return 0;
	return 1;
}

int
isrtf(void)
{
	if(strstr((char *)buf, "\\rtf1")){
		print(mime ? "application/rtf\n" : "rich text format\n");
		return 1;
	}
	return 0;
}

int
ismsdos(void)
{
	if (buf[0] == 0x4d && buf[1] == 0x5a){
		print(mime ? "application/x-msdownload\n" : "MSDOS executable\n");
		return 1;
	}
	return 0;
}

int
iself(void)
{
	static char *cpu[] = {		/* NB: incomplete and arbitary list */
		nil,
	/*1*/	"WE32100",
	/*2*/	"SPARC",
	/*3*/	"i386",
	/*4*/	"M68000",
	/*5*/	"M88000",
	/*6*/	"i486",
	/*7*/	"i860",
	/*8*/	"R3000",
	/*9*/	"S370",
	/*10*/	"R4000",
		nil, nil, nil, nil,
	/*15*/	"HP-PA",
		nil,
		nil,
	/*18*/	"sparc v8+",
	/*19*/	"i960",
	/*20*/	"PPC-32",
	/*21*/	"PPC-64",
		nil, nil, nil, nil,
		nil, nil, nil, nil, nil,
		nil, nil, nil, nil, nil,
		nil, nil, nil, nil,
	/*40*/	"ARM",
	/*41*/	"Alpha",
		nil,
	/*43*/	"sparc v9",
		nil, nil,
		nil, nil, nil, nil,
	/*50*/	"IA-64",
		nil, nil, nil, nil, nil,
		nil, nil, nil, nil, nil,
		nil,
	/*62*/	"AMD64",
		nil, nil, nil,
		nil, nil, nil, nil, nil,
		nil, nil, nil, nil,
	/*75*/	"VAX",
	};


	if (memcmp(buf, "\177ELF", 4) == 0){
		/* gcc misparses \x7FELF as \x7FE L F */
		if (!mime){
			int n = (buf[19] << 8) | buf[18];
			char *p = "unknown";

			if (n > 0 && n < nelem(cpu) && cpu[n])
				p = cpu[n];
			else {
				/* try the other byte order */
				n = (buf[18] << 8) | buf[19];
				if (n > 0 && n < nelem(cpu) && cpu[n])
					p = cpu[n];
			}
			print("%s ELF executable\n", p);
		}
		else
			print("application/x-elf-executable");
		return 1;
	}

	return 0;
}
