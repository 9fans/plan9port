#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>

int _wantfocuschanges;
static int
bad(void)
{
	sysfatal("compiled with no window system support");
	return 0;
}

void
moveto(Mousectl *m, Point pt)
{
	USED(m);
/*	USED(pt); */
	bad();
}

void
closemouse(Mousectl *mc)
{
	USED(mc);
	bad();
}

int
readmouse(Mousectl *mc)
{
	USED(mc);
	return bad();
}

Mousectl*
initmouse(char *file, Image *i)
{
	USED(file);
	USED(i);
	bad();
	return nil;
}

void
setcursor(Mousectl *mc, Cursor *c)
{
	USED(mc);
	USED(c);
	bad();
}

void
bouncemouse(Mouse *m)
{
	USED(m);
	bad();
}

