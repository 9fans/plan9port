#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

extern void p9main(int, char**);

int
main(int argc, char **argv)
{
	p9main(argc, argv);
	exits("main");
	return 99;
}
