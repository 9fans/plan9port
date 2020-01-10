#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

void
memfillcolor(Memimage *m, u32int val)
{
	_memfillcolor(m, val);
}
