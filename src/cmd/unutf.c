/*
 * stupid little program to pipe unicode chars through
 * when converting to non-utf compilers.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

Biobuf bin;

void
main(void)
{
	int c;

	Binit(&bin, 0, OREAD);
	while((c = Bgetrune(&bin)) >= 0)
		print("0x%ux\n", c);
	exits(0);
}
