#include <u.h>
#include <libc.h>

void
main(int argc, char *argv[])
{
	char *pr;
	int n, dflag;

	dflag = 0;
	if(argc>1 && strcmp(argv[1], "-d") == 0){
		--argc;
		++argv;
		dflag = 1;
	}
	if(argc < 2 || argc > 3){
		fprint(2, "usage: basename [-d] string [suffix]\n");
		exits("usage");
	}
	pr = utfrrune(argv[1], '/');
	if(dflag){
		if(pr){
			*pr = 0;
			print("%s\n", argv[1]);
			exits(0);
		}
		print(".\n");
		exits(0);
	}
	if(pr)
		pr++;
	else
		pr = argv[1];
	if(argc==3){
		n = strlen(pr)-strlen(argv[2]);
		if(n >= 0 && !strcmp(pr+n, argv[2]))
			pr[n] = 0;
	}
	print("%s\n", pr);
	exits(0);
}
