#include <u.h>
#include <libc.h>
#include "fmt/nan.h"

double
NaN(void)
{
	return __NaN();
}

double
Inf(int sign)
{
	return __Inf(sign);
}

int
isNaN(double x)
{
	return __isNaN(x);
}

int
isInf(double x, int sign)
{
	return __isInf(x, sign);
}
