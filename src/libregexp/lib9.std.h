#include <fmt.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define	exits(x)	exit(x && *x ? 1 : 0)

#define	nil	0

