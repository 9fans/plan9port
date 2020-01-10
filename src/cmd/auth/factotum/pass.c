/*
 * This is just a repository for a password.
 * We don't want to encourage this, there's
 * no server side.
 *
 * Client:
 *	start proto=pass ...
 *	read password
 */

#include "std.h"
#include "dat.h"

static int
passproto(Conv *c)
{
	Key *k;

	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		return -1;
	c->state = "write";
	convprint(c, "%q %q",
		strfindattr(k->attr, "user"),
		strfindattr(k->privattr, "!password"));
	return 0;
}

static Role passroles[] = {
	"client",	passproto,
	0
};

Proto pass =
{
	"pass",
	passroles,
	"user? !password?",
	nil,
	nil
};
