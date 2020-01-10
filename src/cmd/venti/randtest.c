#include <u.h>
#include <libc.h>
#include <venti.h>
#include <libsec.h>
#include <thread.h>


enum { STACK = 32768 };
void xxxsrand(long);
long xxxlrand(void);

Channel *cw;
Channel *cr;
char *host;
int blocksize, seed, randpct;
int doread, dowrite, packets, permute;
vlong totalbytes, cur;
VtConn *z;
int multi;
int maxpackets;
int sequence;
int doublecheck = 1;
uint *order;

void
usage(void)
{
	fprint(2, "usage: randtest [-q] [-h host] [-s seed] [-b blocksize] [-p randpct] [-n totalbytes] [-M maxblocks] [-P] [-r] [-w]\n");
	threadexitsall("usage");
}

void
wr(char *buf, char *buf2)
{
	uchar score[VtScoreSize], score2[VtScoreSize];
	DigestState ds;

	USED(buf2);
	memset(&ds, 0, sizeof ds);
	if(doublecheck)
		sha1((uchar*)buf, blocksize, score, &ds);
	if(vtwrite(z, score2, VtDataType, (uchar*)buf, blocksize) < 0)
		sysfatal("vtwrite %V at %,lld: %r", score, cur);
	if(doublecheck && memcmp(score, score2, VtScoreSize) != 0)
		sysfatal("score mismatch! %V %V", score, score2);
}

void
wrthread(void *v)
{
	char *p;

	USED(v);
	while((p = recvp(cw)) != nil){
		wr(p, nil);
		free(p);
	}
}

void
rd(char *buf, char *buf2)
{
	uchar score[VtScoreSize];
	DigestState ds;

	memset(&ds, 0, sizeof ds);
	sha1((uchar*)buf, blocksize, score, &ds);
	if(vtread(z, score, VtDataType, (uchar*)buf2, blocksize) < 0)
		sysfatal("vtread %V at %,lld: %r", score, cur);
	if(memcmp(buf, buf2, blocksize) != 0)
		sysfatal("bad data read! %V", score);
}

void
rdthread(void *v)
{
	char *p, *buf2;

	buf2 = vtmalloc(blocksize);
	USED(v);
	while((p = recvp(cr)) != nil){
		rd(p, buf2);
		free(p);
	}
}

char *template;

void
run(void (*fn)(char*, char*), Channel *c)
{
	int i, t, j, packets;
	char *buf2, *buf;

	buf2 = vtmalloc(blocksize);
	buf = vtmalloc(blocksize);
	cur = 0;
	packets = totalbytes/blocksize;
	if(maxpackets > 0 && maxpackets < packets)
		packets = maxpackets;
	totalbytes = (vlong)packets * blocksize;
	order = vtmalloc(packets*sizeof order[0]);
	for(i=0; i<packets; i++)
		order[i] = i;
	if(permute){
		for(i=1; i<packets; i++){
			j = nrand(i+1);
			t = order[i];
			order[i] = order[j];
			order[j] = t;
		}
	}
	for(i=0; i<packets; i++){
		memmove(buf, template, blocksize);
		*(uint*)buf = order[i];
		if(c){
			sendp(c, buf);
			buf = vtmalloc(blocksize);
		}else
			(*fn)(buf, buf2);
		cur += blocksize;
	}
	free(order);
}

#define TWID64	((u64int)~(u64int)0)

u64int
unittoull(char *s)
{
	char *es;
	u64int n;

	if(s == nil)
		return TWID64;
	n = strtoul(s, &es, 0);
	if(*es == 'k' || *es == 'K'){
		n *= 1024;
		es++;
	}else if(*es == 'm' || *es == 'M'){
		n *= 1024*1024;
		es++;
	}else if(*es == 'g' || *es == 'G'){
		n *= 1024*1024*1024;
		es++;
	}else if(*es == 't' || *es == 'T'){
		n *= 1024*1024;
		n *= 1024*1024;
	}
	if(*es != '\0')
		return TWID64;
	return n;
}

void
threadmain(int argc, char *argv[])
{
	int i, max;
	vlong t0;
	double t;

	blocksize = 8192;
	seed = 0;
	randpct = 50;
	host = nil;
	doread = 0;
	dowrite = 0;
	totalbytes = 1*1024*1024*1024;
	fmtinstall('V', vtscorefmt);
	fmtinstall('F', vtfcallfmt);

	ARGBEGIN{
	case 'b':
		blocksize = unittoull(EARGF(usage()));
		break;
	case 'h':
		host = EARGF(usage());
		break;
	case 'M':
		maxpackets = unittoull(EARGF(usage()));
		break;
	case 'm':
		multi = atoi(EARGF(usage()));
		break;
	case 'n':
		totalbytes = unittoull(EARGF(usage()));
		break;
	case 'p':
		randpct = atoi(EARGF(usage()));
		break;
	case 'P':
		permute = 1;
		break;
	case 'S':
		doublecheck = 0;
		ventidoublechecksha1 = 0;
		break;
	case 's':
		seed = atoi(EARGF(usage()));
		break;
	case 'r':
		doread = 1;
		break;
	case 'w':
		dowrite = 1;
		break;
	case 'V':
		chattyventi++;
		break;
	default:
		usage();
	}ARGEND

	if(doread==0 && dowrite==0){
		doread = 1;
		dowrite = 1;
	}

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	if(multi){
		cr = chancreate(sizeof(void*), 0);
		cw = chancreate(sizeof(void*), 0);
		for(i=0; i<multi; i++){
			proccreate(wrthread, nil, STACK);
			proccreate(rdthread, nil, STACK);
		}
	}

	template = vtmalloc(blocksize);
	xxxsrand(seed);
	max = (256*randpct)/100;
	if(max == 0)
		max = 1;
	for(i=0; i<blocksize; i++)
		template[i] = xxxlrand()%max;
	if(dowrite){
		t0 = nsec();
		run(wr, cw);
		for(i=0; i<multi; i++)
			sendp(cw, nil);
		t = (nsec() - t0)/1.e9;
		print("write: %lld bytes / %.3f seconds = %.6f MB/s\n",
			totalbytes, t, (double)totalbytes/1e6/t);
	}
	if(doread){
		t0 = nsec();
		run(rd, cr);
		for(i=0; i<multi; i++)
			sendp(cr, nil);
		t = (nsec() - t0)/1.e9;
		print("read: %lld bytes / %.3f seconds = %.6f MB/s\n",
			totalbytes, t, (double)totalbytes/1e6/t);
	}
	threadexitsall(nil);
}


/*
 *	algorithm by
 *	D. P. Mitchell & J. A. Reeds
 */

#define	LEN	607
#define	TAP	273
#define	MASK	0x7fffffffL
#define	A	48271
#define	M	2147483647
#define	Q	44488
#define	R	3399
#define	NORM	(1.0/(1.0+MASK))

static	ulong	rng_vec[LEN];
static	ulong*	rng_tap = rng_vec;
static	ulong*	rng_feed = 0;

static void
isrand(long seed)
{
	long lo, hi, x;
	int i;

	rng_tap = rng_vec;
	rng_feed = rng_vec+LEN-TAP;
	seed = seed%M;
	if(seed < 0)
		seed += M;
	if(seed == 0)
		seed = 89482311;
	x = seed;
	/*
	 *	Initialize by x[n+1] = 48271 * x[n] mod (2**31 - 1)
	 */
	for(i = -20; i < LEN; i++) {
		hi = x / Q;
		lo = x % Q;
		x = A*lo - R*hi;
		if(x < 0)
			x += M;
		if(i >= 0)
			rng_vec[i] = x;
	}
}

void
xxxsrand(long seed)
{
	isrand(seed);
}

long
xxxlrand(void)
{
	ulong x;

	rng_tap--;
	if(rng_tap < rng_vec) {
		if(rng_feed == 0) {
			isrand(1);
			rng_tap--;
		}
		rng_tap += LEN;
	}
	rng_feed--;
	if(rng_feed < rng_vec)
		rng_feed += LEN;
	x = (*rng_feed + *rng_tap) & MASK;
	*rng_feed = x;

	return x;
}
