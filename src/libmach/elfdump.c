#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include "elf.h"
#include "stabs.h"

void
usage(void)
{
	fprint(2, "usage: elf file list\n");
	fprint(2, "	elf file syms\n");
	fprint(2, "	elf file prog n\n");
	fprint(2, "	elf file sect n\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, n, nn;
	char buf[512];
	ulong off, len;
	Elf *elf;
	ElfProg *p;
	ElfSect *s;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc < 2)
		usage();

	if((elf = elfopen(argv[0])) == nil)
		sysfatal("elfopen %s: %r", argv[0]);

	if(strcmp(argv[1], "syms") == 0){
		ElfSym sym;
		for(i=0; elfsym(elf, i, &sym) >= 0; i++){
			print("%s 0x%lux +%lud bind %d type %d other %d shndx 0x%ux\n",
				sym.name, (ulong)sym.value, (ulong)sym.size,
				sym.bind, sym.type, sym.other, (uint)sym.shndx);
		}
	}
	else if(strcmp(argv[1], "stabs") == 0){
		ElfSect *s1, *s2;
		Stab stabs;
		StabSym sym;

		if((s1 = elfsection(elf, ".stab")) == nil)
			sysfatal("no stabs");
		if(s1->link==0 || s1->link >= elf->nsect)
			sysfatal("bad stabstr %d", s1->link);
		s2 = &elf->sect[s1->link];
		if(elfmap(elf, s1) < 0 || elfmap(elf, s2) < 0)
			sysfatal("elfmap");
		stabs.stabbase = s1->base;
		stabs.stabsize = s1->size;
		stabs.strbase = s2->base;
		stabs.strsize = s2->size;
		stabs.e2 = elf->hdr.e2;
		stabs.e4 = elf->hdr.e4;
		print("%ud %ud\n", stabs.stabsize, stabs.strsize);
		for(i=0; stabsym(&stabs, i, &sym) >= 0; i++)
			print("%s type 0x%x other %d desc %d value 0x%lux\n",
				sym.name, sym.type, sym.other, (int)sym.desc, (ulong)sym.value);
		fprint(2, "err at %d: %r\n", i);
	}
	else if(strcmp(argv[1], "list") == 0){
		if(argc != 2)
			usage();
		print("elf %s %s v%d entry 0x%08lux phoff 0x%lux shoff 0x%lux flags 0x%lux\n",
			elftype(elf->hdr.type), elfmachine(elf->hdr.machine),
			elf->hdr.version, elf->hdr.entry, elf->hdr.phoff, elf->hdr.shoff,
			elf->hdr.flags);
		print("\tehsize %d phentsize %d phnum %d shentsize %d shnum %d shstrndx %d\n",
			elf->hdr.ehsize, elf->hdr.phentsize, elf->hdr.phnum, elf->hdr.shentsize,
			elf->hdr.shnum, elf->hdr.shstrndx);
		for(i=0; i<elf->nprog; i++){
			p = &elf->prog[i];
			print("prog %d type %d offset 0x%08lux vaddr 0x%08lux paddr 0x%08lux filesz 0x%08lux memsz 0x%08lux flags 0x%08lux align 0x%08lux\n",
				i, p->type, p->offset, p->vaddr, p->paddr,
				p->filesz, p->memsz, p->flags, p->align);
		}
		for(i=0; i<elf->nsect; i++){
			s = &elf->sect[i];
			print("sect %d %s type %d flags 0x%lux addr 0x%08lux offset 0x%08lux size 0x%08lux link 0x%lux info 0x%lux align 0x%lux entsize 0x%lux\n",
				i, s->name, s->type, s->flags, s->addr, s->offset, s->size, s->link, s->info,
				s->align, s->entsize);
		}
	}
	else if(strcmp(argv[1], "prog") == 0){
		if(argc != 3)
			usage();
		i = atoi(argv[2]);
		if(i < 0 || i >= elf->nprog)
			sysfatal("bad prog number");
		off = elf->prog[i].offset;
		len = elf->prog[i].filesz;
		fprint(2, "prog %d offset 0x%lux size 0x%lux\n", i, off, len);
	copy:
		seek(elf->fd, off, 0);
		for(n=0; n<len; n+=nn){
			nn = sizeof buf;
			if(nn > len-n)
				nn = len-n;
			nn = read(elf->fd, buf, nn);
			if(nn == 0)
				break;
			if(nn < 0)
				sysfatal("read error");
			write(1, buf, nn);
		}
		if(n < len)
			fprint(2, "early eof\n");
	}
	else if(strcmp(argv[1], "sect") == 0){
		if(argc != 3)
			usage();
		i = atoi(argv[2]);
		if(i < 0 || i >= elf->nsect)
			sysfatal("bad section number");
		off = elf->sect[i].offset;
		len = elf->sect[i].size;
		fprint(2, "section %d offset 0x%lux size 0x%lux\n", i, off, len);
		goto copy;
	}
	else
		usage();
	exits(0);
}
