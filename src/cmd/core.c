#include <u.h>
#include <libc.h>
#include <mach.h>

char *coredir(void);
void coreall(char*);
void corefile(char*, int);

void
usage(void)
{
	fprint(2, "usage: core [dir | corefile]...\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc == 0){
		argc++;
		*--argv = coredir();
	}

	for(i=0; i<argc; i++)
		coreall(argv[i]);
}

char*
coredir(void)
{
	char *dir;

	dir = getenv("COREDIR");
	if(dir == nil)
		dir = ".";
	return dir;
}

void
coreall(char *name)
{
	Dir *d;
	int fd, i, n;
	char *p;

	if((d = dirstat(name)) == nil){
		fprint(2, "%s: %r\n", name);
		return;
	}
	if((d->mode&DMDIR) == 0)
		corefile(name, 1);
	free(d);
	if((fd = open(name, OREAD)) < 0){
		fprint(2, "open %s: %r\n", name);
		return;
	}
	n = dirreadall(fd, &d);
	for(i=0; i<n; i++){
		p = smprint("%s/%s", name, d[i].name);
		if(p == nil)
			sysfatal("out of memory");
		corefile(p, 0);
		free(p);
	}
}

void
corefile(char *name, int explicit)
{
	Fhdr *hdr;
	char t[100];
	Dir *d;

	if((d = dirstat(name)) == nil){
		if(explicit)
			fprint(2, "%s; %r\n", name);
		return;
	}
	strcpy(t, ctime(d->mtime));
	t[strlen(t)-1] = 0;	/* newline */

	if((hdr = crackhdr(name, OREAD)) == nil){
		if(explicit)
			fprint(2, "%s: %r\n", name);
		return;
	}
	if(hdr->ftype != FCORE){
		uncrackhdr(hdr);
		if(explicit)
			fprint(2, "%s: not a core file\n", name);
		return;
	}
	print("acid %s # %s %s\n", name, t, hdr->cmd);
	uncrackhdr(hdr);
}
