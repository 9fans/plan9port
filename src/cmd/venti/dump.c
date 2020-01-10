#include <u.h>
#include <libc.h>
#include <bio.h>
#include <venti.h>
#include <libsec.h>
#include <thread.h>

enum
{
	// XXX What to do here?
	VtMaxLumpSize = 65535,
};

VtConn *z;
char *host;

void
usage(void)
{
	fprint(2, "usage: venti/dump [-h host] score\n");
	threadexitsall("usage");
}

Biobuf bout;
char spaces[256];

void
dump(int indent, uchar *score, int type)
{
	int i, n;
	uchar *buf;
	VtEntry e;
	VtRoot root;

	if(spaces[0] == 0)
		memset(spaces, ' ', sizeof spaces-1);

	buf = vtmallocz(VtMaxLumpSize);
	if(memcmp(score, vtzeroscore, VtScoreSize) == 0)
		n = 0;
	else
		n = vtread(z, score, type, buf, VtMaxLumpSize);
	if(n < 0){
		Bprint(&bout, "%.*serror reading %V: %r\n", indent*4, spaces, score);
		goto out;
	}
	switch(type){
	case VtRootType:
		if(vtrootunpack(&root, buf) < 0){
			Bprint(&bout, "%.*serror unpacking root %V: %r\n", indent*4, spaces, score);
			goto out;
		}
		Bprint(&bout, "%.*s%V root name=%s type=%s prev=%V bsize=%ld\n",
			indent*4, spaces, score, root.name, root.type, root.prev, root.blocksize);
		dump(indent+1, root.score, VtDirType);
		break;

	case VtDirType:
		Bprint(&bout, "%.*s%V dir n=%d\n", indent*4, spaces, score, n);
		for(i=0; i*VtEntrySize<n; i++){
			if(vtentryunpack(&e, buf, i) < 0){
				Bprint(&bout, "%.*s%d: cannot unpack\n", indent+1, spaces, i);
				continue;
			}
			Bprint(&bout, "%.*s%d: gen=%#lux psize=%ld dsize=%ld type=%d flags=%#x size=%llud score=%V\n",
				(indent+1)*4, spaces, i, e.gen, e.psize, e.dsize, e.type, e.flags, e.size, e.score);
			dump(indent+2, e.score, e.type);
		}
		break;

	case VtDataType:
		Bprint(&bout, "%.*s%V data n=%d", indent*4, spaces, score, n);
		for(i=0; i<n; i++){
			if(i%16 == 0)
				Bprint(&bout, "\n%.*s", (indent+1)*4, spaces);
			Bprint(&bout, " %02x", buf[i]);
		}
		Bprint(&bout, "\n");
		break;

	default:
		if(type >= VtDirType)
			Bprint(&bout, "%.*s%V dir+%d\n", indent*4, spaces, score, type-VtDirType);
		else
			Bprint(&bout, "%.*s%V data+%d\n", indent*4, spaces, score, type-VtDirType);
		for(i=0; i<n; i+=VtScoreSize)
			dump(indent+1, buf+i, type-1);
		break;
	}
out:
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

	ARGBEGIN{
	case 'h':
		host = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	if(vtparsescore(argv[0], &prefix, score) < 0)
		sysfatal("could not parse score: %r");

	buf = vtmallocz(VtMaxLumpSize);
	z = vtdial(host);
	if(z == nil)
		sysfatal("dialing venti: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect src: %r");

	for(type=0; type<VtMaxType; type++){
		n = vtread(z, score, type, buf, VtMaxLumpSize);
		if(n >= 0)
			goto havetype;
	}
	sysfatal("cannot find block %V", score);

havetype:
	Binit(&bout, 1, OWRITE);
	dump(0, score, type);
	Bflush(&bout);
	threadexitsall(nil);
}
