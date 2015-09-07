#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <ime.h>

int
moveimespot(Point pt)
{
	return _displayimemovespot(display, pt);
}
