#include "a.h"

/*
 * 18. Insertions from the standard input
 */
void
r_rd(int argc, Rune **argv)
{
	char *s;
	Rune *p;
	Fmt fmt;
	static int didstdin;
	static Biobuf bstdin;

	/*
	 * print prompt, then read until double newline,
	 * then run the text just read as though it were
	 * a macro body, using the remaining arguments.
	 */
	if(isatty(0)){
		if(argc > 1)
			fprint(2, "%S", argv[1]);
		else
			fprint(2, "%c", 7/*BEL*/);
	}

	if(!didstdin){
		Binit(&bstdin, 0, OREAD);
		didstdin = 1;
	}
	runefmtstrinit(&fmt);
	while((s = Brdstr(&bstdin, '\n', 0)) != nil){
		if(s[0] == '\n'){
			free(s);
			break;
		}
		fmtprint(&fmt, "%s", s);
		free(s);
	}
	p = runefmtstrflush(&fmt);
	if(p == nil)
		warn("out of memory in %Crd", dot);
	ds(L(".rd"), p);
	argc--;
	argv++;
	argv[0] = L(".rd");
	runmacro('.', argc, argv);
	ds(L(".rd"), nil);
}

/* terminate exactly as if input had ended */
void
r_ex(int argc, Rune **argv)
{
	USED(argc);
	USED(argv);

	while(popinput())
		;
}

void
t18init(void)
{
	addreq(L("rd"), r_rd, -1);
	addreq(L("ex"), r_ex, 0);
}
