#include <u.h>
#include <libc.h>
#include <ip.h>

void
main(void)
{
	Ipifc *ifc, *list;
	Iplifc *lifc;

	fmtinstall('I', eipfmt);
	fmtinstall('M', eipfmt);
	fmtinstall('E', eipfmt);

	list = readipifc("/net", nil, -1);
	for(ifc = list; ifc; ifc = ifc->next){
		print("ipifc %s %d %E\n", ifc->dev, ifc->mtu, ifc->ether);
		for(lifc = ifc->lifc; lifc; lifc = lifc->next)
			print("\t%I %M %I\n", lifc->ip, lifc->mask, lifc->net);
	}
}
