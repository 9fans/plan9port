#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include "elf.h"


Mach *mach;

extern Mach mach386;
extern Mach machpower;

static Mach *machs[] =
{
	&mach386,
	&machpower,
};

Mach*
machbyname(char *name)
{
	int i;

	for(i=0; i<nelem(machs); i++)
		if(strcmp(machs[i]->name, name) == 0){
			mach = machs[i];
			return machs[i];
		}
	werrstr("machine '%s' not found", name);
	return nil;
}

static struct
{
	ulong magic;
	int (*fn)(int, Fhdr*);
} cracktab[] = {
	0x7F454C46,	crackelf,
	0xFEEDFACE,	crackmacho,
};

Fhdr*
crackhdr(char *name, int mode)
{
	uchar buf[4];
	ulong magic;
	int i, fd;
	Fhdr *hdr;

	if((fd = open(name, mode)) < 0)
		return nil;

	if(seek(fd, 0, 0) < 0 || readn(fd, buf, 4) != 4){
		close(fd);
		return nil;
	}

	hdr = mallocz(sizeof(Fhdr), 1);
	if(hdr == nil){
		close(fd);
		return nil;
	}
	hdr->filename = strdup(name);
	magic = beload4(buf);
	werrstr("magic doesn't match");
	for(i=0; i<nelem(cracktab); i++)
		if(cracktab[i].magic == magic && seek(fd, 0, 0) == 0 && cracktab[i].fn(fd, hdr) >= 0){
			_addhdr(hdr);
			return hdr;
		}
	werrstr("unknown file type: %r");
	free(hdr->filename);
	free(hdr);
	close(fd);
	return nil;
}

void
uncrackhdr(Fhdr *hdr)
{
	int i;

	symclose(hdr);
	if(hdr->elf)
		elfclose(hdr->elf);
	if(hdr->fd >= 0)
		close(hdr->fd);
	free(hdr->cmdline);
	free(hdr->prog);
	for(i=0; i<hdr->nthread; i++)
		free(hdr->thread[i].ureg);
	free(hdr->thread);
	free(hdr->filename);
	free(hdr);
}

int
mapfile(Fhdr *fp, u64int base, Map *map, Regs **regs)
{
	if(fp == nil){
		werrstr("no file");
		return -1;
	}
	if(map == nil){
		werrstr("no map");
		return -1;
	}
	if(fp->map == 0){
		werrstr("cannot load map for this file type");
		return -1;
	}
	if(regs)
		*regs = nil;
	return fp->map(fp, base, map, regs);
}

void
unmapfile(Fhdr *fp, Map *map)
{
	int i;

	if(map == nil || fp == nil)
		return;

	for(i=0; i<map->nseg; i++){
		while(i<map->nseg && map->seg[i].fd == fp->fd){
			map->nseg--;
			memmove(&map->seg[i], &map->seg[i+1],
				(map->nseg-i)*sizeof(map->seg[0]));
		}
	}
}

Regs*
coreregs(Fhdr *fp, uint id)
{
	UregRegs *r;
	int i;

	for(i=0; i<fp->nthread; i++){
		if(fp->thread[i].id == id){
			if((r = mallocz(sizeof *r, 1)) == nil)
				return nil;
			r->r.rw = _uregrw;
			r->ureg = fp->thread[i].ureg;
			return &r->r;
		}
	}
	werrstr("thread not found");
	return nil;
}
