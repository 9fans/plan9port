#include "stdinc.h"
#include "dat.h"
#include "fns.h"

int fast;

VtConn *zsrc, *zdst;

void
usage(void)
{
	fprint(2, "usage: copy src-host dst-host score [type]\n");
	threadexitsall("usage");
}

int
parsescore(uchar *score, char *buf, int n)
{
	int i, c;

	memset(score, 0, VtScoreSize);

	if(n < VtScoreSize*2)
		return -1;
	for(i=0; i<VtScoreSize*2; i++) {
		if(buf[i] >= '0' && buf[i] <= '9')
			c = buf[i] - '0';
		else if(buf[i] >= 'a' && buf[i] <= 'f')
			c = buf[i] - 'a' + 10;
		else if(buf[i] >= 'A' && buf[i] <= 'F')
			c = buf[i] - 'A' + 10;
		else {
			return -1;
		}

		if((i & 1) == 0)
			c <<= 4;
	
		score[i>>1] |= c;
	}
	return 0;
}

void
walk(uchar score[VtScoreSize], uint type, int base)
{
	int i, n, sub;
	uchar *buf;
	VtEntry e;
	VtRoot root;

	if(memcmp(score, vtzeroscore, VtScoreSize) == 0)
		return;

	buf = vtmallocz(VtMaxLumpSize);
	if(fast && vtread(zdst, score, type, buf, VtMaxLumpSize) >= 0){
		fprint(2, "skip %V\n", score);
		free(buf);
		return;
	}

	n = vtread(zsrc, score, type, buf, VtMaxLumpSize);
	if(n < 0){
		fprint(2, "warning: could not read block %V %d: %r", score, type);
		return;
	}

	switch(type){
	case VtRootType:
		if(vtrootunpack(&root, buf) < 0){
			fprint(2, "warning: could not unpack root in %V %d\n", score, type);
			break;
		}
		walk(root.score, VtDirType, 0);
		walk(root.prev, VtRootType, 0);
		break;

	case VtDirType:
		for(i=0; i<n/VtEntrySize; i++){
			if(vtentryunpack(&e, buf, i) < 0){
				fprint(2, "warning: could not unpack entry #%d in %V %d\n", i, score, type);
				continue;
			}
			if(!(e.flags & VtEntryActive))
				continue;
			if(e.flags&VtEntryDir)
				base = VtDirType;
			else
				base = VtDataType;
			sub = base | ((e.flags&VtEntryDepthMask)>>VtEntryDepthShift);
			walk(e.score, sub, base);
		}
		break;

	case VtDataType:
		break;

	default:	/* pointers */
		for(i=0; i<n; i+=VtScoreSize)
			if(memcmp(buf+i, vtzeroscore, VtScoreSize) != 0)
				walk(buf+i, type-1, base);
		break;
	}

	if(vtwrite(zdst, score, type, buf, n) < 0)
		fprint(2, "warning: could not write block %V %d: %r", score, type);
	free(buf);
}

void
threadmain(int argc, char *argv[])
{
	int type, n;
	uchar score[VtScoreSize];
	uchar *buf;

	ARGBEGIN{
	case 'f':
		fast = 1;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 3 && argc != 4)
		usage();

	fmtinstall('V', vtscorefmt);

	if(parsescore(score, argv[2], strlen(argv[2]) < 0))
		sysfatal("could not parse score: %r");

	buf = vtmallocz(VtMaxLumpSize);

	zsrc = vtdial(argv[0]);
	if(zsrc == nil)
		sysfatal("could not dial src server: %r");
	if(vtconnect(zsrc) < 0)
		sysfatal("vtconnect src: %r");

	zdst = vtdial(argv[1]);
	if(zdst == nil)
		sysfatal("could not dial dst server: %r");
	if(vtconnect(zdst) < 0)
		sysfatal("vtconnect dst: %r");

	if(argc == 4){
		type = atoi(argv[3]);
		n = vtread(zsrc, score, type, buf, VtMaxLumpSize);
		if(n < 0)
			sysfatal("could not read block: %r");
	}else{
		for(type=0; type<VtMaxType; type++){
			n = vtread(zsrc, score, type, buf, VtMaxLumpSize);
			if(n >= 0)
				break;
		}
		if(type == VtMaxType)
			sysfatal("could not find block %V of any type", score);
	}

	walk(score, type, VtDirType);

	if(vtsync(zdst) < 0)
		sysfatal("could not sync dst server: %r");

	threadexitsall(0);
}
