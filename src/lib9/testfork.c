#include <lib9.h>

void
sayhi(void *v)
{
	USED(v);

	print("hello from subproc\n");
	print("rendez got %lu from main\n", rendezvous(0x1234, 1234));
	exits(0);
}

int
main(int argc, char **argv)
{
	print("hello from main\n");
	ffork(RFMEM|RFPROC, sayhi, nil);

	print("rendez got %lu from subproc\n", rendezvous(0x1234, 0));
	exits(0);
}
