#include <u.h>
#include <libc.h>
#include <mach.h>
#include "elf.h"
#include "dwarf.h"

static int mapelf(Fhdr *fp, u64int base, Map *map, Regs**);
static int unpacknote(Elf *elf, uchar *a, uchar *ea, ElfNote *note, uchar **pa);

static struct
{
	uint etype;
	uint mtype;
	Mach *mach;
	char *name;
} mtab[] =
{	/* Font Tab 4 */
	ElfMachSparc,	MSPARC,		nil,		"sparc",
	ElfMach386,		M386,		&mach386,	"386",
	ElfMachMips,	MMIPS,		nil,		"mips",
	ElfMachArm,		MARM,		nil,		"arm",
	ElfMachPower,	MPOWER,		nil,		"powerpc",
	ElfMachPower64,	MNONE,		nil,		"powerpc64",
	ElfMachAmd64,	MAMD64,	&machamd64,	"amd64",
};

static struct
{
	uint etype;
	uint atype;
	char *aname;
} atab[] =
{	/* Font Tab 4 */
	ElfAbiSystemV,	ALINUX,		"linux",	/* [sic] */
	ElfAbiLinux,	ALINUX,		"linux",
	ElfAbiFreeBSD,	AFREEBSD,	"freebsd",
};

static struct
{
	uint mtype;
	uint atype;
	void (*elfcore)(Fhdr*, Elf*, ElfNote*);
} ctab[] =
{	/* Font Tab 4 */
	M386,		ALINUX,		elfcorelinux386,
	M386,		ANONE,		elfcorelinux386,	/* [sic] */
//	M386,		AFREEBSD,	elfcorefreebsd386,
	MAMD64,		AFREEBSD,	elfcorefreebsdamd64,
};

int
crackelf(int fd, Fhdr *fp)
{
	uchar *a, *sa, *ea;
	int i, havetext, havedata, n;
	Elf *elf;
	ElfNote note;
	ElfProg *p;
	ElfSect *s1, *s2;
	void (*elfcore)(Fhdr*, Elf*, ElfNote*);

	if((elf = elfinit(fd)) == nil)
		return -1;

	fp->fd = fd;
	fp->elf = elf;
	fp->dwarf = dwarfopen(elf);	/* okay to fail */
	fp->syminit = symelf;

	if((s1 = elfsection(elf, ".stab")) != nil && s1->link!=0 && s1->link < elf->nsect){
		s2 = &elf->sect[s1->link];
		if(elfmap(elf, s1) >= 0 && elfmap(elf, s2) >= 0){
			fp->stabs.stabbase = s1->base;
			fp->stabs.stabsize = s1->size;
			fp->stabs.strbase = (char*)s2->base;
			fp->stabs.strsize = s2->size;
			fp->stabs.e2 = elf->hdr.e2;
			fp->stabs.e4 = elf->hdr.e4;
		}
	}

	for(i=0; i<nelem(mtab); i++){
		if(elf->hdr.machine != mtab[i].etype)
			continue;
		fp->mach = mtab[i].mach;
		fp->mname = mtab[i].name;
		fp->mtype = mtab[i].mtype;
		break;
	}
	if(i == nelem(mtab)){
		werrstr("unsupported machine type %d", elf->hdr.machine);
		goto err;
	}

	if(mach == nil)
		mach = fp->mach;

	fp->aname = "unknown";
	for(i=0; i<nelem(atab); i++){
		if(elf->hdr.abi != atab[i].etype)
			continue;
		fp->atype = atab[i].atype;
		fp->aname = atab[i].aname;
		break;
	}

	switch(elf->hdr.type){
	default:
		werrstr("unknown file type %d", elf->hdr.type);
		goto err;
	case ElfTypeExecutable:
		fp->ftype = FEXEC;
		fp->fname = "executable";
		break;
	case ElfTypeRelocatable:
		fp->ftype = FRELOC;
		fp->fname = "relocatable";
		break;
	case ElfTypeSharedObject:
		fp->ftype = FSHOBJ;
		fp->fname = "shared object";
		break;
	case ElfTypeCore:
		fp->ftype = FCORE;
		fp->fname = "core dump";
		break;
	}

	fp->map = mapelf;

	if(fp->ftype == FCORE){
		elfcore = nil;
		for(i=0; i<nelem(ctab); i++){
			if(ctab[i].atype != fp->atype
			|| ctab[i].mtype != fp->mtype)
				continue;
			elfcore = ctab[i].elfcore;
			break;
		}
		if(elfcore)
		for(i=0; i<elf->nprog; i++){
			p = &elf->prog[i];
			if(p->type != ElfProgNote)
				continue;
			n = p->filesz;
			a = malloc(n);
			if(a == nil)
				goto err;
			if(seek(fp->fd, p->offset, 0) < 0 || readn(fp->fd, a, n) != n){
				free(a);
				continue;
			}
			sa = a;
			ea = a+n;
			while(a < ea){
				note.offset = (a-sa) + p->offset;
				if(unpacknote(elf, a, ea, &note, &a) < 0)
					break;
				elfcore(fp, elf, &note);
			}
			free(sa);
		}
		return 0;
	}

	fp->entry = elf->hdr.entry;

	/* First r-x section we find is the text and initialized data */
	/* First rw- section we find is the r/w data */
	havetext = 0;
	havedata = 0;
	for(i=0; i<elf->nprog; i++){
		p = &elf->prog[i];
		if(p->type != ElfProgLoad)
			continue;
		if(!havetext && p->flags == (ElfProgFlagRead|ElfProgFlagExec) && p->align >= mach->pgsize){
			havetext = 1;
			fp->txtaddr = p->vaddr;
			fp->txtsz = p->memsz;
			fp->txtoff = p->offset;
		}
		if(!havedata && p->flags == (ElfProgFlagRead|ElfProgFlagWrite) && p->align >= mach->pgsize){
			havedata = 1;
			fp->dataddr = p->vaddr;
			fp->datsz = p->filesz;
			fp->datoff = p->offset;
			fp->bsssz = p->memsz - p->filesz;
		}
	}
	if(!havetext){
		werrstr("did not find text segment in elf binary");
		goto err;
	}
	if(!havedata){
		werrstr("did not find data segment in elf binary");
		goto err;
	}
	return 0;

err:
	elfclose(elf);
	return -1;
}

static int
mapelf(Fhdr *fp, u64int base, Map *map, Regs **regs)
{
	int i;
	Elf *elf;
	ElfProg *p;
	u64int sz;
	u64int lim;
	Seg s;

	elf = fp->elf;
	if(elf == nil){
		werrstr("not an elf file");
		return -1;
	}

	for(i=0; i<elf->nprog; i++){
		p = &elf->prog[i];
		if(p->type != ElfProgLoad)
			continue;
		if(p->align < mach->pgsize)
			continue;
		if(p->filesz){
			memset(&s, 0, sizeof s);
			s.file = fp->filename;
			s.fd = fp->fd;
			if(fp->ftype == FCORE)
				s.name = "core";
			else if(p->flags == 5)
				s.name = "text";
			else
				s.name = "data";
			s.base = base+p->vaddr;
			s.size = p->filesz;
			s.offset = p->offset;
			if(addseg(map, s) < 0)
				return -1;
		}
		/*
		 * If memsz > filesz, we're supposed to zero fill.
		 * Core files have zeroed sections where the pages
		 * can be filled in from the text file, so if this is a core
		 * we only fill in that which isn't yet mapped.
		 */
		if(fp->ftype == FCORE){
			sz = p->filesz;
			while(sz < p->memsz){
				if(addrtoseg(map, base+p->vaddr+sz, &s) < 0){
					lim = base + p->vaddr + p->memsz;
					if(addrtosegafter(map, base+p->vaddr+sz, &s) >= 0 && s.base < lim)
						lim = s.base;
					memset(&s, 0, sizeof s);
					s.name = "zero";
					s.base = base + p->vaddr + sz;
					s.size = lim - s.base;
					s.offset = p->offset;
					if(addseg(map, s) < 0)
						return -1;
				}else
					sz = (s.base+s.size) - (base + p->vaddr);
			}
		}else{
			if(p->filesz < p->memsz){
				memset(&s, 0, sizeof s);
				s.name = "zero";
				s.base = base + p->vaddr + p->filesz;
				s.size = p->memsz - p->filesz;
				if(addseg(map, s) < 0)
					return -1;
			}
		}
	}

	if(fp->nthread && regs)
		*regs = coreregs(fp, fp->thread[0].id);

	return 0;
}

static int
unpacknote(Elf *elf, uchar *a, uchar *ea, ElfNote *note, uchar **pa)
{
	if(a+12 > ea)
		return -1;
	note->namesz = elf->hdr.e4(a);
	note->descsz = elf->hdr.e4(a+4);
	note->type = elf->hdr.e4(a+8);
	a += 12;
	note->name = (char*)a;
/* XXX fetch alignment constants from elsewhere */
	a += (note->namesz+3)&~3;
	note->desc = (uchar*)a;
	a += (note->descsz+3)&~3;
	if(a > ea)
		return -1;
	*pa = a;
	return 0;
}
