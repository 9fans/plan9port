#include <u.h>
#include <libc.h>
#include <mach.h>
#include "elf.h"

/*
aggr Linkdebug
{
	'X' 0 version;
	'X' 4 map;
};

aggr Linkmap
{
	'X' 0 addr;
	'X' 4 name;
	'X' 8 dynsect;
	'X' 12 next;
	'X' 16 prev;
};
*/
enum
{
	DT_NULL = 0,
	DT_NEEDED,
	DT_PLTRRELSZ,
	DT_PLTGOT,
	DT_HASH,
	DT_STRTAB,
	DT_SYMTAB,
	DT_RELA,
	DT_RELASZ = 8,
	DT_RELAENT,
	DT_STSZ,
	DT_SYMENT,
	DT_INIT,
	DT_FINI,
	DT_SONAME,
	DT_RPATH,
	DT_SYMBOLIC = 16,
	DT_REL,
	DT_RELSZ,
	DT_RELENT,
	DT_PLTREL,
	DT_DEBUG,
	DT_TEXTREL,
	DT_JMPREL
};

static int
getstr(Map *map, ulong addr, char *buf, uint nbuf)
{
	int i;

	for(i=0; i<nbuf; i++){
		if(get1(map, addr+i, (uchar*)buf+i, 1) < 0)
			return -1;
		if(buf[i] == 0)
			return 0;
	}
	return -1;	/* no nul */
}

static ulong
dyninfo(Fhdr *hdr, int x)
{
	u32int addr, u;

	if(hdr == nil || (addr = ((Elf*)hdr->elf)->dynamic) == 0){
		fprint(2, "no hdr/dynamic %p\n", hdr);
		return 0;
	}
	addr += hdr->base;

	while(addr != 0){
		if(get4(cormap, addr, &u) < 0)
			return 0;
		if(u == x){
			if(get4(cormap, addr+4, &u) < 0)
				return 0;
			return u;
		}
		addr += 8;
	}
	return 0;
}

void
elfdl386mapdl(int verbose)
{
	int i;
	Fhdr *hdr;
	u32int linkdebug, linkmap, name, addr;
	char buf[1024];

	if((linkdebug = dyninfo(symhdr, DT_DEBUG)) == 0){
		fprint(2, "no dt_debug section\n");
		return;
	}
	if(get4(cormap, linkdebug+4, &linkmap) < 0){
		fprint(2, "get4 linkdebug+4 (0x%lux) failed\n", linkdebug);
		return;
	}

	for(i=0; i<100 && linkmap != 0; i++){
		if(get4(cormap, linkmap, &addr) < 0
		|| get4(cormap, linkmap+4, &name) < 0
		|| get4(cormap, linkmap+12, &linkmap) < 0)
			break;

		if(name == 0 || getstr(cormap, name, buf, sizeof buf) < 0 || buf[0] == 0)
			continue;
		if((hdr = crackhdr(buf, OREAD)) == nil){
			fprint(2, "crackhdr %s: %r\n", buf);
			continue;
		}
		hdr->base = addr;
		if(verbose)
			fprint(2, "%s: %s %s %s\n", buf, hdr->aname, hdr->mname, hdr->fname);
		if(mapfile(hdr, addr, symmap, nil) < 0)
			fprint(2, "mapping %s: %r\n", buf);
		if(corhdr){
			/*
			 * Need to map the text file under the core file.
			 */
			unmapfile(corhdr, cormap);
			mapfile(hdr, addr, cormap, nil);
			mapfile(corhdr, 0, cormap, nil);
		}
		if(symopen(hdr) < 0)
			fprint(2, "syminit %s: %r\n", buf);
	}
}
