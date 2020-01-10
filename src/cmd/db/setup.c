/*
 * init routines
 */
#include "defs.h"
#include "fns.h"

char	*symfil;
char	*corfil;

Map	*symmap;
Map	*cormap;
Regs	*correg;
Map	*dotmap;

void
setsym(void)
{
	if(symhdr && symopen(symhdr) < 0)
		dprint("symopen: %r\n");
/*
	Symbol s;
	if (mach->sbreg && lookup(0, mach->sbreg, &s) < 0)
		mach->sb = s.loc.addr;
*/
}

void
setcor(void)
{
	static int mapped;

	if (corhdr) {
		if (!mapped) {
			if (mapfile(corhdr, 0, cormap, &correg) < 0)
				dprint("mapfile %s: %r\n", corfil);
			mapped = 1;
		}
		free(correg);
		if (pid == 0 && corhdr->nthread > 0)
			pid = corhdr->thread[0].id;
		correg = coreregs(corhdr, pid);
		if(correg == nil)
			dprint("no such pid in core dump\n");
	} else {
		unmapproc(cormap);
		unmapfile(corhdr, cormap);
		free(correg);
		correg = nil;

		if (pid > 0) {
			if (mapproc(pid, cormap, &correg) < 0)
				dprint("mapproc %d: %r\n", pid);
		} else
			dprint("no core image\n");
	}
	kmsys();
	return;
}

Map*
dumbmap(int fd)
{
	Map *dumb;
	Seg s;

	dumb = allocmap();
	memset(&s, 0, sizeof s);
	s.fd = fd;
	s.base = 0;
	s.offset = 0;
	s.size = 0xFFFFFFFF;
	s.name = "data";
	s.file = "<dumb>";
	if(addseg(dumb, s) < 0){
		freemap(dumb);
		return nil;
	}
	if(mach == nil)
		mach = machcpu;
	return dumb;
}

/*
 * set up maps for a direct process image (/proc)
 */
void
cmdmap(Map *map)
{
	int i;
	char name[MAXSYM];

	rdc();
	readsym(name);
	i = findseg(map, name, nil);
	if (i < 0)	/* not found */
		error("Invalid map name");

	if (expr(0)) {
	/*	if (strcmp(name, "text") == 0) */
	/*		textseg(expv, &fhdr); */
		map->seg[i].base = expv;
	} else
		error("Invalid base address");
	if (expr(0))
		map->seg[i].size = expv - map->seg[i].base;
	else
		error("Invalid end address");
	if (expr(0))
		map->seg[i].offset = expv;
	else
		error("Invalid file offset");
/*
	if (rdc()=='?' && map == cormap) {
		if (fcor)
			close(fcor);
		fcor=fsym;
		corfil = symfil;
		cormap = symmap;
	} else if (lastc == '/' && map == symmap) {
		if (fsym)
			close(fsym);
		fsym=fcor;
		symfil=corfil;
		symmap=cormap;
	} else
		reread();
*/
}

void
kmsys(void)
{
	int i;

	i = findseg(symmap, "text", symfil);
	if (i >= 0) {
		symmap->seg[i].base = symmap->seg[i].base&~mach->ktmask;
		symmap->seg[i].size = -symmap->seg[i].base;
	}
	i = findseg(symmap, "data", symfil);
	if (i >= 0) {
		symmap->seg[i].base = symmap->seg[i].base&~mach->ktmask;
		symmap->seg[i].size = -symmap->seg[i].base;
	}
}

void
attachprocess(void)
{
	if (!adrflg) {
		dprint("usage: pid$a\n");
		return;
	}
	pid = adrval;
	setcor();
}
