/*
 * vbackup [-Dnv] fspartition [score]
 *
 * Copy a file system to a disk image stored on Venti.
 * Prints a vnfs config line for the copied image.
 *
 *	-D	print debugging
 *	-f	'fast' writes - skip write if block exists on server
 *	-i	read old scores incrementally
 *	-m	set mount name
 *	-M	set mount place
 *	-n	nop -- don't actually write blocks
 *	-s	print status updates
 *	-v	print debugging trace
 *	-w	write parallelism
 *
 * If score is given on the command line, it should be the
 * score from a previous vbackup on this fspartition.
 * In this mode, only the new blocks are stored to Venti.
 * The result is still a complete image, but requires many
 * fewer Venti writes in the common case.
 *
 * This program is structured as three processes connected
 * by buffered queues:
 *
 * 	fsysproc | cmpproc | ventiproc
 *
 * Fsysproc reads the disk and queues the blocks.
 * Cmpproc compares the blocks against the SHA1 hashes
 * in the old image, if any.  It discards the unchanged blocks
 * and queues the changed ones.  Ventiproc writes blocks to Venti.
 *
 * There is a fourth proc, statusproc, which prints status
 * updates about how the various procs are progressing.
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <libsec.h>
#include <venti.h>
#include <diskfs.h>
#include "queue.h"

enum
{
	STACK = 32768
};

typedef struct WriteReq WriteReq;
struct WriteReq
{
	Packet *p;
	uint type;
	uchar score[VtScoreSize];
};

Biobuf	bscores;		/* biobuf filled with block scores */
int		debug;		/* debugging flag (not used) */
Disk*	disk;			/* disk being backed up */
RWLock	endlk;		/* silly synchonization */
int		errors;		/* are we exiting with an error status? */
int		fastwrites;		/* do not write blocks already on server */
int		fsscanblock;	/* last block scanned */
Fsys*	fsys;			/* file system being backed up */
int		incremental;	/* use vscores rather than bscores */
int		nchange;		/* number of changed blocks */
int		nop;			/* don't actually send blocks to venti */
int		nskip;		/* number of blocks skipped (already on server) */
int		nwritethread;	/* number of write-behind threads */
Queue*	qcmp;		/* queue fsys->cmp */
Queue*	qventi;		/* queue cmp->venti */
int		statustime;	/* print status every _ seconds */
int		verbose;		/* print extra stuff */
VtFile*	vfile;			/* venti file being written */
VtFile*	vscores;		/* venti file with block scores */
Channel*	writechan;	/* chan(WriteReq) */
VtConn*	z;			/* connection to venti */
VtCache*	zcache;		/* cache of venti blocks */
uchar*	zero;			/* blocksize zero bytes */

int nsend, nrecv;

void		cmpproc(void*);
void		fsysproc(void*);
void		statusproc(void*);
void		ventiproc(void*);
int		timefmt(Fmt*);
char*	guessmountplace(char *dev);

void
usage(void)
{
	fprint(2, "usage: vbackup [-DVnv] [-m mtpt] [-M mtpl] [-s secs] [-w n] disk [score]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	char *pref, *mountname, *mountplace;
	uchar score[VtScoreSize], prev[VtScoreSize];
	int i, fd, csize;
	vlong bsize;
	Tm tm;
	VtEntry e;
	VtBlock *b;
	VtCache *c;
	VtRoot root;
	char *tmp, *tmpnam;

	fmtinstall('F', vtfcallfmt);
	fmtinstall('H', encodefmt);
	fmtinstall('T', timefmt);
	fmtinstall('V', vtscorefmt);

	mountname = sysname();
	mountplace = nil;
	ARGBEGIN{
	default:
		usage();
		break;
	case 'D':
		debug++;
		break;
	case 'V':
		chattyventi = 1;
		break;
	case 'f':
		fastwrites = 1;
		break;
	case 'i':
		incremental = 1;
		break;
	case 'm':
		mountname = EARGF(usage());
		break;
	case 'M':
		mountplace = EARGF(usage());
		i = strlen(mountplace);
		if(i > 0 && mountplace[i-1] == '/')
			mountplace[i-1] = 0;
		break;
	case 'n':
		nop = 1;
		break;
	case 's':
		statustime = atoi(EARGF(usage()));
		break;
	case 'v':
		verbose = 1;
		break;
	case 'w':
		nwritethread = atoi(EARGF(usage()));
		break;
	}ARGEND

	if(argc != 1 && argc != 2)
		usage();

	if(statustime)
		print("# %T vbackup %s %s\n", argv[0], argc>=2 ? argv[1] : "");

	/*
	 * open fs
	 */
	if((disk = diskopenfile(argv[0])) == nil)
		sysfatal("diskopen: %r");
	if((disk = diskcache(disk, 32768, 2*MAXQ+16)) == nil)
		sysfatal("diskcache: %r");
	if((fsys = fsysopen(disk)) == nil)
		sysfatal("fsysopen: %r");

	/*
	 * connect to venti
	 */
	if((z = vtdial(nil)) == nil)
		sysfatal("vtdial: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	/*
	 * set up venti block cache
	 */
	zero = vtmallocz(fsys->blocksize);
	bsize = fsys->blocksize;
	csize = 50;	/* plenty; could probably do with 5 */

	if(verbose)
		fprint(2, "cache %d blocks\n", csize);
	c = vtcachealloc(z, bsize*csize);
	zcache = c;

	/*
	 * parse starting score
	 */
	memset(prev, 0, sizeof prev);
	if(argc == 1){
		vfile = vtfilecreateroot(c, (fsys->blocksize/VtScoreSize)*VtScoreSize,
			fsys->blocksize, VtDataType);
		if(vfile == nil)
			sysfatal("vtfilecreateroot: %r");
		vtfilelock(vfile, VtORDWR);
		if(vtfilewrite(vfile, zero, 1, bsize*fsys->nblock-1) != 1)
			sysfatal("vtfilewrite: %r");
		if(vtfileflush(vfile) < 0)
			sysfatal("vtfileflush: %r");
	}else{
		if(vtparsescore(argv[1], &pref, score) < 0)
			sysfatal("bad score: %r");
		if(pref!=nil && strcmp(pref, fsys->type) != 0)
			sysfatal("score is %s but fsys is %s", pref, fsys->type);
		b = vtcacheglobal(c, score, VtRootType, VtRootSize);
		if(b){
			if(vtrootunpack(&root, b->data) < 0)
				sysfatal("bad root: %r");
			if(strcmp(root.type, fsys->type) != 0)
				sysfatal("root is %s but fsys is %s", root.type, fsys->type);
			memmove(prev, score, VtScoreSize);
			memmove(score, root.score, VtScoreSize);
			vtblockput(b);
		}
		b = vtcacheglobal(c, score, VtDirType, VtEntrySize);
		if(b == nil)
			sysfatal("vtcacheglobal %V: %r", score);
		if(vtentryunpack(&e, b->data, 0) < 0)
			sysfatal("%V: vtentryunpack failed", score);
		if(verbose)
			fprint(2, "entry: size %llud psize %d dsize %d\n",
				e.size, e.psize, e.dsize);
		vtblockput(b);
		if((vfile = vtfileopenroot(c, &e)) == nil)
			sysfatal("vtfileopenroot: %r");
		vtfilelock(vfile, VtORDWR);
		if(e.dsize != bsize)
			sysfatal("file system block sizes don't match %d %lld", e.dsize, bsize);
		if(e.size != fsys->nblock*bsize)
			sysfatal("file system block counts don't match %lld %lld", e.size, fsys->nblock*bsize);
	}

	tmpnam = nil;
	if(incremental){
		if(vtfilegetentry(vfile, &e) < 0)
			sysfatal("vtfilegetentry: %r");
		if((vscores = vtfileopenroot(c, &e)) == nil)
			sysfatal("vtfileopenroot: %r");
		vtfileunlock(vfile);
	}else{
		/*
		 * write scores of blocks into temporary file
		 */
		if((tmp = getenv("TMP")) != nil){
			/* okay, good */
		}else if(access("/var/tmp", 0) >= 0)
			tmp = "/var/tmp";
		else
			tmp = "/tmp";
		tmpnam = smprint("%s/vbackup.XXXXXX", tmp);
		if(tmpnam == nil)
			sysfatal("smprint: %r");

		if((fd = opentemp(tmpnam, ORDWR|ORCLOSE)) < 0)
			sysfatal("opentemp %s: %r", tmpnam);
		if(statustime)
			print("# %T reading scores into %s\n", tmpnam);
		if(verbose)
			fprint(2, "read scores into %s...\n", tmpnam);

		Binit(&bscores, fd, OWRITE);
		for(i=0; i<fsys->nblock; i++){
			if(vtfileblockscore(vfile, i, score) < 0)
				sysfatal("vtfileblockhash %d: %r", i);
			if(Bwrite(&bscores, score, VtScoreSize) != VtScoreSize)
				sysfatal("Bwrite: %r");
		}
		Bterm(&bscores);
		vtfileunlock(vfile);

		/*
		 * prep scores for rereading
		 */
		seek(fd, 0, 0);
		Binit(&bscores, fd, OREAD);
	}

	/*
	 * start the main processes
	 */
	if(statustime)
		print("# %T starting procs\n");
	qcmp = qalloc();
	qventi = qalloc();

	rlock(&endlk);
	proccreate(fsysproc, nil, STACK);
	rlock(&endlk);
	proccreate(ventiproc, nil, STACK);
	rlock(&endlk);
	proccreate(cmpproc, nil, STACK);
	if(statustime){
		rlock(&endlk);
		proccreate(statusproc, nil, STACK);
	}

	/*
	 * wait for processes to finish
	 */
	wlock(&endlk);

	qfree(qcmp);
	qfree(qventi);

	if(statustime)
		print("# %T procs exited: %d of %d %d-byte blocks changed, "
			"%d read, %d written, %d skipped, %d copied\n",
			nchange, fsys->nblock, fsys->blocksize,
			vtcachenread, vtcachenwrite, nskip, vtcachencopy);

	/*
	 * prepare root block
	 */
	if(incremental)
		vtfileclose(vscores);
	vtfilelock(vfile, -1);
	if(vtfileflush(vfile) < 0)
		sysfatal("vtfileflush: %r");
	if(vtfilegetentry(vfile, &e) < 0)
		sysfatal("vtfilegetentry: %r");
	vtfileunlock(vfile);
	vtfileclose(vfile);

	b = vtcacheallocblock(c, VtDirType, VtEntrySize);
	if(b == nil)
		sysfatal("vtcacheallocblock: %r");
	vtentrypack(&e, b->data, 0);
	if(vtblockwrite(b) < 0)
		sysfatal("vtblockwrite: %r");

	memset(&root, 0, sizeof root);
	strecpy(root.name, root.name+sizeof root.name, argv[0]);
	strecpy(root.type, root.type+sizeof root.type, fsys->type);
	memmove(root.score, b->score, VtScoreSize);
	root.blocksize = fsys->blocksize;
	memmove(root.prev, prev, VtScoreSize);
	vtblockput(b);

	b = vtcacheallocblock(c, VtRootType, VtRootSize);
	if(b == nil)
		sysfatal("vtcacheallocblock: %r");
	vtrootpack(&root, b->data);
	if(vtblockwrite(b) < 0)
		sysfatal("vtblockwrite: %r");

	tm = *localtime(time(0));
	tm.year += 1900;
	tm.mon++;
	if(mountplace == nil)
		mountplace = guessmountplace(argv[0]);
	print("mount /%s/%d/%02d%02d%s %s:%V %d/%02d%02d/%02d%02d\n",
		mountname, tm.year, tm.mon, tm.mday,
		mountplace,
		root.type, b->score,
		tm.year, tm.mon, tm.mday, tm.hour, tm.min);
	print("# %T %s %s:%V\n", argv[0], root.type, b->score);
	if(statustime)
		print("# %T venti sync\n");
	vtblockput(b);
	if(vtsync(z) < 0)
		sysfatal("vtsync: %r");
	if(statustime)
		print("# %T synced\n");

	fsysclose(fsys);
	diskclose(disk);
	vtcachefree(zcache);

	// Vtgoodbye hangs on Linux - not sure why.
	// Probably vtfcallrpc doesn't quite get the
	// remote hangup right.  Also probably related
	// to the vtrecvproc problem below.
	// vtgoodbye(z);

	// Leak here, because I can't seem to make
	// the vtrecvproc exit.
	// vtfreeconn(z);

	free(tmpnam);
	z = nil;
	zcache = nil;
	fsys = nil;
	disk = nil;
	threadexitsall(nil);
}

void
fsysproc(void *dummy)
{
	u32int i;
	Block *db;

	USED(dummy);

	for(i=0; i<fsys->nblock; i++){
		fsscanblock = i;
		if((db = fsysreadblock(fsys, i)) != nil)
			qwrite(qcmp, db, i);
	}
	fsscanblock = i;
	qclose(qcmp);

	if(statustime)
		print("# %T fsys proc exiting\n");
	runlock(&endlk);
}

void
cmpproc(void *dummy)
{
	uchar *data;
	Block *db;
	u32int bno, bsize;
	uchar score[VtScoreSize];
	uchar score1[VtScoreSize];

	USED(dummy);

	if(incremental)
		vtfilelock(vscores, VtOREAD);
	bsize = fsys->blocksize;
	while((db = qread(qcmp, &bno)) != nil){
		data = db->data;
		sha1(data, vtzerotruncate(VtDataType, data, bsize), score, nil);
		if(incremental){
			if(vtfileblockscore(vscores, bno, score1) < 0)
				sysfatal("cmpproc vtfileblockscore %d: %r", bno);
		}else{
			if(Bseek(&bscores, (vlong)bno*VtScoreSize, 0) < 0)
				sysfatal("cmpproc Bseek: %r");
			if(Bread(&bscores, score1, VtScoreSize) != VtScoreSize)
				sysfatal("cmpproc Bread: %r");
		}
		if(memcmp(score, score1, VtScoreSize) != 0){
			nchange++;
			if(verbose)
				print("# block %ud: old %V new %V\n", bno, score1, score);
			qwrite(qventi, db, bno);
		}else
			blockput(db);
	}
	qclose(qventi);
	if(incremental)
		vtfileunlock(vscores);
	if(statustime)
		print("# %T cmp proc exiting\n");
	runlock(&endlk);
}

void
writethread(void *v)
{
	WriteReq wr;
	char err[ERRMAX];

	USED(v);

	while(recv(writechan, &wr) == 1){
		nrecv++;
		if(wr.p == nil)
			break;

		if(fastwrites && vtread(z, wr.score, wr.type, nil, 0) < 0){
			rerrstr(err, sizeof err);
			if(strstr(err, "read too small")){	/* already exists */
				nskip++;
				packetfree(wr.p);
				continue;
			}
		}
		if(vtwritepacket(z, wr.score, wr.type, wr.p) < 0)
			sysfatal("vtwritepacket: %r");
		packetfree(wr.p);
	}
}

int
myvtwrite(VtConn *z, uchar score[VtScoreSize], uint type, uchar *buf, int n)
{
	WriteReq wr;

	if(nwritethread == 0){
		n = vtwrite(z, score, type, buf, n);
		if(n < 0)
			sysfatal("vtwrite: %r");
		return n;
	}

	wr.p = packetalloc();
	packetappend(wr.p, buf, n);
	packetsha1(wr.p, score);
	memmove(wr.score, score, VtScoreSize);
	wr.type = type;
	nsend++;
	send(writechan, &wr);
	return 0;
}

void
ventiproc(void *dummy)
{
	int i;
	Block *db;
	u32int bno;
	u64int bsize;

	USED(dummy);

	proccreate(vtsendproc, z, STACK);
	proccreate(vtrecvproc, z, STACK);

	writechan = chancreate(sizeof(WriteReq), 0);
	for(i=0; i<nwritethread; i++)
		threadcreate(writethread, nil, STACK);
	vtcachesetwrite(zcache, myvtwrite);

	bsize = fsys->blocksize;
	vtfilelock(vfile, -1);
	while((db = qread(qventi, &bno)) != nil){
		if(nop){
			blockput(db);
			continue;
		}
		if(vtfilewrite(vfile, db->data, bsize, bno*bsize) != bsize)
			sysfatal("ventiproc vtfilewrite: %r");
		if(vtfileflushbefore(vfile, (bno+1)*bsize) < 0)
			sysfatal("ventiproc vtfileflushbefore: %r");
		blockput(db);
	}
	vtfileunlock(vfile);
	vtcachesetwrite(zcache, nil);
	for(i=0; i<nwritethread; i++)
		send(writechan, nil);
	chanfree(writechan);
	if(statustime)
		print("# %T venti proc exiting - nsend %d nrecv %d\n", nsend, nrecv);
	runlock(&endlk);
}

static int
percent(u32int a, u32int b)
{
	return (vlong)a*100/b;
}

void
statusproc(void *dummy)
{
	int n;
	USED(dummy);

	for(n=0;;n++){
		sleep(1000);
		if(qcmp->closed && qcmp->nel==0 && qventi->closed && qventi->nel==0)
			break;
		if(n < statustime)
			continue;
		n = 0;
		print("# %T fsscan=%d%% cmpq=%d%% ventiq=%d%%\n",
			percent(fsscanblock, fsys->nblock),
			percent(qcmp->nel, MAXQ),
			percent(qventi->nel, MAXQ));
	}
	print("# %T status proc exiting\n");
	runlock(&endlk);
}

int
timefmt(Fmt *fmt)
{
	vlong ns;
	Tm tm;
	ns = nsec();
	tm = *localtime(time(0));
	return fmtprint(fmt, "%04d/%02d%02d %02d:%02d:%02d.%03d",
		tm.year+1900, tm.mon+1, tm.mday, tm.hour, tm.min, tm.sec,
		(int)(ns%1000000000)/1000000);
}

char*
guessmountplace(char *dev)
{
	char *cmd, *q;
	int p[2], fd[3], n;
	char buf[100];

	if(pipe(p) < 0)
		sysfatal("pipe: %r");

	fd[0] = -1;
	fd[1] = p[1];
	fd[2] = -1;
	cmd = smprint("mount | awk 'BEGIN{v=\"%s\"; u=v; sub(/rdisk/, \"disk\", u);} ($1==v||$1==u) && $2 == \"on\" {print $3}'", dev);
	if(threadspawnl(fd, "sh", "sh", "-c", cmd, nil) < 0)
		sysfatal("exec mount|awk (to find mtpt of %s): %r", dev);
	/* threadspawnl closed p[1] */
	free(cmd);
	n = readn(p[0], buf, sizeof buf-1);
	close(p[0]);
	if(n <= 0)
		return dev;
	buf[n] = 0;
	if((q = strchr(buf, '\n')) == nil)
		return dev;
	*q = 0;
	q = buf+strlen(buf);
	if(q>buf && *(q-1) == '/')
		*--q = 0;
	return strdup(buf);
}
