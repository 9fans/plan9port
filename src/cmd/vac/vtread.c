#include "stdinc.h"
#include <bio.h>

typedef struct Source Source;

struct Source
{
	ulong gen;
	int psize;
	int dsize;
	int dir;
	int active;
	int depth;
	uvlong size;
	uchar score[VtScoreSize];
	int reserved;
};

int bsize;
Biobuf *bout;
VtRootLump root;
int ver;
int cmp;
int all;
int find;
uchar fscore[VtScoreSize];
int dirSize;
void (*parse)(Source*, uchar*);
VtSession *z;

int vtGetUint16(uchar *p);
ulong vtGetUint32(uchar *p);
uvlong vtGetUint48(uchar *p);
void usage(void);
int parseScore(uchar *score, char *buf, int n);
void readRoot(VtRootLump*, uchar *score, char *file);
void parse1(Source*, uchar*);
void parse2(Source*, uchar*);
int dumpDir(Source*, int indent);

void
main(int argc, char *argv[])
{
	char *host = nil;
	uchar score[VtScoreSize];
	uchar buf[VtMaxLumpSize];
	int type;
	int n;
	
	type = VtDataType;

	ARGBEGIN{
	case 't':
		type = atoi(ARGF());
		break;
	}ARGEND

	vtAttach();

	bout = vtMemAllocZ(sizeof(Biobuf));
	Binit(bout, 1, OWRITE);

	if(argc != 1)
		usage();

	vtAttach();

	fmtinstall('V', vtScoreFmt);
	fmtinstall('R', vtErrFmt);

	z = vtDial(host);
	if(z == nil)
		vtFatal("could not connect to server: %s", vtGetError());

	if(!vtConnect(z, 0))
		sysfatal("vtConnect: %r");

	if(!parseScore(score, argv[0], strlen(argv[0])))
		vtFatal("could not parse score: %s", vtGetError());

	n = vtRead(z, score, type, buf, VtMaxLumpSize);
	if(n < 0)
		vtFatal("could not read block: %s", vtGetError());
	Bwrite(bout, buf, n);

	Bterm(bout);

	vtClose(z);
	vtDetach();
	exits(0);
}

void
usage(void)
{
	fprint(2, "%s: -t type score\n", argv0);
	exits("usage");
}

int
parseScore(uchar *score, char *buf, int n)
{
	int i, c;

	memset(score, 0, VtScoreSize);

	if(n < VtScoreSize*2)
		return 0;
	for(i=0; i<VtScoreSize*2; i++) {
		if(buf[i] >= '0' && buf[i] <= '9')
			c = buf[i] - '0';
		else if(buf[i] >= 'a' && buf[i] <= 'f')
			c = buf[i] - 'a' + 10;
		else if(buf[i] >= 'A' && buf[i] <= 'F')
			c = buf[i] - 'A' + 10;
		else {
			return 0;
		}

		if((i & 1) == 0)
			c <<= 4;
	
		score[i>>1] |= c;
	}
	return 1;
}
