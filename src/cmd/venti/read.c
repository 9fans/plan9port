#include "stdinc.h"
#include "dat.h"
#include "fns.h"

char *host;

void
usage(void)
{
	fprint(2, "usage: read [-h host] score [type]\n");
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
	int type, n;
	uchar score[VtScoreSize];
	uchar *buf;
	VtConn *z;

	ARGBEGIN{
	case 'h':
		host = EARGF(usage());
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 1 && argc != 2)
		usage();


	fmtinstall('V', vtscorefmt);

	if(parsescore(score, argv[0], strlen(argv[0])) < 0)
		sysfatal("could not parse score '%s': %r", argv[0]);

	buf = vtmallocz(VtMaxLumpSize);

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	if(argc == 1){
		n = -1;
		for(type=0; type<VtMaxType; type++){
			n = vtread(z, score, type, buf, VtMaxLumpSize);
			if(n >= 0){
				fprint(2, "venti/read%s%s %V %d\n", host ? " -h" : "", host ? host : "",
					score, type);
				break;
			}
		}
	}else{
		type = atoi(argv[1]);
		n = vtread(z, score, type, buf, VtMaxLumpSize);
	}
	vthangup(z);
	if(n < 0)
		sysfatal("could not read block: %r");
	if(write(1, buf, n) != n)
		sysfatal("write: %r");

	threadexitsall(0);
}
