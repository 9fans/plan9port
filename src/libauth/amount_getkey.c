#include <u.h>
#include <libc.h>
#include <auth.h>

int (*amount_getkey)(char*) = auth_getkey;
