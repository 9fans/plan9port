#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>

static int
bad(void)
{
	sysfatal("compiled with no window system support");
	return 0;
}

ulong
event(Event *e)
{
	USED(e);
	return bad();
}

ulong
eread(ulong keys, Event *e)
{
	USED(keys);
	USED(e);
	return bad();
}

void
einit(ulong keys)
{
	USED(keys);
	bad();
}

int
ekbd(void)
{
	return bad();
}

Mouse
emouse(void)
{
	Mouse m;
	
	bad();
	return m;
}

int
ecanread(ulong keys)
{
	USED(keys);
	return bad();
}

int
ecanmouse(void)
{
	return bad();
}

int
ecankbd(void)
{
	return bad();
}

void
emoveto(Point p)
{
	USED(p);
	bad();
}

void
esetcursor(Cursor *c)
{
	USED(c);
	bad();
}

