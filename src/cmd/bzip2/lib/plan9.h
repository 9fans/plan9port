#include <u.h>
#include <libc.h>
#include <ctype.h>

#define exit(x) exits((x) ? "whoops" : nil)
#define size_t ulong
