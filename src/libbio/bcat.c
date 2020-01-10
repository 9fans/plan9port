#include <fmt.h>
#include "bio.h"

Biobuf bout;

void
bcat(Biobuf *b, char *name)
{
	char buf[1000];
	int n;

	while((n = Bread(b, buf, sizeof buf)) > 0){
		if(Bwrite(&bout, buf, n) < 0)
			fprint(2, "writing during %s: %r\n", name);
	}
	if(n < 0)
		fprint(2, "reading %s: %r\n", name);
}

int
main(int argc, char **argv)
{
	int i;
	Biobuf b, *bp;
	Fmt fmt;

	Binit(&bout, 1, O_WRONLY);
	Bfmtinit(&fmt, &bout);
	fmtprint(&fmt, "hello, world\n");
	Bfmtflush(&fmt);

	if(argc == 1){
		Binit(&b, 0, O_RDONLY);
		bcat(&b, "<stdin>");
	}else{
		for(i=1; i<argc; i++){
			if((bp = Bopen(argv[i], O_RDONLY)) == 0){
				fprint(2, "Bopen %s: %r\n", argv[i]);
				continue;
			}
			bcat(bp, argv[i]);
			Bterm(bp);
		}
	}
	exit(0);
}
