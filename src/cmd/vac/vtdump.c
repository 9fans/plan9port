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
VtRoot root;
int ver;
int cmp;
int all;
int find;
uchar fscore[VtScoreSize];
VtConn *z;

int vtgetuint16(uchar *p);
ulong vtgetuint32(uchar *p);
uvlong vtgetuint48(uchar *p);
void usage(void);
void readroot(VtRoot*, uchar *score, char *file);
int dumpdir(Source*, int indent);
int timevtread(VtConn*, uchar*, int, void*, int);

int mainstacksize = 512*1024;

void
threadmain(int argc, char *argv[])
{
	char *host = nil, *pref;
	uchar score[VtScoreSize];
	Source source;
	char *p;
	int n;
	uchar buf[VtMaxLumpSize];

	ARGBEGIN{
	case 'h':
		host = ARGF();
		break;
	case 'c':
		cmp++;
		break;
	case 'f':
		find++;
		p = ARGF();
		if(p == nil || vtparsescore(p, &pref, fscore) < 0 || !pref || strcmp(pref, "vac") != 0)
			usage();
		break;
	case 'a':
		all = 1;
		break;
	}ARGEND

	bout = vtmallocz(sizeof(Biobuf));
	Binit(bout, 1, OWRITE);

	if(argc > 1)
		usage();

	fmtinstall('V', vtscorefmt);
	fmtinstall('H', encodefmt);

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	readroot(&root, score, argv[0]);
	bsize = root.blocksize;
	if(!find) {
		Bprint(bout, "score: %V\n", score);
		Bprint(bout, "name: %s\n", root.name);
		Bprint(bout, "type: %s\n", root.type);
		Bprint(bout, "bsize: %d\n", bsize);
		Bprint(bout, "prev: %V\n", root.prev);
	}

fprint(2, "read...\n");
	n = timevtread(z, root.score, VtDirType, buf, bsize);
	if(n < 0)
		sysfatal("could not read root dir");

fprint(2, "...\n");
	/* fake up top level source */
	memset(&source, 0, sizeof(source));
	memmove(source.score, root.score, VtScoreSize);
	source.psize = bsize;
	source.dsize = bsize;
	source.dir = 1;
	source.active = 1;
	source.depth = 0;
	source.size = n;

	dumpdir(&source, 0);

	Bterm(bout);

	vthangup(z);
	threadexitsall(0);
}

void
sourceprint(Source *s, int indent, int entry)
{
	int i;
	uvlong size;
	int ne;

	for(i=0; i<indent; i++)
		Bprint(bout, " ");
	Bprint(bout, "%4d", entry);
	if(s->active) {
		/* dir size in directory entries */
		if(s->dir) {
			ne = s->dsize/VtEntrySize;
			size = ne*(s->size/s->dsize) + (s->size%s->dsize)/VtEntrySize;
		} else 
			size = s->size;
		if(cmp) {
			Bprint(bout, ": gen: %lud size: %llud",
				s->gen, size);
			if(!s->dir)
				Bprint(bout, ": %V", s->score);
		} else {
			Bprint(bout, ": gen: %lud psize: %d dsize: %d",
				s->gen, s->psize, s->dsize);
			Bprint(bout, " depth: %d size: %llud: %V",
				s->depth, size, s->score);
		}
		
		if(s->reserved)
			Bprint(bout, ": reserved not emtpy");
	}
	Bprint(bout, "\n");
}

int
parse(Source *s, uchar *p)
{
	VtEntry dir;

	memset(s, 0, sizeof(*s));
	if(vtentryunpack(&dir, p, 0) < 0)
		return -1;

	if(!(dir.flags & VtEntryActive))
		return 0;

	s->active = 1;
	s->gen = dir.gen;
	s->psize = dir.psize;
	s->dsize = dir.dsize;
	s->size = dir.size;
	memmove(s->score, dir.score, VtScoreSize);
	if((dir.type&~VtTypeDepthMask) == VtDirType)
		s->dir = 1;
	s->depth = dir.type&VtTypeDepthMask;
	return 0;
}

int
sourceread(Source *s, ulong block, uchar *p, int n)
{
	uchar *buf;
	uchar score[VtScoreSize];
	int i, nn, np, type;
	int elem[VtPointerDepth];

	buf = vtmalloc(VtMaxLumpSize);

	memmove(score, s->score, VtScoreSize);

	np = s->psize/VtScoreSize;
	for(i=0; i<s->depth; i++) {
		elem[i] = block % np;
		block /= np;
	}
	assert(block == 0);

	for(i=s->depth-1; i>=0; i--) {
		nn = timevtread(z, score, (s->dir ? VtDirType : VtDataType)+1+i, buf, s->psize);
		if(nn < 0){
fprint(2, "vtread %V %d: %r\n", score, (s->dir ? VtDirType : VtDataType)+1+i);
			free(buf);
			return -1;
		}

		if((elem[i]+1)*VtScoreSize > nn){
			free(buf);
			return 0;
		}
		memmove(score, buf + elem[i]*VtScoreSize, VtScoreSize);
	}

	if(s->dir)
		type = VtDirType;
	else
		type = VtDataType;

	nn = timevtread(z, score, type, p, n);
	if(nn < 0){
fprint(2, "vtread %V %d: %r\n", score, type);
abort();
		free(buf);
		return -1;
	}
	
	free(buf);
	return nn;
}

void
dumpfilecontents(Source *s)
{
	int nb, lb, i, n;
	uchar buf[VtMaxLumpSize];

	nb = (s->size + s->dsize - 1)/s->dsize;
	lb = s->size%s->dsize;
	for(i=0; i<nb; i++) {
		memset(buf, 0, s->dsize);
		n = sourceread(s, i, buf, s->dsize);
		if(n < 0) {	
			fprint(2, "could not read block: %d: %r\n", i);
			continue;
		}
		if(i < nb-1)
			Bwrite(bout, buf, s->dsize);
		else
			Bwrite(bout, buf, lb);
	}
}

void
dumpfile(Source *s, int indent)
{
	int nb, i, j, n;
	uchar *buf;
	uchar score[VtScoreSize];

	buf = vtmalloc(VtMaxLumpSize);
	nb = (s->size + s->dsize - 1)/s->dsize;
	for(i=0; i<nb; i++) {
		memset(buf, 0, s->dsize);
		n = sourceread(s, i, buf, s->dsize);
		if(n < 0) {	
			fprint(2, "could not read block: %d: %r\n", i);
			continue;
		}
		for(j=0; j<indent; j++)
			Bprint(bout, " ");
		sha1(buf, n, score, nil);		
		Bprint(bout, "%4d: size: %ud: %V\n", i, n, score);
	}
	vtfree(buf);
}

int
dumpdir(Source *s, int indent)
{
	int pb, ne, nb, i, j, n, entry;
	Source ss;
	uchar *buf;

	buf = vtmalloc(VtMaxLumpSize);
	pb = s->dsize/VtEntrySize;
	ne = pb*(s->size/s->dsize) + (s->size%s->dsize)/VtEntrySize;
	nb = (s->size + s->dsize - 1)/s->dsize;
	for(i=0; i<nb; i++) {
		memset(buf, 0, s->dsize);
		n = sourceread(s, i, buf, s->dsize);
		if(n < 0) {	
			fprint(2, "could not read block: %d: %r\n", i);
			continue;
		}
		for(j=0; j<pb; j++) {
			entry = i*pb + j;
			if(entry >= ne)
				break;
			parse(&ss, buf + j * VtEntrySize);

			if(!find)
				sourceprint(&ss, indent, entry);
			else if(memcmp(ss.score, fscore, VtScoreSize) == 0) {
				dumpfilecontents(&ss);
				free(buf);
				return -1;
			}

			if(ss.dir) {
				if(dumpdir(&ss, indent+1) < 0){
					free(buf);
					return -1;
				}
			} else if(all)
				dumpfile(&ss, indent+1);
		}
	}
	free(buf);
	return 0;
}

void
usage(void)
{
	fprint(2, "%s: [file]\n", argv0);
	threadexits("usage");
}

void
readroot(VtRoot *root, uchar *score, char *file)
{
	int fd;
	char *pref;
	char buf[VtRootSize];
	int n, nn;

	if(file == 0)
		fd = 0;
	else {
		fd = open(file, OREAD);
		if(fd < 0)
			sysfatal("could not open file: %s: %r\n", file);
	}
	n = readn(fd, buf, sizeof(buf)-1);
	if(n < 0)
		sysfatal("read failed: %r\n");
	if(n==0 || buf[n-1] != '\n')
		sysfatal("not a root file");
	buf[n-1] = 0;
	close(fd);

	if(vtparsescore(buf, &pref, score) < 0){
		sysfatal("not a root file");
	}
	nn = timevtread(z, score, VtRootType, buf, VtRootSize);
	if(nn < 0)
		sysfatal("cannot read root %V", score);
	if(vtrootunpack(root, buf) < 0)
		sysfatal("cannot parse root: %r");
}

int
timevtread(VtConn *z, uchar *score, int type, void *buf, int nbuf)
{
/*
	ulong t0, t1;
	int n;

	t0 = nsec();
	n = vtread(z, score, type, buf, nbuf);
	t1 = nsec();
	fprint(2, "read %V: %.6f seconds\n", score, (t1-t0)/1.e9);
	return n;
*/
	return vtread(z, score, type, buf, nbuf);
}
