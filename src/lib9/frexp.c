#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

double
p9frexp(double d, int *i)
{
	return frexp(d, i);
}
