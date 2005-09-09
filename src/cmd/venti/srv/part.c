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
parsepart(char *name, char **file, u64int *lo, u64int *hi)
{
	char *p;

	*file = estrdup(name);
	if((p = strrchr(*file, ':')) == nil){
		*lo = 0;
		*hi = 0;
		return 0;
	}
	*p++ = 0;
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

Part*
initpart(char *name, int mode)
{
	Part *part;
	Dir *dir;
	char *file;
	u64int lo, hi;

	if(parsepart(name, &file, &lo, &hi) < 0)
		return nil;
	trace(TraceDisk, "initpart %s file %s lo 0x%llx hi 0x%llx", name, file, lo, hi);
	part = MKZ(Part);
	part->name = estrdup(name);
	part->filename = estrdup(file);
	if(readonly){
		mode &= (OREAD|OWRITE|ORDWR);
		mode |= OREAD;
	}
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
		freepart(part);
		seterr(EOk, "can't determine size of partition %s", file);
		free(file);
		return nil;
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
		if(fstatfs(part->fd, &sfs) >= 0)
			part->fsblocksize = sfs.f_bsize;
	}
#endif
	free(dir);
	return part;
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
#undef min
#define min(a, b) ((a) < (b) ? (a) : (b))
int
prwb(char *name, int fd, int isread, u64int offset, void *vbuf, u32int count, u32int blocksize)
{
	char *op;
	u8int *buf, *tmp, *freetmp, *dst;
	u32int c, delta, icount, opsize;
	int r;

	buf = vbuf;
	tmp = nil;
	freetmp = nil;
	icount = count;
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
			goto Error;
		}
		if(isread)
			memmove(buf, tmp, count);
		else{
			memmove(tmp, buf, count);
			if(pwrite(fd, tmp, opsize, offset) != blocksize){
				dst = tmp;
				op = "write";
				goto Error;
			}
		}
	}
	if(freetmp)
		free(freetmp);
	return icount;

Error:
	seterr(EAdmin, "%s %s offset 0x%llux count %ud buf %p returned %d: %r",
		op, name, offset, opsize, dst, r);
	if(freetmp)
		free(freetmp);
	return -1;
}

int
rwpart(Part *part, int isread, u64int offset, u8int *buf, u32int count)
{
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

	return prwb(part->filename, part->fd, isread, part->offset+offset, buf, count, blocksize);
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

