/*
 * File map routines
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

static int fdrw(Map*, Seg*, u64int, void*, uint, int);
static int zerorw(Map*, Seg*, u64int, void*, uint, int);
static int mrw(Map*, u64int, void*, uint, int);
static int datarw(Map*, Seg*, u64int, void*, uint, int);

Map*
allocmap(void)
{
	return mallocz(sizeof(Map), 1);
}

void
freemap(Map *map)
{
	if(map == nil)
		return;
	free(map->seg);
	free(map);
}

int
addseg(Map *map, Seg seg)
{
	Seg *ss;

	if(map == nil){
		werrstr("invalid map");
		return -1;
	}

	ss = realloc(map->seg, (map->nseg+1)*sizeof(ss[0]));
	if(ss == nil)
		return -1;
	map->seg = ss;
	if(seg.rw == 0){
		if(seg.name && strcmp(seg.name, "zero") == 0)
			seg.rw = zerorw;
		else if(seg.p)
			seg.rw = datarw;
		else
			seg.rw = fdrw;
	}
	map->seg[map->nseg] = seg;
	return map->nseg++;
}

int
findseg(Map *map, char *name, char *file)
{
	int i;

	if(map == nil)
		return -1;
	for(i=0; i<map->nseg; i++){
		if(name && (!map->seg[i].name || strcmp(map->seg[i].name, name) != 0))
			continue;
		if(file && (!map->seg[i].file || strcmp(map->seg[i].file, file) != 0))
			continue;
		return i;
	}
	werrstr("segment %s in %s not found", name, file);
	return -1;
}

int
addrtoseg(Map *map, u64int addr, Seg *sp)
{
	int i;
	Seg *s;

	if(map == nil){
		werrstr("no map");
		return -1;
	}
	for(i=map->nseg-1; i>=0; i--){
		s = &map->seg[i];
		if(s->base <= addr && addr-s->base < s->size){
			if(sp)
				*sp = *s;
			return i;
		}
	}
	werrstr("address 0x%lux is not mapped", addr);
	return -1;
}

int
addrtosegafter(Map *map, u64int addr, Seg *sp)
{
	int i;
	Seg *s, *best;
	ulong bdist;

	if(map == nil){
		werrstr("no map");
		return -1;
	}

	/*
	 * If segments were sorted this would be easier,
	 * but since segments may overlap, sorting also
	 * requires splitting and rejoining, and that's just
	 * too complicated.
	 */
	best = nil;
	bdist = 0;
	for(i=map->nseg-1; i>=0; i--){
		s = &map->seg[i];
		if(s->base > addr){
			if(best==nil || s->base-addr < bdist){
				bdist = s->base - addr;
				best = s;
			}
		}
	}
	if(best){
		if(sp)
			*sp = *best;
		return best-map->seg;
	}
	werrstr("nothing mapped after address 0x%lux", addr);
	return -1;
}

void
removeseg(Map *map, int i)
{
	if(map == nil)
		return;
	if(i < 0 || i >= map->nseg)
		return;
	memmove(&map->seg[i], &map->seg[i+1], (map->nseg-(i+1))*sizeof(Seg));
	map->nseg--;
}

int
get1(Map *map, u64int addr, uchar *a, uint n)
{
	return mrw(map, addr, a, n, 1);
}

int
get2(Map *map, u64int addr, u16int *u)
{
	u16int v;

	if(mrw(map, addr, &v, 2, 1) < 0)
		return -1;
	*u = mach->swap2(v);
	return 2;
}

int
get4(Map *map, u64int addr, u32int *u)
{
	u32int v;

	if(mrw(map, addr, &v, 4, 1) < 0)
		return -1;
	*u = mach->swap4(v);
	return 4;
}

int
get8(Map *map, u64int addr, u64int *u)
{
	u64int v;

	if(mrw(map, addr, &v, 8, 1) < 0)
		return -1;
	*u = mach->swap8(v);
	return 8;
}

int
geta(Map *map, u64int addr, u64int *u)
{
	u32int v;

	if(machcpu == &machamd64)
		return get8(map, addr, u);
	if(get4(map, addr, &v) < 0)
		return -1;
	*u = v;
	return 4;
}

int
put1(Map *map, u64int addr, uchar *a, uint n)
{
	return mrw(map, addr, a, n, 0);
}

int
put2(Map *map, u64int addr, u16int u)
{
	u = mach->swap2(u);
	return mrw(map, addr, &u, 2, 0);
}

int
put4(Map *map, u64int addr, u32int u)
{
	u = mach->swap4(u);
	return mrw(map, addr, &u, 4, 0);
}

int
put8(Map *map, u64int addr, u64int u)
{
	u = mach->swap8(u);
	return mrw(map, addr, &u, 8, 0);
}

static Seg*
reloc(Map *map, u64int addr, uint n, u64int *off, uint *nn)
{
	int i;
	ulong o;

	if(map == nil){
		werrstr("invalid map");
		return nil;
	}

	for(i=map->nseg-1; i>=0; i--){
		if(map->seg[i].base <= addr){
			o = addr - map->seg[i].base;
			if(o >= map->seg[i].size)
				continue;
			if(o+n > map->seg[i].size)
				*nn = map->seg[i].size - o;
			else
				*nn = n;
			*off = o;
			return &map->seg[i];
		}
	}
	werrstr("address 0x%lux not mapped", addr);
	return nil;
}

static int
mrw(Map *map, u64int addr, void *a, uint n, int r)
{
	uint nn;
	uint tot;
	Seg *s;
	u64int off;

	for(tot=0; tot<n; tot+=nn){
		s = reloc(map, addr+tot, n-tot, &off, &nn);
		if(s == nil)
			return -1;
		if(s->rw(map, s, off, a, nn, r) < 0)
			return -1;
	}
	return 0;
}

static int
fdrw(Map *map, Seg *seg, u64int addr, void *a, uint n, int r)
{
	int nn;
	uint tot;
	ulong off;

	USED(map);
	off = seg->offset + addr;
	for(tot=0; tot<n; tot+=nn){
		if(r)
			nn = pread(seg->fd, a, n-tot, off+tot);
		else
			nn = pwrite(seg->fd, a, n-tot, off+tot);
		if(nn < 0)
			return -1;
		if(nn == 0){
			werrstr("partial %s at address 0x%lux in %s",
				r ? "read" : "write", off+tot, seg->file);
			return -1;
		}
	}
	return 0;
}

static int
zerorw(Map *map, Seg *seg, u64int addr, void *a, uint n, int r)
{
	USED(map);
	USED(seg);
	USED(addr);

	if(r==0){
		werrstr("cannot write zero segment");
		return -1;
	}
	memset(a, 0, n);
	return 0;
}

static int
datarw(Map *map, Seg *seg, u64int addr, void *a, uint n, int r)
{
	USED(map);

	if(r)
		memmove(a, seg->p+addr, n);
	else
		memmove(seg->p+addr, a, n);
	return 0;
}
