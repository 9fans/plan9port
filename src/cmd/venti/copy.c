#include <u.h>
#include <libc.h>
#include <venti.h>
#include <libsec.h>
#include <thread.h>

int changes;
int rewrite;
int ignoreerrors;
int fast;
int verbose;
VtConn *zsrc, *zdst;

void
usage(void)
{
	fprint(2, "usage: copy [-fir] [-t type] srchost dsthost score\n");
	threadexitsall("usage");
}

void
walk(uchar score[VtScoreSize], uint type, int base)
{
	int i, n;
	uchar *buf;
	VtEntry e;
	VtRoot root;

	if(memcmp(score, vtzeroscore, VtScoreSize) == 0)
		return;

	buf = vtmallocz(VtMaxLumpSize);
	if(fast && vtread(zdst, score, type, buf, VtMaxLumpSize) >= 0){
		if(verbose)
			fprint(2, "skip %V\n", score);
		free(buf);
		return;
	}

	n = vtread(zsrc, score, type, buf, VtMaxLumpSize);
	if(n < 0){
		if(rewrite){
			changes++;
			memmove(score, vtzeroscore, VtScoreSize);
		}else if(!ignoreerrors)
			sysfatal("reading block %V (type %d): %r", type, score);
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
		vtrootpack(&root, buf);	/* walk might have changed score */
		break;

	case VtDirType:
		for(i=0; i<n/VtEntrySize; i++){
			if(vtentryunpack(&e, buf, i) < 0){
				fprint(2, "warning: could not unpack entry #%d in %V %d\n", i, score, type);
				continue;
			}
			if(!(e.flags & VtEntryActive))
				continue;
			walk(e.score, e.type, e.type&VtTypeBaseMask);
			vtentrypack(&e, buf, i);
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

	if(vtwrite(zdst, score, type, buf, n) < 0){
		/* figure out score for better error message */
		/* can't use input argument - might have changed contents */
		n = vtzerotruncate(type, buf, n);
		sha1(buf, n, score, nil);
		sysfatal("writing block %V (type %d): %r", score, type);
	}
	free(buf);
}

void
threadmain(int argc, char *argv[])
{
	int type, n;
	uchar score[VtScoreSize];
	uchar *buf;
	char *prefix;

	fmtinstall('F', vtfcallfmt);
	fmtinstall('V', vtscorefmt);

	type = -1;
	ARGBEGIN{
	case 'f':
		fast = 1;
		break;
	case 'i':
		if(rewrite)
			usage();
		ignoreerrors = 1;
		break;
	case 'r':
		if(ignoreerrors)
			usage();
		rewrite = 1;
		break;
	case 't':
		type = atoi(EARGF(usage()));
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 3)
		usage();

	if(vtparsescore(argv[2], &prefix, score) < 0)
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

	if(type != -1){
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
	if(changes)
		print("%s:%V (%d pointers rewritten)\n", prefix, score, changes);

	if(vtsync(zdst) < 0)
		sysfatal("could not sync dst server: %r");

	threadexitsall(0);
}
