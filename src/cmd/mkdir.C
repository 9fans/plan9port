#include <u.h>
#include <libc.h>

void
main(int argc, char *argv[])
{
	int i, f;
	char *e;

	e = nil;
	for(i=1; i<argc; i++){
		if(access(argv[i], 0) == AEXIST){
			fprint(2, "mkdir: %s already exists\n", argv[i]);
			e = "error";
			continue;
		}
		f = create(argv[i], OREAD, DMDIR | 0777L);
		if(f < 0){
			fprint(2, "mkdir: can't create %s: %r\n", argv[i]);
			e = "error";
			continue;
		}
		close(f);
	}
	exits(e);
}
