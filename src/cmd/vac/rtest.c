#include "stdinc.h"

enum {
	Nblock = 300000,
	BlockSize = 8*1024,
};

uchar data[Nblock*VtScoreSize];
int rflag;
int nblock = 10000;
int perm[Nblock];

void
main(int argc, char *argv[])
{
	VtSession *z;
	int i, j, t;
	int start;
	uchar buf[BlockSize];

	srand(time(0));

	ARGBEGIN{
	case 'r':
		rflag++;
		break;
	case 'n':
		nblock = atoi(ARGF());
		break;
	}ARGEND

	for(i=0; i<nblock; i++)
		perm[i] = i;

	if(rflag) {
		for(i=0; i<nblock; i++) {
			j = nrand(nblock);
			t = perm[j];
			perm[j] = perm[i];
			perm[i] = t;
		}
	}

	if(readn(0, data, VtScoreSize*nblock) < VtScoreSize*nblock)
		sysfatal("read failed: %r");

	vtAttach();

	z = vtDial("iolaire2");
	if(z == nil)
		sysfatal("cound not connect to venti");
	if(!vtConnect(z, 0))
		vtFatal("vtConnect: %s", vtGetError());

	print("starting\n");

	start = times(0);

	if(rflag && nblock > 10000)
		nblock = 10000;

	for(i=0; i<nblock; i++) {
		if(vtRead(z, data+perm[i]*VtScoreSize, VtDataType, buf, BlockSize) < 0)
			vtFatal("vtRead failed: %d: %s", i, vtGetError());
	}

	print("time = %f\n", (times(0) - start)*0.001);

	vtClose(z);
	vtDetach();
}
