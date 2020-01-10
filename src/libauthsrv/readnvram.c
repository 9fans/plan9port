#include <u.h>
#include <libc.h>
#include <authsrv.h>

static long	finddosfile(int, char*);

static int
check(void *x, int len, uchar sum, char *msg)
{
	if(nvcsum(x, len) == sum)
		return 0;
	memset(x, 0, len);
	fprint(2, "%s\n", msg);
	return 1;
}

/*
 *  get key info out of nvram.  since there isn't room in the PC's nvram use
 *  a disk partition there.
 */
static struct {
	char *cputype;
	char *file;
	int off;
	int len;
} nvtab[] = {
	"sparc", "#r/nvram", 1024+850, sizeof(Nvrsafe),
	"pc", "#S/sdC0/nvram", 0, sizeof(Nvrsafe),
	"pc", "#S/sdC0/9fat", -1, sizeof(Nvrsafe),
	"pc", "#S/sdC1/nvram", 0, sizeof(Nvrsafe),
	"pc", "#S/sdC1/9fat", -1, sizeof(Nvrsafe),
	"pc", "#S/sd00/nvram", 0, sizeof(Nvrsafe),
	"pc", "#S/sd00/9fat", -1, sizeof(Nvrsafe),
	"pc", "#S/sd01/nvram", 0, sizeof(Nvrsafe),
	"pc", "#S/sd01/9fat", -1, sizeof(Nvrsafe),
	"pc", "#f/fd0disk", -1, 512,	/* 512: #f requires whole sector reads */
	"pc", "#f/fd1disk", -1, 512,
	"mips", "#r/nvram", 1024+900, sizeof(Nvrsafe),
	"power", "#F/flash/flash0", 0x440000, sizeof(Nvrsafe),
	"power", "#r/nvram", 4352, sizeof(Nvrsafe),	/* OK for MTX-604e */
	"debug", "/tmp/nvram", 0, sizeof(Nvrsafe),
};

static char*
xreadcons(char *prompt, char *def, int secret, char *buf, int nbuf)
{
	char *p;

	p = readcons(prompt, def, secret);
	if(p == nil)
		return nil;
	strecpy(buf, buf+nbuf, p);
	memset(p, 0, strlen(p));
	free(p);
	return buf;
}

/*
 *  get key info out of nvram.  since there isn't room in the PC's nvram use
 *  a disk partition there.
 */
int
readnvram(Nvrsafe *safep, int flag)
{
	char buf[1024], in[128], *cputype, *nvrfile, *nvrlen, *nvroff, *v[2];
	int fd, err, i, safeoff, safelen;
	Nvrsafe *safe;

	err = 0;
	memset(safep, 0, sizeof(*safep));

	nvrfile = getenv("nvram");
	cputype = getenv("cputype");
	if(cputype == nil)
		cputype = "mips";
	if(strcmp(cputype, "386")==0 || strcmp(cputype, "alpha")==0)
		cputype = "pc";

	fd = -1;
	safeoff = -1;
	safelen = -1;
	if(nvrfile != nil){
		/* accept device and device!file */
		i = gettokens(nvrfile, v, nelem(v), "!");
		fd = open(v[0], ORDWR);
		safelen = sizeof(Nvrsafe);
		if(strstr(v[0], "/9fat") == nil)
			safeoff = 0;
		nvrlen = getenv("nvrlen");
		if(nvrlen != nil)
			safelen = atoi(nvrlen);
		nvroff = getenv("nvroff");
		if(nvroff != nil){
			if(strcmp(nvroff, "dos") == 0)
				safeoff = -1;
			else
				safeoff = atoi(nvroff);
		}
		if(safeoff < 0 && fd >= 0){
			safelen = 512;
			safeoff = finddosfile(fd, i == 2 ? v[1] : "plan9.nvr");
			if(safeoff < 0){
				close(fd);
				fd = -1;
			}
		}
		free(nvrfile);
		if(nvrlen != nil)
			free(nvrlen);
		if(nvroff != nil)
			free(nvroff);
	}else{
		for(i=0; i<nelem(nvtab); i++){
			if(strcmp(cputype, nvtab[i].cputype) != 0)
				continue;
			if((fd = open(nvtab[i].file, ORDWR)) < 0)
				continue;
			safeoff = nvtab[i].off;
			safelen = nvtab[i].len;
			if(safeoff == -1){
				safeoff = finddosfile(fd, "plan9.nvr");
				if(safeoff < 0){
					close(fd);
					fd = -1;
					continue;
				}
			}
			break;
		}
	}

	if(fd < 0
	|| seek(fd, safeoff, 0) < 0
	|| read(fd, buf, safelen) != safelen){
		err = 1;
		if(flag&(NVwrite|NVwriteonerr))
			fprint(2, "can't read nvram: %r\n");
		memset(safep, 0, sizeof(*safep));
		safe = safep;
	}else{
		memmove(safep, buf, sizeof *safep);
		safe = safep;

		err |= check(safe->machkey, DESKEYLEN, safe->machsum, "bad nvram key");
/*		err |= check(safe->config, CONFIGLEN, safe->configsum, "bad secstore key"); */
		err |= check(safe->authid, ANAMELEN, safe->authidsum, "bad authentication id");
		err |= check(safe->authdom, DOMLEN, safe->authdomsum, "bad authentication domain");
	}

	if((flag&NVwrite) || (err && (flag&NVwriteonerr))){
		xreadcons("authid", nil, 0, safe->authid, sizeof(safe->authid));
		xreadcons("authdom", nil, 0, safe->authdom, sizeof(safe->authdom));
		xreadcons("secstore key", nil, 1, safe->config, sizeof(safe->config));
		for(;;){
			if(xreadcons("password", nil, 1, in, sizeof in) == nil)
				goto Out;
			if(passtokey(safe->machkey, in))
				break;
		}
		safe->machsum = nvcsum(safe->machkey, DESKEYLEN);
		safe->configsum = nvcsum(safe->config, CONFIGLEN);
		safe->authidsum = nvcsum(safe->authid, sizeof(safe->authid));
		safe->authdomsum = nvcsum(safe->authdom, sizeof(safe->authdom));
		memmove(buf, safe, sizeof *safe);
		if(seek(fd, safeoff, 0) < 0
		|| write(fd, buf, safelen) != safelen){
			fprint(2, "can't write key to nvram: %r\n");
			err = 1;
		}else
			err = 0;
	}
Out:
	close(fd);
	return err ? -1 : 0;
}

typedef struct Dosboot	Dosboot;
struct Dosboot{
	uchar	magic[3];	/* really an xx86 JMP instruction */
	uchar	version[8];
	uchar	sectsize[2];
	uchar	clustsize;
	uchar	nresrv[2];
	uchar	nfats;
	uchar	rootsize[2];
	uchar	volsize[2];
	uchar	mediadesc;
	uchar	fatsize[2];
	uchar	trksize[2];
	uchar	nheads[2];
	uchar	nhidden[4];
	uchar	bigvolsize[4];
	uchar	driveno;
	uchar	reserved0;
	uchar	bootsig;
	uchar	volid[4];
	uchar	label[11];
	uchar	type[8];
};
#define	GETSHORT(p) (((p)[1]<<8) | (p)[0])
#define	GETLONG(p) ((GETSHORT((p)+2) << 16) | GETSHORT((p)))

typedef struct Dosdir	Dosdir;
struct Dosdir
{
	char	name[8];
	char	ext[3];
	uchar	attr;
	uchar	reserved[10];
	uchar	time[2];
	uchar	date[2];
	uchar	start[2];
	uchar	length[4];
};

static char*
dosparse(char *from, char *to, int len)
{
	char c;

	memset(to, ' ', len);
	if(from == 0)
		return 0;
	while(len-- > 0){
		c = *from++;
		if(c == '.')
			return from;
		if(c == 0)
			break;
		if(c >= 'a' && c <= 'z')
			*to++ = c + 'A' - 'a';
		else
			*to++ = c;
	}
	return 0;
}

/*
 *  return offset of first file block
 *
 *  This is a very simplistic dos file system.  It only
 *  works on floppies, only looks in the root, and only
 *  returns a pointer to the first block of a file.
 *
 *  This exists for cpu servers that have no hard disk
 *  or nvram to store the key on.
 *
 *  Please don't make this any smarter: it stays resident
 *  and I'ld prefer not to waste the space on something that
 *  runs only at boottime -- presotto.
 */
static long
finddosfile(int fd, char *file)
{
	uchar secbuf[512];
	char name[8];
	char ext[3];
	Dosboot	*b;
	Dosdir *root, *dp;
	int nroot, sectsize, rootoff, rootsects, n;

	/* dos'ize file name */
	file = dosparse(file, name, 8);
	dosparse(file, ext, 3);

	/* read boot block, check for sanity */
	b = (Dosboot*)secbuf;
	if(read(fd, secbuf, sizeof(secbuf)) != sizeof(secbuf))
		return -1;
	if(b->magic[0] != 0xEB || b->magic[1] != 0x3C || b->magic[2] != 0x90)
		return -1;
	sectsize = GETSHORT(b->sectsize);
	if(sectsize != 512)
		return -1;
	rootoff = (GETSHORT(b->nresrv) + b->nfats*GETSHORT(b->fatsize)) * sectsize;
	if(seek(fd, rootoff, 0) < 0)
		return -1;
	nroot = GETSHORT(b->rootsize);
	rootsects = (nroot*sizeof(Dosdir)+sectsize-1)/sectsize;
	if(rootsects <= 0 || rootsects > 64)
		return -1;

	/*
	 *  read root. it is contiguous to make stuff like
	 *  this easier
	 */
	root = malloc(rootsects*sectsize);
	if(read(fd, root, rootsects*sectsize) != rootsects*sectsize)
		return -1;
	n = -1;
	for(dp = root; dp < &root[nroot]; dp++)
		if(memcmp(name, dp->name, 8) == 0 && memcmp(ext, dp->ext, 3) == 0){
			n = GETSHORT(dp->start);
			break;
		}
	free(root);

	if(n < 0)
		return -1;

	/*
	 *  dp->start is in cluster units, not sectors.  The first
	 *  cluster is cluster 2 which starts immediately after the
	 *  root directory
	 */
	return rootoff + rootsects*sectsize + (n-2)*sectsize*b->clustsize;
}
