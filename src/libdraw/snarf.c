#include <u.h>
#include <libc.h>
#include <draw.h>

void
putsnarf(char *snarf)
{
	_displaywrsnarf(display, snarf);
}

char*
getsnarf(void)
{
	return _displayrdsnarf(display);
}
