#include <u.h>
#include <libc.h>

static void
nop(void)
{
}

void (*_pin)(void) = nop;
void (*_unpin)(void) = nop;
