#include <u.h>
#include <libc.h>
#include <ip.h>

Ipifc*
readipifc(char *net, Ipifc *ipifc, int index)
{
	USED(net);
	USED(ipifc);
	USED(index);
	
	werrstr("not implemented");
	return nil;
}

int
myetheraddr(uchar *to, char *dev)
{
	USED(to);
	USED(dev);

	werrstr("not implemented");
	return -1;
}
