#include <u.h>
#include <libc.h>
#include <bio.h>

unsigned char	odata[16];
unsigned char	data[16];
int		ndata;
unsigned long	addr;
int		repeats;
int		swizzle;
int		swizzle8;
int		flush;
int		abase=2;
int		xd(char *, int);
void		xprint(char *, ulong);
void		initarg(void), swizz(void), swizz8(void);
enum{
	Narg=10
};
typedef struct Arg Arg;
typedef void fmtfn(char *);
struct Arg
{
	int	ascii;		/* 0==none, 1==ascii */
	int	loglen;		/* 0==1, 1==2, 2==4, 3==8 */
	int	base;		/* 0==8, 1==10, 2==16 */
	fmtfn	*fn;		/* function to call with data */
	char	*afmt;		/* format to use to print address */
	char	*fmt;		/* format to use to print data */
}arg[Narg];
int	narg;

fmtfn	fmt0, fmt1, fmt2, fmt3, fmtc;
fmtfn *fmt[4] = {
	fmt0,
	fmt1,
	fmt2,
	fmt3
};

char *dfmt[4][3] = {
	" %.3uo",	" %.3ud",	" %.2ux",
	" %.6uo",	" %.5ud",	" %.4ux",
	" %.11luo",	" %.10lud",	" %.8lux",
	" %.22lluo",	" %.20llud",	" %.16llux",
};

char *cfmt[3][3] = {
	"   %c",	"   %c", 	"  %c",
	" %.3s",	" %.3s",	" %.2s",
	" %.3uo",	" %.3ud",	" %.2ux",
};

char *afmt[2][3] = {
	"%.7luo ",	"%.7lud ",	"%.7lux ",
	"%7luo ",	"%7lud ",	"%7lux ",
};

Biobuf	bin;
Biobuf	bout;

void
main(int argc, char *argv[])
{
	int i, err;
	Arg *ap;

	Binit(&bout, 1, OWRITE);
	err = 0;
	ap = 0;
	while(argc>1 && argv[1][0]=='-' && argv[1][1]){
		--argc;
		argv++;
		argv[0]++;
		if(argv[0][0] == 'r'){
			repeats = 1;
			if(argv[0][1])
				goto Usage;
			continue;
		}
		if(argv[0][0] == 's'){
			swizzle = 1;
			if(argv[0][1])
				goto Usage;
			continue;
		}
		if(argv[0][0] == 'S'){
			swizzle8 = 1;
			if(argv[0][1])
				goto Usage;
			continue;
		}
		if(argv[0][0] == 'u'){
			flush = 1;
			if(argv[0][1])
				goto Usage;
			continue;
		}
		if(argv[0][0] == 'a'){
			argv[0]++;
			switch(argv[0][0]){
			case 'o':
				abase = 0;
				break;
			case 'd':
				abase = 1;
				break;
			case 'x':
				abase = 2;
				break;
			default:
				goto Usage;
			}
			if(argv[0][1])
				goto Usage;
			continue;
		}
		ap = &arg[narg];
		initarg();
		while(argv[0][0]){
			switch(argv[0][0]){
			case 'c':
				ap->ascii = 1;
				ap->loglen = 0;
				if(argv[0][1] || argv[0][-1]!='-')
					goto Usage;
				break;
			case 'o':
				ap->base = 0;
				break;
			case 'd':
				ap->base = 1;
				break;
			case 'x':
				ap->base = 2;
				break;
			case 'b':
			case '1':
				ap->loglen = 0;
				break;
			case 'w':
			case '2':
				ap->loglen = 1;
				break;
			case 'l':
			case '4':
				ap->loglen = 2;
				break;
			case 'v':
			case '8':
				ap->loglen = 3;
				break;
			default:
			Usage:
   fprint(2, "usage: xd [-u] [-r] [-s] [-a{odx}] [-c|{b1w2l4v8}{odx}] ... file ...\n");
				exits("usage");
			}
			argv[0]++;
		}
		if(ap->ascii)
			ap->fn = fmtc;
		else
			ap->fn = fmt[ap->loglen];
		ap->fmt = dfmt[ap->loglen][ap->base];
		ap->afmt = afmt[ap>arg][abase];
	}
	if(narg == 0)
		initarg();
	if(argc == 1)
		err = xd(0, 0);
	else if(argc == 2)
		err = xd(argv[1], 0);
	else for(i=1; i<argc; i++)
		err |= xd(argv[i], 1);
	exits(err? "error" : 0);
}

void
initarg(void)
{
	Arg *ap;

	ap = &arg[narg++];
	if(narg >= Narg){
		fprint(2, "xd: too many formats (max %d)\n", Narg);
		exits("usage");
	}
	ap->ascii = 0;
	ap->loglen = 2;
	ap->base = 2;
	ap->fn = fmt2;
	ap->fmt = dfmt[ap->loglen][ap->base];
	ap->afmt = afmt[narg>1][abase];
}

int
xd(char *name, int title)
{
	int fd;
	int i, star;
	Arg *ap;
	Biobuf *bp;

	fd = 0;
	if(name){
		bp = Bopen(name, OREAD);
		if(bp == 0){
			fprint(2, "xd: can't open %s\n", name);
			return 1;
		}
	}else{
		bp = &bin;
		Binit(bp, fd, OREAD);
	}
	if(title)
		xprint("%s\n", (long)name);
	addr = 0;
	star = 0;
	while((ndata=Bread(bp, data, 16)) >= 0){
		if(ndata < 16)
			for(i=ndata; i<16; i++)
				data[i] = 0;
		if(swizzle)
			swizz();
		if(swizzle8)
			swizz8();
		if(ndata==16 && repeats){
			if(addr>0 && data[0]==odata[0]){
				for(i=1; i<16; i++)
					if(data[i] != odata[i])
						break;
				if(i == 16){
					addr += 16;
					if(star == 0){
						star++;
						xprint("*\n", 0);
					}
					continue;
				}
			}
			for(i=0; i<16; i++)
				odata[i] = data[i];
			star = 0;
		}
		for(ap=arg; ap<&arg[narg]; ap++){
			xprint(ap->afmt, addr);
			(*ap->fn)(ap->fmt);
			xprint("\n", 0);
			if(flush)
				Bflush(&bout);
		}
		addr += ndata;
		if(ndata<16){
			xprint(afmt[0][abase], addr);
			xprint("\n", 0);
			if(flush)
				Bflush(&bout);
			break;
		}
	}
	Bterm(bp);
	return 0;
}

void
swizz(void)
{
	uchar *p, *q;
	int i;
	uchar swdata[16];

	p = data;
	q = swdata;
	for(i=0; i<16; i++)
		*q++ = *p++;
	p = data;
	q = swdata;
	for(i=0; i<4; i++){
		p[0] = q[3];
		p[1] = q[2];
		p[2] = q[1];
		p[3] = q[0];
		p += 4;
		q += 4;
	}
}

void
swizz8(void)
{
	uchar *p, *q;
	int i;
	uchar swdata[16];

	p = data;
	q = swdata;
	for(i=0; i<16; i++)
		*q++ = *p++;
	p = data;
	q = swdata;
	for(i=0; i<8; i++){
		p[0] = q[7];
		p[1] = q[6];
		p[2] = q[5];
		p[3] = q[4];
		p[4] = q[3];
		p[5] = q[2];
		p[6] = q[1];
		p[7] = q[0];
		p += 8;
		q += 8;
	}
}

void
fmt0(char *f)
{
	int i;
	for(i=0; i<ndata; i++)
		xprint(f, data[i]);
}

void
fmt1(char *f)
{
	int i;
	for(i=0; i<ndata; i+=2)
		xprint(f, (data[i]<<8)|data[i+1]);
}

void
fmt2(char *f)
{
	int i;
	for(i=0; i<ndata; i+=4)
		xprint(f, (u32int)((data[i]<<24)|(data[i+1]<<16)|(data[i+2]<<8)|data[i+3]));
}

void
fmt3(char *f)
{
	int i;
	unsigned long long v;
	for(i=0; i<ndata; i+=8){
		v = (data[i]<<24)|(data[i+1]<<16)|(data[i+2]<<8)|data[i+3];
		v <<= 32;
		v |= (data[i+4]<<24)|(data[i+1+4]<<16)|(data[i+2+4]<<8)|data[i+3+4];
		if(Bprint(&bout, f, v)<0){
			fprint(2, "xd: i/o error\n");
			exits("i/o error");
		}
	}
}

void
fmtc(char *f)
{
	int i;

	USED(f);
	for(i=0; i<ndata; i++)
		switch(data[i]){
		case '\t':
			xprint(cfmt[1][2], (long)"\\t");
			break;
		case '\r':
			xprint(cfmt[1][2], (long)"\\r");
			break;
		case '\n':
			xprint(cfmt[1][2], (long)"\\n");
			break;
		case '\b':
			xprint(cfmt[1][2], (long)"\\b");
			break;
		default:
			if(data[i]>=0x7F || ' '>data[i])
				xprint(cfmt[2][2], data[i]);
			else
				xprint(cfmt[0][2], data[i]);
			break;
		}
}

void
xprint(char *fmt, ulong d)
{
	if(Bprint(&bout, fmt, d)<0){
		fprint(2, "xd: i/o error\n");
		exits("i/o error");
	}
}
