#include <u.h>
#include <libc.h>

/*
 * On gcc and clang, getcallerpc is a macro invoking a compiler builtin.
 * If the macro in libc.h did not trigger, there's no implementation.
 */
#undef getcallerpc
ulong
getcallerpc(void *v)
{
	return 1;
}
