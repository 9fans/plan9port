#include <u.h>
#include <libc.h>
#include <mach.h>
#include "stabs.h"
#include "macho.h"

void
usage(void)
{
	fprint(2, "usage: machodump file list\n");
	fprint(2, "	machodump file stabs\n");
	exits("usage");
}

uchar*
load(int fd, ulong off, int size)
{
	uchar *a;

	a = malloc(size);
print("malloc %d -> %p\n", size, a);
	if(a == nil)
		sysfatal("malloc: %r");
	if(seek(fd, off, 0) < 0)
		sysfatal("seek %lud: %r", off);
	if(readn(fd, a, size) != size)
		sysfatal("readn: %r");
	return a;
}

void
main(int argc, char **argv)
{
	int i;
	Macho *m;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc < 2)
		usage();

	if((m = machoopen(argv[0])) == nil)
		sysfatal("machoopen %s: %r", argv[0]);

	if(strcmp(argv[1], "stabs") == 0){
		Stab stabs;
		StabSym sym;

		for(i=0; i<m->ncmd; i++){
			if(m->cmd[i].type == MachoCmdSymtab){
				stabs.stabbase = load(m->fd, m->cmd[i].sym.symoff, m->cmd[i].sym.nsyms*16);
				stabs.stabsize = m->cmd[i].sym.nsyms*16;
				stabs.strbase = load(m->fd, m->cmd[i].sym.stroff, m->cmd[i].sym.strsize);
				stabs.strsize = m->cmd[i].sym.strsize;
				stabs.e4 = m->e4;
				stabs.e2 = (m->e4 == beload4 ? beload2 : leload2);
				print("cmd%d: %p %ud %p %ud\n", i, stabs.stabbase, stabs.stabsize, stabs.strbase, stabs.strsize);
				for(i=0; stabsym(&stabs, i, &sym) >= 0; i++)
					print("%s type 0x%x other %d desc %d value 0x%lux\n",
						sym.name, sym.type, sym.other, (int)sym.desc, (ulong)sym.value);
				print("err at %d: %r\n", i);
			}
		}
	}
	else if(strcmp(argv[1], "list") == 0){
		print("macho cpu %ud sub %ud filetype %lud flags %lud\n",
			m->cputype, m->subcputype, m->filetype, m->flags);
		for(i=0; i<m->ncmd; i++){
			switch(m->cmd[i].type){
			case MachoCmdSymtab:
				print("cmd%d: symtab %lud+%lud %lud+%lud\n", i,
					m->cmd[i].sym.symoff, m->cmd[i].sym.nsyms,
					m->cmd[i].sym.stroff, m->cmd[i].sym.strsize);
				break;
			case MachoCmdSegment:
				print("cmd%d: segment %s vm 0x%lux+0x%lux file 0x%lux+0x%lux prot 0x%lux/0x%lux ns %d flags 0x%lux\n", i,
					m->cmd[i].seg.name, m->cmd[i].seg.vmaddr, m->cmd[i].seg.vmsize,
					m->cmd[i].seg.fileoff, m->cmd[i].seg.filesz, m->cmd[i].seg.maxprot,
					m->cmd[i].seg.initprot, m->cmd[i].seg.nsect, m->cmd[i].seg.flags);
				break;
			default:
				print("cmd%d: type %d offset %lud\n", i, m->cmd[i].type, m->cmd[i].off);
				break;
			}
		}
	}
	else
		usage();
	exits(0);
}
