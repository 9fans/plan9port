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
unpackseg(uchar *p, Macho *m, MachoCmd *c, uint type, uint sz)
{
	u32int (*e4)(uchar*);

	e4 = m->e4;

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
		break;
	case MachoCmdSymtab:
		if(sz < 24)
			return -1;
		c->sym.symoff = e4(p+8);
		c->sym.nsyms = e4(p+12);
		c->sym.stroff = e4(p+16);
		c->sym.strsize = e4(p+20);
		break;
	}
	return 0;
}


Macho*
machoinit(int fd)
{
	int i;
	uchar hdr[7*4], *cmdp;
	u32int (*e4)(uchar*);
	ulong ncmd, cmdsz, ty, sz, off;
	Macho *m;

	if(seek(fd, 0, 0) < 0 || readn(fd, hdr, sizeof hdr) != sizeof hdr)
		return nil;

	if(beload4(hdr) == 0xFEEDFACE)
		e4 = beload4;
	else if(leload4(hdr) == 0xFEEDFACE)
		e4 = leload4;
	else{
		werrstr("bad magic - not mach-o file");
		return nil;
	}

	ncmd = e4(hdr+4*4);
	cmdsz = e4(hdr+5*4);
	if(ncmd > 0x10000 || cmdsz >= 0x01000000){
		werrstr("implausible mach-o header ncmd=%lud cmdsz=%lud", ncmd, cmdsz);
		return nil;
	}

	m = mallocz(sizeof(*m)+ncmd*sizeof(MachoCmd)+cmdsz, 1);
	if(m == nil)
		return nil;

	m->fd = fd;
	m->e4 = e4;
	m->cputype = e4(hdr+1*4);
	m->subcputype = e4(hdr+2*4);
	m->filetype = e4(hdr+3*4);
	m->ncmd = ncmd;
	m->flags = e4(hdr+6*4);

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
		unpackseg(cmdp, m, &m->cmd[i], ty, sz);
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
