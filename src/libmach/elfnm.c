#include <u.h>
#include <libc.h>
#include <mach.h>
#include <elf.h>

void
usage(void)
{
	fprint(2, "usage: elfnm file...\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	ElfSym esym;
	Fhdr *fp;
	int i, j;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc == 0)
		usage();

	for(i=0; i<argc; i++){
		if((fp = crackhdr(argv[i], OREAD)) == nil){
			fprint(2, "%s: %r\n", argv[i]);
			continue;
		}
		for(j=0; elfsym(fp->elf, j, &esym)>=0; j++)
			print("%s 0x%lux\n", esym.name, esym.value);
		uncrackhdr(fp);
	}
	exits(0);
}
