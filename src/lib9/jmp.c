#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

void
p9longjmp(p9jmp_buf buf, int val)
{
	siglongjmp((void*)buf, val);
}

void
p9notejmp(void *x, p9jmp_buf buf, int val)
{
	USED(x);
	siglongjmp((void*)buf, val);
}

