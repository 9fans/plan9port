#include <u.h>
#include <libc.h>
#include <auth.h>

int
login(char *user, char *password, char *namespace)
{
	int rv;
	AuthInfo *ai;

	if((ai = auth_userpasswd(user, password)) == nil)
		return -1;

	rv = auth_chuid(ai, namespace);
	auth_freeAI(ai);
	return rv;
}
