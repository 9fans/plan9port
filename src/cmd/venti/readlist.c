#include <u.h>
#include <libc.h>
#include <thread.h>
#include <venti.h>
#include <bio.h>

enum
{
	// XXX What to do here?
	VtMaxLumpSize = 65535,
};

char *host;
Biobuf b;
VtConn *z;
uchar *buf;
void run(Biobuf*);
int nn;

void
usage(void)
{
	fprint(2, "usage: readlist [-h host] list\n");
	threadexitsall("usage");
}

int
parsescore(uchar *score, char *buf, int n)
{
	int i, c;

	memset(score, 0, VtScoreSize);

	if(n != VtScoreSize*2){
		werrstr("score wrong length %d", n);
		return -1;
	}
	for(i=0; i<VtScoreSize*2; i++) {
		if(buf[i] >= '0' && buf[i] <= '9')
			c = buf[i] - '0';
		else if(buf[i] >= 'a' && buf[i] <= 'f')
			c = buf[i] - 'a' + 10;
		else if(buf[i] >= 'A' && buf[i] <= 'F')
			c = buf[i] - 'A' + 10;
		else {
			c = buf[i];
			werrstr("bad score char %d '%c'", c, c);
			return -1;
		}

		if((i & 1) == 0)
			c <<= 4;

		score[i>>1] |= c;
	}
	return 0;
}

void
threadmain(int argc, char *argv[])
{
	int fd, i;

	ARGBEGIN{
	case 'h':
		host = EARGF(usage());
		break;
	default:
		usage();
		break;
	}ARGEND

	fmtinstall('V', vtscorefmt);
	buf = vtmallocz(VtMaxLumpSize);
	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	if(argc == 0){
		Binit(&b, 0, OREAD);
		run(&b);
	}else{
		for(i=0; i<argc; i++){
			if((fd = open(argv[i], OREAD)) < 0)
				sysfatal("open %s: %r", argv[i]);
			Binit(&b, fd, OREAD);
			run(&b);
		}
	}
	threadexitsall(nil);
}

void
run(Biobuf *b)
{
	char *p, *f[10];
	int nf;
	uchar score[20];
	int type, n;

	while((p = Brdline(b, '\n')) != nil){
		p[Blinelen(b)-1] = 0;
		nf = tokenize(p, f, nelem(f));
		if(nf != 2)
			sysfatal("syntax error in work list");
		if(parsescore(score, f[0], strlen(f[0])) < 0)
			sysfatal("bad score %s in work list", f[0]);
		type = atoi(f[1]);
		n = vtread(z, score, type, buf, VtMaxLumpSize);
		if(n < 0)
			sysfatal("could not read %s %s: %r", f[0], f[1]);
		/* write(1, buf, n); */
		if(++nn%1000 == 0)
			print("%d...", nn);
	}
}
