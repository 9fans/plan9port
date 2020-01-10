#include <u.h>
/* #include <everything_but_the_kitchen_sink.h> */
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <libc.h>
#include <ip.h>

static void
sockaddr2ip(uchar *ip, struct sockaddr *sa)
{
	struct sockaddr_in *sin;

	sin = (struct sockaddr_in*)sa;
	memmove(ip, v4prefix, IPaddrlen);
	memmove(ip+IPv4off, &sin->sin_addr, 4);
}

Ipifc*
readipifc(char *net, Ipifc *ifc, int index)
{
	char *p, *ep, *q, *bp;
	int i, mib[6];
	size_t n;
	Ipifc *list, **last;
	Iplifc *lifc, **lastlifc;
	struct if_msghdr *mh, *nmh;
	struct ifa_msghdr *ah;
	struct sockaddr *sa;
	struct sockaddr_dl *sdl;
	uchar ip[IPaddrlen];

	USED(net);

	free(ifc);
	ifc = nil;
	list = nil;
	last = &list;

	/*
	 * Does not handle IPv6 yet.
	 */

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	n = 0;
	if(sysctl(mib, 6, nil, &n, nil, 0) < 0)
		return nil;
	bp = mallocz(n, 1);
	if(bp == nil)
		return nil;
	if(sysctl(mib, 6, bp, &n, nil, 0) < 0){
		free(bp);
		return nil;
	}

	p = bp;
	ep = p+n;
	while(p < ep){
		mh = (struct if_msghdr*)p;
		p += mh->ifm_msglen;
		if(mh->ifm_type != RTM_IFINFO)
			continue;
		ifc = mallocz(sizeof *ifc, 1);
		if(ifc == nil)
			break;
		*last = ifc;
		last = &ifc->next;
		sdl = (struct sockaddr_dl*)(mh+1);
		n = sdl->sdl_nlen;
		if(n >= sizeof ifc->dev)
			n = sizeof ifc->dev - 1;
		memmove(ifc->dev, sdl->sdl_data, n);
		ifc->dev[n] = 0;
		ifc->rp.linkmtu = mh->ifm_data.ifi_mtu;
		lastlifc = &ifc->lifc;

		if(sdl->sdl_type == IFT_ETHER && sdl->sdl_alen == 6)
			memmove(ifc->ether, LLADDR(sdl), 6);

		while(p < ep){
			ah = (struct ifa_msghdr*)p;
			nmh = (struct if_msghdr*)p;
			if(nmh->ifm_type != RTM_NEWADDR)
				break;
			p += nmh->ifm_msglen;
			lifc = nil;
			for(i=0, q=(char*)(ah+1); i<RTAX_MAX && q<p; i++){
				if(!(ah->ifam_addrs & (1<<i)))
					continue;
				sa = (struct sockaddr*)q;
				q += (sa->sa_len+sizeof(long)-1) & ~(sizeof(long)-1);
				if(sa->sa_family != AF_INET)
					continue;
				if(lifc == nil){
					lifc = mallocz(sizeof *lifc, 1);
					if(lifc == nil)
						continue;
					*lastlifc = lifc;
					lastlifc = &lifc->next;
				}
				sockaddr2ip(ip, sa);
				switch(i){
				case RTAX_IFA:
					ipmove(lifc->ip, ip);
					break;
				case RTAX_NETMASK:
					memset(ip, 0xFF, IPv4off);
					ipmove(lifc->mask, ip);
					break;
				case RTAX_BRD:
					if(mh->ifm_flags & IFF_POINTOPOINT)
						/* ipmove(lifc->remote, ip) */ ;
					if(mh->ifm_flags & IFF_BROADCAST)
						/* ipmove(lifc->bcast, ip) */ ;
					break;
				case RTAX_GATEWAY:
					break;
				case RTAX_DST:
					break;
				}
			}
			if(lifc)
				maskip(lifc->ip, lifc->mask, lifc->net);
		}
	}
	free(bp);
	return list;
}
