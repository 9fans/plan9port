#include <u.h>
#include <libc.h>
#include <mach.h>
#include "macho.h"

static int mapmacho(Fhdr *fp, u64int base, Map *map, Regs**);

static struct
{
	uint etype;
	uint mtype;
	Mach *mach;
	char *name;
	int (*coreregs)(Macho*, uchar**);
} mtab[] =
{
	MachoCpuPower,	MPOWER,		&machpower, 		"powerpc",	coreregsmachopower,
};

static uchar*
load(int fd, ulong off, int size)
{
	uchar *a;

	a = malloc(size);
	if(a == nil)
		return nil;
	if(seek(fd, off, 0) < 0 || readn(fd, a, size) != size){
		free(a);
		return nil;
	}
	return a;
}

int
crackmacho(int fd, Fhdr *fp)
{
	int i;
	Macho *m;

	if((m = machoinit(fd)) == nil)
		return -1;

	fp->fd = fd;
	fp->macho = m;

	for(i=0; i<nelem(mtab); i++){
		if(m->cputype != mtab[i].etype)
			continue;
		fp->mach = mtab[i].mach;
		fp->mtype = mtab[i].mtype;
		fp->mname = mtab[i].name;
		m->coreregs = mtab[i].coreregs;
		break;
	}
	if(i == nelem(mtab)){
		werrstr("unsupported cpu type %ud", m->cputype);
		goto err;
	}

	fp->atype = AMACH;
	fp->aname = "mach";

	if(mach == nil)
		mach = fp->mach;

	switch(m->filetype){
	default:
		werrstr("unsupported macho file type %lud", m->filetype);
		goto err;
	case MachoFileObject:
		fp->ftype = FOBJ;
		fp->fname = "object";
		break;
	case MachoFileExecutable:
		fp->ftype = FEXEC;
		fp->fname = "executable";
		break;
	case MachoFileFvmlib:
		fp->ftype = FSHLIB;
		fp->fname = "shared library";
		break;
	case MachoFileCore:
		fp->ftype = FCORE;
		fp->fname = "core";
		break;
	case MachoFilePreload:
		fp->ftype = FBOOT;
		fp->fname = "preloaded executable";
		break;
	}

	fp->txtaddr = fp->dataddr = 0;
	fp->txtsz = fp->datsz = 0;
	for(i=0; i<m->ncmd; i++){
		if(m->cmd[i].type != MachoCmdSegment)
			continue;
		if(strcmp(m->cmd[i].seg.name, "__TEXT") == 0){
			fp->txtaddr = m->cmd[i].seg.vmaddr;
			fp->txtsz = m->cmd[i].seg.vmsize;
			fp->txtoff = m->cmd[i].seg.fileoff;
		}
		if(strcmp(m->cmd[i].seg.name, "__DATA") == 0){
			fp->dataddr = m->cmd[i].seg.vmaddr;
			fp->datsz = m->cmd[i].seg.filesz;
			fp->datoff = m->cmd[i].seg.fileoff;
			fp->bsssz = m->cmd[i].seg.vmsize - fp->datsz;
		}
	}

	fp->map = mapmacho;
	fp->syminit = symmacho;

	for(i=0; i<m->ncmd; i++)
		if(m->cmd[i].type == MachoCmdSymtab)
			break;
	if(i < m->ncmd){
		fp->stabs.stabbase = load(fp->fd, m->cmd[i].sym.symoff, m->cmd[i].sym.nsym*16);
		fp->stabs.stabsize = m->cmd[i].sym.nsym*16;
		fp->stabs.strbase = (char*)load(fp->fd, m->cmd[i].sym.stroff, m->cmd[i].sym.strsize);
		if(fp->stabs.stabbase == nil || fp->stabs.strbase == nil){
			fp->stabs.stabbase = nil;
			fp->stabs.strbase = nil;
		}else{
			fp->stabs.strsize = m->cmd[i].sym.strsize;
			fp->stabs.e2 = (m->e4==beload4 ? beload2 : leload2);
			fp->stabs.e4 = m->e4;
		}
	}

	return 0;

err:
	machoclose(m);
	return -1;
}

static int
mapmacho(Fhdr *fp, u64int base, Map *map, Regs **rp)
{
	int i, n;
	uchar *u;
	Macho *m;
	MachoCmd *c;
	Seg s;
	UregRegs *r;

	m = fp->macho;
	if(m == nil){
		werrstr("not a macho file");
		return -1;
	}

	for(i=0; i<m->ncmd; i++){
		c = &m->cmd[i];
		if(c->type != MachoCmdSegment)
			continue;
		if(c->seg.filesz){
			memset(&s, 0, sizeof s);
			s.file = fp->filename;
			s.fd = fp->fd;
			if(fp->ftype == FCORE)
				s.name = "core";
			else if(strcmp(c->seg.name, "__DATA") == 0)
				s.name = "data";
			else
				s.name = "text";
			s.base = base+c->seg.vmaddr;
			s.size = c->seg.filesz;
			s.offset = c->seg.fileoff;
			if(addseg(map, s) < 0)
				return -1;
		}
		if(c->seg.filesz < c->seg.vmsize){
			memset(&s, 0, sizeof s);
			s.name = "zero";
			s.base = base + c->seg.vmaddr + c->seg.filesz;
			s.size = c->seg.vmsize - c->seg.filesz;
			if(addseg(map, s) < 0)
				return -1;
		}
	}

	if(fp->ftype == FCORE && m->coreregs){
		n = m->coreregs(m, &u);
		if(n < 0){
			fprint(2, "mapping registers: %r\n");
			goto noregs;
		}
		if((r = mallocz(sizeof *r, 1)) == nil)
			return -1;
		r->r.rw = _uregrw;
		r->ureg = u;
		*rp = &r->r;
	}
noregs:
	return 0;
}
