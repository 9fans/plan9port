#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include "9proc.h"

extern void p9main(int, char**);

int
main(int argc, char **argv)
{
	_p9uproc(0);
	p9main(argc, argv);
	exits("main");
	return 99;
}
