#include "stdinc.h"
#include "dat.h"
#include "fns.h"

u32int	maxblocksize;
int	readonly;

Part*
initpart(char *name, int writable)
{
	Part *part;
	Dir *dir;
	int how;

	part = MK(Part);
	part->name = estrdup(name);
	if(!writable && readonly)
		how = OREAD;
	else
		how = ORDWR;
	part->fd = open(name, how);
	if(part->fd < 0){
		if(how == ORDWR)
			part->fd = open(name, OREAD);
		if(part->fd < 0){
			freepart(part);
			seterr(EOk, "can't open partition='%s': %r", name);
			return nil;
		}
		fprint(2, "warning: %s opened for reading only\n", name);
	}
	dir = dirfstat(part->fd);
	if(dir == nil){
		freepart(part);
		seterr(EOk, "can't stat partition='%s': %r", name);
		return nil;
	}
	part->size = dir->length;
	part->blocksize = 0;
	free(dir);
	return part;
}

void
freepart(Part *part)
{
	if(part == nil)
		return;
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

int
writepart(Part *part, u64int addr, u8int *buf, u32int n)
{
	long m, mm, nn;

	qlock(&stats.lock);
	stats.diskwrites++;
	stats.diskbwrites += n;
	qunlock(&stats.lock);

	if(addr > part->size || addr + n > part->size){
		seterr(ECorrupt, "out of bounds write to partition='%s'", part->name);
		return -1;
	}
	print("write %s %lud at %llud\n", part->name, n, addr);
	for(nn = 0; nn < n; nn += m){
		mm = n - nn;
		if(mm > MaxIo)
			mm = MaxIo;
		m = pwrite(part->fd, &buf[nn], mm, addr + nn);
		if(m != mm){
			if(m < 0){
				seterr(EOk, "can't write partition='%s': %r", part->name);
				return -1;
			}
			logerr(EOk, "truncated write to partition='%s' n=%ld wrote=%ld", part->name, mm, m);
		}
	}
	return 0;
}

int
readpart(Part *part, u64int addr, u8int *buf, u32int n)
{
	long m, mm, nn;
	int i;

	qlock(&stats.lock);
	stats.diskreads++;
	stats.diskbreads += n;
	qunlock(&stats.lock);

	if(addr > part->size || addr + n > part->size){
		seterr(ECorrupt, "out of bounds read from partition='%s': addr=%lld n=%d size=%lld", part->name, addr, n, part->size);
		return -1;
	}
	print("read %s %lud at %llud\n", part->name, n, addr);
	for(nn = 0; nn < n; nn += m){
		mm = n - nn;
		if(mm > MaxIo)
			mm = MaxIo;
		m = -1;
		for(i=0; i<4; i++) {
			m = pread(part->fd, &buf[nn], mm, addr + nn);
			if(m == mm)
				break;
		}
		if(m != mm){
			if(m < 0){
				seterr(EOk, "can't read partition='%s': %r", part->name);
				return -1;
			}
			logerr(EOk, "warning: truncated read from partition='%s' n=%ld read=%ld", part->name, mm, m);
		}
	}
	return 0;
}
