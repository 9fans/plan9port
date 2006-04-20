/*
 * Parse 32-bit ELF files.
 * Copyright (c) 2004 Russ Cox.  See LICENSE.
 */

#include <u.h>
#include <libc.h>
#include <mach.h>
#include "elf.h"

typedef struct ElfHdrBytes ElfHdrBytes;
typedef struct ElfSectBytes ElfSectBytes;
typedef struct ElfProgBytes ElfProgBytes;
typedef struct ElfSymBytes ElfSymBytes;

struct ElfHdrBytes
{
	uchar	ident[16];
	uchar	type[2];
	uchar	machine[2];
	uchar	version[4];
	uchar	entry[4];
	uchar	phoff[4];
	uchar	shoff[4];
	uchar	flags[4];
	uchar	ehsize[2];
	uchar	phentsize[2];
	uchar	phnum[2];
	uchar	shentsize[2];
	uchar	shnum[2];
	uchar	shstrndx[2];
};

struct ElfSectBytes
{
	uchar	name[4];
	uchar	type[4];
	uchar	flags[4];
	uchar	addr[4];
	uchar	offset[4];
	uchar	size[4];
	uchar	link[4];
	uchar	info[4];
	uchar	align[4];
	uchar	entsize[4];
};

struct ElfSymBytes
{
	uchar	name[4];
	uchar	value[4];
	uchar	size[4];
	uchar	info;	/* top4: bind, bottom4: type */
	uchar	other;
	uchar	shndx[2];
};

struct ElfProgBytes
{
	uchar	type[4];
	uchar	offset[4];
	uchar	vaddr[4];
	uchar	paddr[4];
	uchar	filesz[4];
	uchar	memsz[4];
	uchar	flags[4];
	uchar	align[4];
};

uchar ElfMagic[4] = { 0x7F, 'E', 'L', 'F' };

static void	unpackhdr(ElfHdr*, ElfHdrBytes*);
static void	unpackprog(ElfHdr*, ElfProg*, ElfProgBytes*);
static void	unpacksect(ElfHdr*, ElfSect*, ElfSectBytes*);

static char *elftypes[] = {
	"none",
	"relocatable",
	"executable",
	"shared object",
	"core",
};

char*
elftype(int t)
{
	if(t < 0 || t >= nelem(elftypes))
		return "unknown";
	return elftypes[t];
}

static char *elfmachs[] = {
	"none",
	"32100",
	"sparc",
	"386",
	"68000",
	"88000",
	"486",
	"860",
	"MIPS",
};

char*
elfmachine(int t)
{
	if(t < 0 || t >= nelem(elfmachs))
		return "unknown";
	return elfmachs[t];
}

Elf*
elfopen(char *name)
{
	int fd;
	Elf *e;

	if((fd = open(name, OREAD)) < 0)
		return nil;
	if((e = elfinit(fd)) == nil)
		close(fd);
	return e;
}

Elf*
elfinit(int fd)
{
	int i;
	Elf *e;
	ElfHdr *h;
	ElfHdrBytes hdrb;
	ElfProgBytes progb;
	ElfSectBytes sectb;
	ElfSect *s;

	e = mallocz(sizeof(Elf), 1);
	if(e == nil)
		return nil;
	e->fd = fd;

	/*
	 * parse header
	 */
	seek(fd, 0, 0);
	if(readn(fd, &hdrb, sizeof hdrb) != sizeof hdrb)
		goto err;
	h = &e->hdr;
	unpackhdr(h, &hdrb);
	if(h->class != ElfClass32){
		werrstr("bad ELF class - not 32-bit");
		goto err;
	}
	if(h->encoding != ElfDataLsb && h->encoding != ElfDataMsb){
		werrstr("bad ELF encoding - not LSB, MSB");
		goto err;
	}
	if(hdrb.ident[6] != h->version){
		werrstr("bad ELF encoding - version mismatch %02ux and %08ux",
			(uint)hdrb.ident[6], (uint)h->version);
		goto err;
	}

	/*
	 * the prog+section info is almost always small - just load it into memory.
	 */
	e->nprog = h->phnum;
	e->prog = mallocz(sizeof(ElfProg)*e->nprog, 1);
	for(i=0; i<e->nprog; i++){
		if(seek(fd, h->phoff+i*h->phentsize, 0) < 0
		|| readn(fd, &progb, sizeof progb) != sizeof progb)
			goto err;
		unpackprog(h, &e->prog[i], &progb);
	}

	e->nsect = h->shnum;
	if(e->nsect == 0)
		goto nosects;
	e->sect = mallocz(sizeof(ElfSect)*e->nsect, 1);
	for(i=0; i<e->nsect; i++){
		if(seek(fd, h->shoff+i*h->shentsize, 0) < 0
		|| readn(fd, &sectb, sizeof sectb) != sizeof sectb)
			goto err;
		unpacksect(h, &e->sect[i], &sectb);
	}

	if(h->shstrndx >= e->nsect){
		fprint(2, "warning: bad string section index %d >= %d", h->shstrndx, e->nsect);
		h->shnum = 0;
		e->nsect = 0;
		goto nosects;
	}
	s = &e->sect[h->shstrndx];
	if(elfmap(e, s) < 0)
		goto err;

	for(i=0; i<e->nsect; i++)
		if(e->sect[i].name)
			e->sect[i].name = (char*)s->base + (ulong)e->sect[i].name;

	e->symtab = elfsection(e, ".symtab");
	if(e->symtab){
		if(e->symtab->link >= e->nsect)
			e->symtab = nil;
		else{
			e->symstr = &e->sect[e->symtab->link];
			e->nsymtab = e->symtab->size / sizeof(ElfSymBytes);
		}
	}
	e->dynsym = elfsection(e, ".dynsym");
	if(e->dynsym){
		if(e->dynsym->link >= e->nsect)
			e->dynsym = nil;
		else{
			e->dynstr = &e->sect[e->dynsym->link];
			e->ndynsym = e->dynsym->size / sizeof(ElfSymBytes);
		}
	}

	e->bss = elfsection(e, ".bss");

nosects:
	return e;

err:
	free(e->sect);
	free(e->prog);
	free(e->shstrtab);
	free(e);
	return nil;
}

void
elfclose(Elf *elf)
{
	int i;

	for(i=0; i<elf->nsect; i++)
		free(elf->sect[i].base);
	free(elf->sect);
	free(elf->prog);
	free(elf->shstrtab);
	free(elf);
}

static void
unpackhdr(ElfHdr *h, ElfHdrBytes *b)
{
	u16int (*e2)(uchar*);
	u32int (*e4)(uchar*);
	u64int (*e8)(uchar*);

	memmove(h->magic, b->ident, 4);
	h->class = b->ident[4];
	h->encoding = b->ident[5];
	switch(h->encoding){
	case ElfDataLsb:
		e2 = leload2;
		e4 = leload4;
		e8 = leload8;
		break;
	case ElfDataMsb:
		e2 = beload2;
		e4 = beload4;
		e8 = beload8;
		break;
	default:
		return;
	}
	h->abi = b->ident[7];
	h->abiversion = b->ident[8];

	h->e2 = e2;
	h->e4 = e4;
	h->e8 = e8;
	
	h->type = e2(b->type);
	h->machine = e2(b->machine);
	h->version = e4(b->version);
	h->entry = e4(b->entry);
	h->phoff = e4(b->phoff);
	h->shoff = e4(b->shoff);
	h->flags = e4(b->flags);
	h->ehsize = e2(b->ehsize);
	h->phentsize = e2(b->phentsize);
	h->phnum = e2(b->phnum);
	h->shentsize = e2(b->shentsize);
	h->shnum = e2(b->shnum);
	h->shstrndx = e2(b->shstrndx);
}

static void
unpackprog(ElfHdr *h, ElfProg *p, ElfProgBytes *b)
{
	u32int (*e4)(uchar*);

	e4 = h->e4;
	p->type = e4(b->type);
	p->offset = e4(b->offset);
	p->vaddr = e4(b->vaddr);
	p->paddr = e4(b->paddr);
	p->filesz = e4(b->filesz);
	p->memsz = e4(b->memsz);
	p->flags = e4(b->flags);
	p->align = e4(b->align);
}

static void
unpacksect(ElfHdr *h, ElfSect *s, ElfSectBytes *b)
{
	u32int (*e4)(uchar*);

	e4 = h->e4;
	s->name = (char*)(uintptr)e4(b->name);
	s->type = e4(b->type);
	s->flags = e4(b->flags);
	s->addr = e4(b->addr);
	s->offset = e4(b->offset);
	s->size = e4(b->size);
	s->link = e4(b->link);
	s->info = e4(b->info);
	s->align = e4(b->align);
	s->entsize = e4(b->entsize);
}

ElfSect*
elfsection(Elf *elf, char *name)
{
	int i;

	for(i=0; i<elf->nsect; i++){
		if(elf->sect[i].name == name)
			return &elf->sect[i];
		if(elf->sect[i].name && name
		&& strcmp(elf->sect[i].name, name) == 0)
			return &elf->sect[i];
	}
	werrstr("elf section '%s' not found", name);
	return nil;
}

int
elfmap(Elf *elf, ElfSect *sect)
{
	if(sect->base)
		return 0;
	if((sect->base = malloc(sect->size)) == nil)
		return -1;
	werrstr("short read");
	if(seek(elf->fd, sect->offset, 0) < 0
	|| readn(elf->fd, sect->base, sect->size) != sect->size){
		free(sect->base);
		sect->base = nil;
		return -1;
	}
	return 0;
}

int
elfsym(Elf *elf, int i, ElfSym *sym)
{
	ElfSect *symtab, *strtab;
	uchar *p;
	char *s;
	ulong x;

	if(i < 0){
		werrstr("bad index %d in elfsym", i);
		return -1;
	}

	if(i < elf->nsymtab){
		symtab = elf->symtab;
		strtab = elf->symstr;
	extract:
		if(elfmap(elf, symtab) < 0 || elfmap(elf, strtab) < 0)
			return -1;
		p = symtab->base + i * sizeof(ElfSymBytes);
		s = (char*)strtab->base;
		x = elf->hdr.e4(p);
		if(x >= strtab->size){
			werrstr("bad symbol name offset 0x%lux", x);
			return -1;
		}
		sym->name = s + x;
		sym->value = elf->hdr.e4(p+4);
		sym->size = elf->hdr.e4(p+8);
		x = p[12];
		sym->bind = x>>4;
		sym->type = x & 0xF;
		sym->other = p[13];
		sym->shndx = elf->hdr.e2(p+14);
		return 0;
	}
	i -= elf->nsymtab;
	if(i < elf->ndynsym){
		symtab = elf->dynsym;
		strtab = elf->dynstr;
		goto extract;
	}
	/* i -= elf->ndynsym */

	werrstr("symbol index out of range");
	return -1;
}

