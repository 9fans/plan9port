#include <u.h>
#include <libc.h>
#include <mach.h>
#include "macho.h"

/*
http://www.channelu.com/NeXT/NeXTStep/3.3/nd/DevTools/14_MachO/MachO.htmld/
*/

Macho*
machoopen(char *name)
{
	int fd;
	Macho *m;

	if((fd = open(name, OREAD)) < 0)
		return nil;
	m = machoinit(fd);
	if(m == nil)
		close(fd);
	return m;
}

static int
unpackcmd(uchar *p, Macho *m, MachoCmd *c, uint type, uint sz)
{
	uint32 (*e4)(uchar*);
	uint64 (*e8)(uchar*);
	MachoSect *s;
	int i;

	e4 = m->e4;
	e8 = m->e8;

	c->type = type;
	c->size = sz;
	switch(type){
	default:
		return -1;
	case MachoCmdSegment:
		if(sz < 56)
			return -1;
		strecpy(c->seg.name, c->seg.name+sizeof c->seg.name, (char*)p+8);
		c->seg.vmaddr = e4(p+24);
		c->seg.vmsize = e4(p+28);
		c->seg.fileoff = e4(p+32);
		c->seg.filesz = e4(p+36);
		c->seg.maxprot = e4(p+40);
		c->seg.initprot = e4(p+44);
		c->seg.nsect = e4(p+48);
		c->seg.flags = e4(p+52);
		c->seg.sect = mallocz(c->seg.nsect * sizeof c->seg.sect[0], 1);
		if(c->seg.sect == nil)
			return -1;
		if(sz < 56+c->seg.nsect*68)
			return -1;
		p += 56;
		for(i=0; i<c->seg.nsect; i++) {
			s = &c->seg.sect[i];
			strecpy(s->name, s->name+sizeof s->name, (char*)p+0);
			strecpy(s->segname, s->segname+sizeof s->segname, (char*)p+16);
			s->addr = e4(p+32);
			s->size = e4(p+36);
			s->offset = e4(p+40);
			s->align = e4(p+44);
			s->reloff = e4(p+48);
			s->nreloc = e4(p+52);
			s->flags = e4(p+56);
			// p+60 and p+64 are reserved
			p += 68;
		}
		break;
	case MachoCmdSegment64:
		if(sz < 72)
			return -1;
		strecpy(c->seg.name, c->seg.name+sizeof c->seg.name, (char*)p+8);
		c->seg.vmaddr = e8(p+24);
		c->seg.vmsize = e8(p+32);
		c->seg.fileoff = e8(p+40);
		c->seg.filesz = e8(p+48);
		c->seg.maxprot = e4(p+56);
		c->seg.initprot = e4(p+60);
		c->seg.nsect = e4(p+64);
		c->seg.flags = e4(p+68);
		c->seg.sect = mallocz(c->seg.nsect * sizeof c->seg.sect[0], 1);
		if(c->seg.sect == nil)
			return -1;
		if(sz < 72+c->seg.nsect*80)
			return -1;
		p += 72;
		for(i=0; i<c->seg.nsect; i++) {
			s = &c->seg.sect[i];
			strecpy(s->name, s->name+sizeof s->name, (char*)p+0);
			strecpy(s->segname, s->segname+sizeof s->segname, (char*)p+16);
			s->addr = e8(p+32);
			s->size = e8(p+40);
			s->offset = e4(p+48);
			s->align = e4(p+52);
			s->reloff = e4(p+56);
			s->nreloc = e4(p+60);
			s->flags = e4(p+64);
			// p+68, p+72, and p+76 are reserved
			p += 80;
		}
		break;
	case MachoCmdSymtab:
		if(sz < 24)
			return -1;
		c->sym.symoff = e4(p+8);
		c->sym.nsym = e4(p+12);
		c->sym.stroff = e4(p+16);
		c->sym.strsize = e4(p+20);
		break;
	case MachoCmdDysymtab:
		if(sz < 80)
			return -1;
		c->dsym.ilocalsym = e4(p+8);
		c->dsym.nlocalsym = e4(p+12);
		c->dsym.iextdefsym = e4(p+16);
		c->dsym.nextdefsym = e4(p+20);
		c->dsym.iundefsym = e4(p+24);
		c->dsym.nundefsym = e4(p+28);
		c->dsym.tocoff = e4(p+32);
		c->dsym.ntoc = e4(p+36);
		c->dsym.modtaboff = e4(p+40);
		c->dsym.nmodtab = e4(p+44);
		c->dsym.extrefsymoff = e4(p+48);
		c->dsym.nextrefsyms = e4(p+52);
		c->dsym.indirectsymoff = e4(p+56);
		c->dsym.nindirectsyms = e4(p+60);
		c->dsym.extreloff = e4(p+64);
		c->dsym.nextrel = e4(p+68);
		c->dsym.locreloff = e4(p+72);
		c->dsym.nlocrel = e4(p+76);
		break;
	}
	return 0;
}

int
macholoadrel(Macho *m, MachoSect *sect)
{
	MachoRel *rel, *r;
	uchar *buf, *p;
	int i, n;
	uint32 v;

	if(sect->rel != nil || sect->nreloc == 0)
		return 0;
	rel = mallocz(sect->nreloc * sizeof r[0], 1);
	if(rel == nil)
		return -1;
	n = sect->nreloc * 8;
	buf = mallocz(n, 1);
	if(buf == nil) {
		free(rel);
		return -1;
	}
	if(seek(m->fd, sect->reloff, 0) < 0 || readn(m->fd, buf, n) != n) {
		free(rel);
		free(buf);
		return -1;
	}
	for(i=0; i<sect->nreloc; i++) {
		r = &rel[i];
		p = buf+i*8;
		r->addr = m->e4(p);

		// TODO(rsc): Wrong interpretation for big-endian bitfields?
		v = m->e4(p+4);
		r->symnum = v & 0xFFFFFF;
		v >>= 24;
		r->pcrel = v&1;
		v >>= 1;
		r->length = 1<<(v&3);
		v >>= 2;
		r->extrn = v&1;
		v >>= 1;
		r->type = v;
	}
	sect->rel = rel;
	free(buf);
	return 0;
}

int
macholoadsym(Macho *m, MachoSymtab *symtab)
{
	char *strbuf;
	uchar *symbuf, *p;
	int i, n, symsize;
	MachoSym *sym, *s;
	uint32 v;

	if(symtab->sym != nil)
		return 0;

	strbuf = mallocz(symtab->strsize, 1);
	if(strbuf == nil)
		return -1;
	if(seek(m->fd, symtab->stroff, 0) < 0 || readn(m->fd, strbuf, symtab->strsize) != symtab->strsize) {
		free(strbuf);
		return -1;
	}

	symsize = 12;
	if(m->is64)
		symsize = 16;
	n = symtab->nsym * symsize;
	symbuf = mallocz(n, 1);
	if(symbuf == nil) {
		free(strbuf);
		return -1;
	}
	if(seek(m->fd, symtab->symoff, 0) < 0 || readn(m->fd, symbuf, n) != n) {
		free(strbuf);
		free(symbuf);
		return -1;
	}
	sym = mallocz(symtab->nsym * sizeof sym[0], 1);
	if(sym == nil) {
		free(strbuf);
		free(symbuf);
		return -1;
	}
	p = symbuf;
	for(i=0; i<symtab->nsym; i++) {
		s = &sym[i];
		v = m->e4(p);
		if(v >= symtab->strsize) {
			free(strbuf);
			free(symbuf);
			free(sym);
			return -1;
		}
		s->name = strbuf + v;
		s->type = p[4];
		s->sectnum = p[5];
		s->desc = m->e2(p+6);
		if(m->is64)
			s->value = m->e8(p+8);
		else
			s->value = m->e4(p+8);
		p += symsize;
	}
	symtab->str = strbuf;
	symtab->sym = sym;
	free(symbuf);
	return 0;
}

Macho*
machoinit(int fd)
{
	int i, is64;
	uchar hdr[7*4], *cmdp;
	uchar tmp[4];
	uint16 (*e2)(uchar*);
	uint32 (*e4)(uchar*);
	uint64 (*e8)(uchar*);
	ulong ncmd, cmdsz, ty, sz, off;
	Macho *m;

	if(seek(fd, 0, 0) < 0 || readn(fd, hdr, sizeof hdr) != sizeof hdr)
		return nil;

	if((beload4(hdr)&~1) == 0xFEEDFACE){
		e2 = beload2;
		e4 = beload4;
		e8 = beload8;
	}else if((leload4(hdr)&~1) == 0xFEEDFACE){
		e2 = leload2;
		e4 = leload4;
		e8 = leload8;
	}else{
		werrstr("bad magic - not mach-o file");
		return nil;
	}
	is64 = e4(hdr) == 0xFEEDFACF;
	ncmd = e4(hdr+4*4);
	cmdsz = e4(hdr+5*4);
	if(ncmd > 0x10000 || cmdsz >= 0x01000000){
		werrstr("implausible mach-o header ncmd=%lud cmdsz=%lud", ncmd, cmdsz);
		return nil;
	}
	if(is64)
		readn(fd, tmp, 4);	// skip reserved word in header

	m = mallocz(sizeof(*m)+ncmd*sizeof(MachoCmd)+cmdsz, 1);
	if(m == nil)
		return nil;

	m->fd = fd;
	m->e2 = e2;
	m->e4 = e4;
	m->e8 = e8;
	m->cputype = e4(hdr+1*4);
	m->subcputype = e4(hdr+2*4);
	m->filetype = e4(hdr+3*4);
	m->ncmd = ncmd;
	m->flags = e4(hdr+6*4);
	m->is64 = is64;

	m->cmd = (MachoCmd*)(m+1);
	off = sizeof hdr;
	cmdp = (uchar*)(m->cmd+ncmd);
	if(readn(fd, cmdp, cmdsz) != cmdsz){
		werrstr("reading cmds: %r");
		free(m);
		return nil;
	}

	for(i=0; i<ncmd; i++){
		ty = e4(cmdp);
		sz = e4(cmdp+4);
		m->cmd[i].off = off;
		unpackcmd(cmdp, m, &m->cmd[i], ty, sz);
		cmdp += sz;
		off += sz;
	}
	return m;
}

void
machoclose(Macho *m)
{
	close(m->fd);
	free(m);
}
