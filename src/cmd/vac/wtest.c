#include "stdinc.h"

enum {
	Nblock = 10000,
	BlockSize = 8*1024,
};

uchar data[Nblock*BlockSize];

void
main(int argc, char *argv[])
{
	VtSession *z;
	int i;
	uchar score[VtScoreSize];
	int start;

	ARGBEGIN{
	}ARGEND

	for(i=0; i<Nblock; i++) {
		if(readn(0, data+i*BlockSize, BlockSize) < BlockSize)
			sysfatal("read failed: %r");
	}

	vtAttach();

	z = vtDial("iolaire2");
	if(z == nil)
		sysfatal("cound not connect to venti");
	if(!vtConnect(z, 0))
		vtFatal("vtConnect: %s", vtGetError());

	print("starting\n");

	start = times(0);

	for(i=0; i<Nblock; i++) {
		if(!vtWrite(z, score, VtDataType, data+i*BlockSize, BlockSize))
			vtFatal("vtWrite failed: %s", vtGetError());
	}

	print("time = %f\n", (times(0) - start)*0.001);

	vtClose(z);
	vtDetach();
}
