#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

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
	free(hdr);
	close(fd);
	return nil;
}

void
uncrackhdr(Fhdr *hdr)
{
	close(hdr->fd);
	_delhdr(hdr);
	free(hdr->cmd);
	free(hdr);
}

int
mapfile(Fhdr *fp, ulong base, Map *map, Regs **regs)
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
