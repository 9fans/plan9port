#include <u.h>
#include <libc.h>
#include <draw.h>

int
drawreplxy(int min, int max, int x)
{
	int sx;

	sx = (x-min)%(max-min);
	if(sx < 0)
		sx += max-min;
	return sx+min;
}

Point
drawrepl(Rectangle r, Point p)
{
	p.x = drawreplxy(r.min.x, r.max.x, p.x);
	p.y = drawreplxy(r.min.y, r.max.y, p.y);
	return p;
}
