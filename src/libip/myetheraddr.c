#include <u.h>
#include <libc.h>
#include <ip.h>

static char zea[6];

int
myetheraddr(uchar *to, char *dev)
{
	Ipifc *ifclist, *ifc;

	ifclist = readipifc(nil, nil, -1);
	for(ifc=ifclist; ifc; ifc=ifc->next){
		if(dev && strcmp(ifc->dev, dev) != 0)
			continue;
		if(memcmp(zea, ifc->ether, 6) == 0)
			continue;
		memmove(to, ifc->ether, 6);
		freeipifc(ifclist);
		return 0;
	}
	freeipifc(ifclist);
	werrstr("no ethernet devices");
	return -1;
}
