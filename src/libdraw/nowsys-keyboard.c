#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <keyboard.h>

static int
bad(void)
{
	sysfatal("compiled with no window system support");
	return 0;
}

void
closekeyboard(Keyboardctl *mc)
{
	USED(mc);
	bad();
}

Keyboardctl*
initkeyboard(char *file)
{
	USED(file);
	bad();
	return nil;
}
