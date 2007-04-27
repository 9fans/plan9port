#include "stdinc.h"
#include "dat.h"
#include "fns.h"

void
zeropart(Part *part, int blocksize)
{
	ZBlock *b;
	u64int addr;
	int w;

	fprint(2, "clearing %s\n", part->name);
	b = alloczblock(MaxIoSize, 1, blocksize);

	w = 0;
	for(addr = PartBlank; addr + MaxIoSize <= part->size; addr += MaxIoSize){
		if(writepart(part, addr, b->data, MaxIoSize) < 0)
			sysfatal("can't initialize %s, writing block %d failed: %r", part->name, w);
		w++;
	}

	for(; addr + blocksize <= part->size; addr += blocksize)
		if(writepart(part, addr, b->data, blocksize) < 0)
			sysfatal("can't initialize %s: %r", part->name);

	if(flushpart(part) < 0)
		sysfatal("can't flush writes to %s: %r", part->name);

	freezblock(b);
}
