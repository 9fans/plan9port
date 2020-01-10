#include <u.h>
#include <libc.h>
#include <mach.h>
#include "elf.h"

static int
elfsyminit(Fhdr *fp)
{
	int i, onlyundef;
	Elf *elf;
	Symbol sym;
	ElfSym esym;
	ElfProg *p;

	elf = fp->elf;

	onlyundef = fp->nsym > 0;
	for(i=0; elfsym(elf, i, &esym) >= 0; i++){
		if(esym.name == nil)
			continue;
		if(onlyundef && esym.shndx != ElfSymShnNone)
			continue;
		if(esym.type != ElfSymTypeObject && esym.type != ElfSymTypeFunc)
			continue;
		if(strchr(esym.name, '@'))
			continue;
		memset(&sym, 0, sizeof sym);
		sym.name = esym.name;
		sym.loc.type = LADDR;
		sym.loc.addr = esym.value;
		if(esym.size){
			sym.hiloc.type = LADDR;
			sym.hiloc.addr = esym.value+esym.size;
		}
		sym.fhdr = fp;
		if(esym.type==ElfSymTypeObject){
			sym.class = CDATA;
			sym.type = 'D';
			if(&elf->sect[esym.shndx] == elf->bss)
				sym.type = 'B';
		}else if(esym.type==ElfSymTypeFunc){
			sym.class = CTEXT;
			sym.type = 'T';
		}
		if(esym.shndx == ElfSymShnNone)
			sym.type = 'U';
		if(esym.bind==ElfSymBindLocal)
			sym.type += 'a' - 'A';
		_addsym(fp, &sym);
	}

	for(i=0; i<elf->nprog; i++){
		p = &elf->prog[i];
		if(p->type != ElfProgDynamic)
			continue;
		elf->dynamic = p->vaddr;
		memset(&sym, 0, sizeof sym);
		sym.name = "_DYNAMIC";
		sym.loc = locaddr(p->vaddr);
		sym.hiloc = locaddr(p->vaddr+p->filesz);
		sym.type = 'D';
		sym.class = CDATA;
		_addsym(fp, &sym);
	}
	return 0;
}

int
elfsymlookup(Elf *elf, char *name, ulong *addr)
{
	int i;
	ElfSym esym;

	for(i=0; elfsym(elf, i, &esym) >= 0; i++){
		if(esym.name == nil)
			continue;
		if(strcmp(esym.name, name) == 0){
			*addr = esym.value;
			return 0;
		}
	}
	return -1;
}

int
symelf(Fhdr *fhdr)
{
	int ret;

	ret = -1;

	/* try dwarf */
	if(fhdr->dwarf){
		if(machdebug)
			fprint(2, "dwarf symbols...\n");
		if(symdwarf(fhdr) < 0)
			fprint(2, "initializing dwarf: %r");
		else
			ret = 0;
	}

	/* try stabs */
	if(fhdr->stabs.stabbase){
		if(machdebug)
			fprint(2, "stabs symbols...\n");
		if(symstabs(fhdr) < 0)
			fprint(2, "initializing stabs: %r");
		else
			ret = 0;
	}

	if(machdebug)
		fprint(2, "elf symbols...\n");

	if(elfsyminit(fhdr) < 0)
		fprint(2, "initializing elf: %r");
	else
		ret = 0;
	return ret;
}
