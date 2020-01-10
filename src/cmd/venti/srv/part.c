#ifdef PLAN9PORT	/* SORRY! */
#	include <u.h>
#	include <sys/types.h>
#	ifdef __linux__	/* REALLY SORRY! */
#		define CANBLOCKSIZE 1
#		include <sys/vfs.h>
#	elif defined(__FreeBSD__)
#		define CANBLOCKSIZE 1
#		include <sys/param.h>
#		include <sys/stat.h>
#		include <sys/mount.h>
#	endif
#endif
#include "stdinc.h"
#include <ctype.h>
#include "dat.h"
#include "fns.h"

u32int	maxblocksize;
int	readonly;

int findsubpart(Part *part, char *name);

static int
strtoullsuf(char *p, char **pp, int rad, u64int *u)
{
	u64int v;

	if(!isdigit((uchar)*p))
		return -1;
	v = strtoull(p, &p, rad);
	switch(*p){
	case 'k':
	case 'K':
		v *= 1024;
		p++;
		break;
	case 'm':
	case 'M':
		v *= 1024*1024;
		p++;
		break;
	case 'g':
	case 'G':
		v *= 1024*1024*1024;
		p++;
		break;
	case 't':
	case 'T':
		v *= 1024*1024;
		v *= 1024*1024;
		p++;
		break;
	}
	*pp = p;
	*u = v;
	return 0;
}

static int
parsepart(char *name, char **file, char **subpart, u64int *lo, u64int *hi)
{
	char *p;

	*file = estrdup(name);
	*lo = 0;
	*hi = 0;
	*subpart = nil;
	if((p = strrchr(*file, ':')) == nil)
		return 0;
	*p++ = 0;
	if(isalpha(*p)){
		*subpart = p;
		return 0;
	}
	if(*p == '-')
		*lo = 0;
	else{
		if(strtoullsuf(p, &p, 0, lo) < 0){
			free(*file);
			return -1;
		}
	}
	if(*p == '-')
		p++;
	if(*p == 0){
		*hi = 0;
		return 0;
	}
	if(strtoullsuf(p, &p, 0, hi) < 0 || *p != 0){
		free(*file);
		return -1;
	}
	return 0;
}

#undef min
#define min(a, b) ((a) < (b) ? (a) : (b))
Part*
initpart(char *name, int mode)
{
	Part *part;
	Dir *dir;
	char *file, *subname;
	u64int lo, hi;

	if(parsepart(name, &file, &subname, &lo, &hi) < 0){
		werrstr("cannot parse name %s", name);
		return nil;
	}
	trace(TraceDisk, "initpart %s file %s lo 0x%llx hi 0x%llx", name, file, lo, hi);
	part = MKZ(Part);
	part->name = estrdup(name);
	part->filename = estrdup(file);
	if(readonly){
		mode &= ~(OREAD|OWRITE|ORDWR);
		mode |= OREAD;
	}
#ifdef __linux__	/* sorry, but linus made O_DIRECT unusable! */
	mode &= ~ODIRECT;
#endif
	part->fd = open(file, mode);
	if(part->fd < 0){
		if((mode&(OREAD|OWRITE|ORDWR)) == ORDWR)
			part->fd = open(file, (mode&~ORDWR)|OREAD);
		if(part->fd < 0){
			freepart(part);
			fprint(2, "can't open partition='%s': %r\n", file);
			seterr(EOk, "can't open partition='%s': %r", file);
			fprint(2, "%r\n");
			free(file);
			return nil;
		}
		fprint(2, "warning: %s opened for reading only\n", name);
	}
	part->offset = lo;
	dir = dirfstat(part->fd);
	if(dir == nil){
		freepart(part);
		seterr(EOk, "can't stat partition='%s': %r", file);
		free(file);
		return nil;
	}
	if(dir->length == 0){
		free(dir);
		dir = dirstat(file);
		if(dir == nil || dir->length == 0) {
			freepart(part);
			seterr(EOk, "can't determine size of partition %s", file);
			free(file);
			return nil;
		}
	}
	if(dir->length < hi || dir->length < lo){
		freepart(part);
		seterr(EOk, "partition '%s': bounds out of range (max %lld)", name, dir->length);
		free(dir);
		free(file);
		return nil;
	}
	if(hi == 0)
		hi = dir->length;
	part->size = hi - part->offset;
#ifdef CANBLOCKSIZE
	{
		struct statfs sfs;
		if(fstatfs(part->fd, &sfs) >= 0 && sfs.f_bsize > 512)
			part->fsblocksize = sfs.f_bsize;
	}
#endif

	part->fsblocksize = min(part->fsblocksize, MaxIo);

	if(subname && findsubpart(part, subname) < 0){
		werrstr("cannot find subpartition %s", subname);
		freepart(part);
		return nil;
	}
	free(dir);
	return part;
}

int
flushpart(Part *part)
{
	USED(part);
#ifdef __linux__	/* grrr! */
	if(fsync(part->fd) < 0){
		logerr(EAdmin, "flushpart %s: %r", part->name);
		return -1;
	}
	posix_fadvise(part->fd, 0, 0, POSIX_FADV_DONTNEED);
#endif
	return 0;
}

void
freepart(Part *part)
{
	if(part == nil)
		return;
	if(part->fd >= 0)
		close(part->fd);
	free(part->name);
	free(part);
}

void
partblocksize(Part *part, u32int blocksize)
{
	if(part->blocksize)
		sysfatal("resetting partition=%s's block size", part->name);
	part->blocksize = blocksize;
	if(blocksize > maxblocksize)
		maxblocksize = blocksize;
}

/*
 * Read/write some amount of data between a block device or file and a memory buffer.
 *
 * Most Unix systems require that when accessing a block device directly,
 * the buffer, offset, and count are all multiples of the device block size,
 * making this a lot more complicated than it otherwise would be.
 *
 * Most of our callers will make things easy on us, but for some callers it's best
 * if we just do the work here, with only one place to get it right (hopefully).
 *
 * If everything is aligned properly, prwb will try to do big transfers in the main
 * body of the loop: up to MaxIo bytes at a time.  If everything isn't aligned properly,
 * we work one block at a time.
 */
int
prwb(char *name, int fd, int isread, u64int offset, void *vbuf, u32int count, u32int blocksize)
{
	char *op;
	u8int *buf, *freetmp, *dst;
	u32int icount, opsize;
	int r, count1;


#ifndef PLAN9PORT
	USED(blocksize);
	icount = count;
	buf = vbuf;
	op = isread ? "read" : "write";
	dst = buf;
	freetmp = nil;
	while(count > 0){
		opsize = min(count, 131072 /* blocksize */);
		if(isread)
			r = pread(fd, dst, opsize, offset);
		else
			r = pwrite(fd, dst, opsize, offset);
		if(r <= 0)
			goto Error;
		offset += r;
		count -= r;
		dst += r;
		if(r != opsize)
			goto Error;
	}
	return icount;
#else
	u32int c, delta;
	u8int *tmp;

	icount = count;
	buf = vbuf;
	tmp = nil;
	freetmp = nil;
	opsize = blocksize;

	if(count == 0){
		logerr(EStrange, "pwrb %s called to %s 0 bytes", name, isread ? "read" : "write");
		return 0;
	}

	assert(blocksize > 0);

	/* allocate blocksize-aligned temp buffer if needed */
	if((ulong)offset%blocksize || (ulong)buf%blocksize || count%blocksize){
		if((freetmp = malloc(blocksize*2)) == nil)
			return -1;
		tmp = freetmp;
		tmp += blocksize - (ulong)tmp%blocksize;
	}

	/* handle beginning fringe */
	if((delta = (ulong)offset%blocksize) != 0){
		assert(tmp != nil);
		if((r=pread(fd, tmp, blocksize, offset-delta)) != blocksize){
			dst = tmp;
			offset = offset-delta;
			op = "read";
			count1 = blocksize;
			goto Error;
		}
		c = min(count, blocksize-delta);
		assert(c > 0 && c < blocksize);
		if(isread)
			memmove(buf, tmp+delta, c);
		else{
			memmove(tmp+delta, buf, c);
			if((r=pwrite(fd, tmp, blocksize, offset-delta)) != blocksize){
				dst = tmp;
				offset = offset-delta;
				op = "read";
				count1 = blocksize;
				goto Error;
			}
		}
		assert(c > 0);
		offset += c;
		buf += c;
		count -= c;
	}

	/* handle full blocks */
	while(count >= blocksize){
		assert((ulong)offset%blocksize == 0);
		if((ulong)buf%blocksize){
			assert(tmp != nil);
			dst = tmp;
			opsize = blocksize;
		}else{
			dst = buf;
			opsize = count - count%blocksize;
			if(opsize > MaxIo)
				opsize = MaxIo;
		}
		if(isread){
			if((r=pread(fd, dst, opsize, offset))<=0 || r%blocksize){
				op = "read";
				count1 = opsize;
				goto Error;
			}
			if(dst == tmp){
				assert(r == blocksize);
				memmove(buf, tmp, blocksize);
			}
		}else{
			if(dst == tmp){
				assert(opsize == blocksize);
				memmove(dst, buf, blocksize);
			}
			if((r=pwrite(fd, dst, opsize, offset))<=0 || r%blocksize){
				count1 = opsize;
				op = "write";
				goto Error;
			}
			if(dst == tmp)
				assert(r == blocksize);
		}
		assert(r > 0);
		offset += r;
		buf += r;
		count -= r;
	}

	/* handle ending fringe */
	if(count > 0){
		assert((ulong)offset%blocksize == 0);
		assert(tmp != nil);
		/*
		 * Complicated condition: if we're reading it's okay to get less than
		 * a block as long as it's enough to satisfy the read - maybe this is
		 * a normal file.  (We never write to normal files, or else things would
		 * be even more complicated.)
		 */
		r = pread(fd, tmp, blocksize, offset);
		if((isread && r < count) || (!isread && r != blocksize)){
print("FAILED isread=%d r=%d count=%d blocksize=%d\n", isread, r, count, blocksize);
			dst = tmp;
			op = "read";
			count1 = blocksize;
			goto Error;
		}
		if(isread)
			memmove(buf, tmp, count);
		else{
			memmove(tmp, buf, count);
			if(pwrite(fd, tmp, blocksize, offset) != blocksize){
				dst = tmp;
				count1 = blocksize;
				op = "write";
				goto Error;
			}
		}
	}
	if(freetmp)
		free(freetmp);
	return icount;
#endif

Error:
	seterr(EAdmin, "%s %s offset 0x%llux count %ud buf %p returned %d: %r",
		op, name, offset, count1, dst, r);
	if(freetmp)
		free(freetmp);
	return -1;
}

#ifndef PLAN9PORT
static int sdreset(Part*);
static int reopen(Part*);
static int threadspawnl(int[3], char*, char*, ...);
#endif

int
rwpart(Part *part, int isread, u64int offset, u8int *buf, u32int count)
{
	int n, try;
	u32int blocksize;

	trace(TraceDisk, "%s %s %ud at 0x%llx",
		isread ? "read" : "write", part->name, count, offset);
	if(offset >= part->size || offset+count > part->size){
		seterr(EStrange, "out of bounds %s offset 0x%llux count %ud to partition %s size 0x%llux",
			isread ? "read" : "write", offset, count, part->name, part->size);
		return -1;
	}

	blocksize = part->fsblocksize;
	if(blocksize == 0)
		blocksize = part->blocksize;
	if(blocksize == 0)
		blocksize = 4096;

	for(try=0;; try++){
		n = prwb(part->filename, part->fd, isread, part->offset+offset, buf, count, blocksize);
		if(n >= 0 || try > 10)
			break;

#ifndef PLAN9PORT
	    {
		char err[ERRMAX];
		/*
		 * This happens with the sdmv disks frustratingly often.
		 * Try to fix things up and continue.
		 */
		rerrstr(err, sizeof err);
		if(strstr(err, "i/o timeout") || strstr(err, "i/o error") || strstr(err, "partition has changed")){
			reopen(part);
			continue;
		}
	    }
#endif
		break;
	}
#ifdef __linux__	/* sigh */
	posix_fadvise(part->fd, part->offset+offset, n, POSIX_FADV_DONTNEED);
#endif
	return n;
}
int
readpart(Part *part, u64int offset, u8int *buf, u32int count)
{
	return rwpart(part, 1, offset, buf, count);
}

int
writepart(Part *part, u64int offset, u8int *buf, u32int count)
{
	return rwpart(part, 0, offset, buf, count);
}

ZBlock*
readfile(char *name)
{
	Part *p;
	ZBlock *b;

	p = initpart(name, OREAD);
	if(p == nil)
		return nil;
	b = alloczblock(p->size, 0, p->blocksize);
	if(b == nil){
		seterr(EOk, "can't alloc %s: %r", name);
		freepart(p);
		return nil;
	}
	if(readpart(p, 0, b->data, p->size) < 0){
		seterr(EOk, "can't read %s: %r", name);
		freepart(p);
		freezblock(b);
		return nil;
	}
	freepart(p);
	return b;
}

/*
 * Search for the Plan 9 partition with the given name.
 * This lets you write things like /dev/ad4:arenas
 * if you move a disk from a Plan 9 system to a FreeBSD system.
 *
 * God I hope I never write this code again.
 */
#define MAGIC "plan9 partitions"
static int
tryplan9part(Part *part, char *name)
{
	uchar buf[512];
	char *line[40], *f[4];
	int i, n;
	vlong start, end;

	/*
	 * Partition table in second sector.
	 * Could also look on 2nd last sector and last sector,
	 * but those disks died out long before venti came along.
	 */
	if(readpart(part, 512, buf, 512) != 512)
		return -1;

	/* Plan 9 partition table is just text strings */
	if(strncmp((char*)buf, "part ", 5) != 0)
		return -1;

	buf[511] = 0;
	n = getfields((char*)buf, line, 40, 1, "\n");
	for(i=0; i<n; i++){
		if(getfields(line[i], f, 4, 1, " ") != 4)
			break;
		if(strcmp(f[0], "part") != 0)
			break;
		if(strcmp(f[1], name) == 0){
			start = 512*strtoll(f[2], 0, 0);
			end = 512*strtoll(f[3], 0, 0);
			if(start  < end && end <= part->size){
				part->offset += start;
				part->size = end - start;
				return 0;
			}
			return -1;
		}
	}
	return -1;
}

#define	GSHORT(p)	(((p)[1]<<8)|(p)[0])
#define	GLONG(p)	((GSHORT(p+2)<<16)|GSHORT(p))

typedef struct Dospart Dospart;
struct Dospart
{
	uchar flag;		/* active flag */
	uchar shead;		/* starting head */
	uchar scs[2];		/* starting cylinder/sector */
	uchar type;		/* partition type */
	uchar ehead;		/* ending head */
	uchar ecs[2];		/* ending cylinder/sector */
	uchar offset[4];		/* starting sector */
	uchar size[4];		/* length in sectors */
};


int
findsubpart(Part *part, char *name)
{
	int i;
	uchar buf[512];
	u64int size;
	Dospart *dp;

	/* See if this is a Plan 9 partition. */
	if(tryplan9part(part, name) >= 0)
		return 0;

	/* Otherwise try for an MBR and then narrow to Plan 9 partition. */
	if(readpart(part, 0, buf, 512) != 512)
		return -1;
	if(buf[0x1FE] != 0x55 || buf[0x1FF] != 0xAA)
		return -1;
	dp = (Dospart*)(buf+0x1BE);
	size = part->size;
	for(i=0; i<4; i++){
		if(dp[i].type == '9'){
			part->offset = 512LL*GLONG(dp[i].offset);
			part->size = 512LL*GLONG(dp[i].size);
			if(tryplan9part(part, name) >= 0)
				return 0;
			part->offset = 0;
			part->size = size;
		}
		/* Not implementing extended partitions - enough is enough. */
	}
	return -1;
}
