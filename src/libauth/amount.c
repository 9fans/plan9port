#include <u.h>
#include <libc.h>
#include <auth.h>
#include "authlocal.h"

int
amount(int fd, char *mntpt, int flags, char *aname)
{
	int rv, afd;
	AuthInfo *ai;

	afd = fauth(fd, aname);
	if(afd >= 0){
		ai = auth_proxy(afd, amount_getkey, "proto=p9any role=client");
		if(ai != nil)
			auth_freeAI(ai);
	}
	rv = mount(fd, afd, mntpt, flags, aname);
	if(afd >= 0)
		close(afd);
	return rv;
}
