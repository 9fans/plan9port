#include <u.h>
#include <libc.h>
#include <ip.h>

int
myipaddr(uchar *ip, char *net)
{
	Ipifc *nifc;
	Iplifc *lifc;
	static Ipifc *ifc;

	ifc = readipifc(net, ifc, -1);
	for(nifc = ifc; nifc; nifc = nifc->next)
		for(lifc = nifc->lifc; lifc; lifc = lifc->next)
			if(ipcmp(lifc->ip, IPnoaddr) != 0){
				ipmove(ip, lifc->ip);
				return 0;
			}
	ipmove(ip, IPnoaddr);
	return -1;
}
