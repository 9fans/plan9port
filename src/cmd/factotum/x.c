#include <u.h>
#include <libc.h>
#include <auth.h>

void
f(void*)
{
}

void
main(void)
{
	f(auth_challenge);
	f(auth_response);
}
