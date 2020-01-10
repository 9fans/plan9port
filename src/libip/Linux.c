#include <u.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <asm/types.h>
#include <net/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <libc.h>
#include <ip.h>

/*
 * Use netlink sockets to find interfaces.
 * Thanks to Erik Quanstrom.
 */
static int
netlinkrequest(int fd, int type, int (*fn)(struct nlmsghdr *h, Ipifc**, int),
	Ipifc **ifc, int index)
{
	char buf[1024];
	int n;
	struct sockaddr_nl nl;
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;
	struct nlmsghdr *h;

	memset(&nl, 0, sizeof nl);
	nl.nl_family = AF_NETLINK;

	memset(&req, 0, sizeof req);
	req.nlh.nlmsg_len = sizeof req;
	req.nlh.nlmsg_type = type;
	req.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
	req.nlh.nlmsg_pid = 0;
	req.nlh.nlmsg_seq = 1;
	req.g.rtgen_family = AF_NETLINK;

	if(sendto(fd, (void*)&req, sizeof req, 0, (struct sockaddr*)&nl, sizeof nl) < 0)
		return -1;

	while((n=read(fd, buf, sizeof buf)) > 0){
		for(h=(struct nlmsghdr*)buf; NLMSG_OK(h, n); h = NLMSG_NEXT(h, n)){
			if(h->nlmsg_type == NLMSG_DONE)
				return 0;
			if(h->nlmsg_type == NLMSG_ERROR){
				werrstr("netlink error");
				return -1;
			}
			if(fn(h, ifc, index) < 0)
				return -1;
		}
	}
	werrstr("netlink error");
	return -1;
}

static int
devsocket(void)
{
	/* we couldn't care less which one; just want to talk to the kernel! */
	static int dumb[3] = { AF_INET, AF_PACKET, AF_INET6 };
	int i, fd;

	for(i=0; i<nelem(dumb); i++)
		if((fd = socket(dumb[i], SOCK_DGRAM, 0)) >= 0)
			return fd;
	return -1;
}

static int
parsertattr(struct rtattr **dst, int ndst, struct nlmsghdr *h, int type, int skip)
{
	struct rtattr *src;
	int len;

	len = h->nlmsg_len - NLMSG_LENGTH(skip);
	if(len < 0 || h->nlmsg_type != type){
		werrstr("attrs too short");
		return -1;
	}
	src = (struct rtattr*)((char*)NLMSG_DATA(h) + NLMSG_ALIGN(skip));

	memset(dst, 0, ndst*sizeof dst[0]);
	for(; RTA_OK(src, len); src = RTA_NEXT(src, len))
		if(src->rta_type < ndst)
			dst[src->rta_type] = src;
	return 0;
}

static void
rta2ip(int af, uchar *ip, struct rtattr *rta)
{
	memset(ip, 0, IPaddrlen);

	switch(af){
	case AF_INET:
		memmove(ip, v4prefix, IPv4off);
		memmove(ip+IPv4off, RTA_DATA(rta), IPv4addrlen);
		break;
	case AF_INET6:
		memmove(ip, RTA_DATA(rta), IPaddrlen);
		break;
	}
}

static int
getlink(struct nlmsghdr *h, Ipifc **ipifclist, int index)
{
	char *p;
	int fd;
	struct rtattr *attr[IFLA_MAX+1];
	struct ifinfomsg *ifi;
	Ipifc *ifc;

	ifi = (struct ifinfomsg*)NLMSG_DATA(h);
	if(index >= 0 && ifi->ifi_index != index)
		return 0;

	ifc = mallocz(sizeof *ifc, 1);
	if(ifc == nil)
		return -1;
	ifc->index = ifi->ifi_index;

	while(*ipifclist)
		ipifclist = &(*ipifclist)->next;
	*ipifclist = ifc;

	if(parsertattr(attr, nelem(attr), h, RTM_NEWLINK, sizeof(struct ifinfomsg)) < 0)
		return -1;

	if(attr[IFLA_IFNAME])
		p = (char*)RTA_DATA(attr[IFLA_IFNAME]);
	else
		p = "nil";
	strecpy(ifc->dev, ifc->dev+sizeof ifc->dev, p);

	if(attr[IFLA_MTU])
		ifc->mtu = *(int*)RTA_DATA(attr[IFLA_MTU]);

	/*
	 * Does not work on old Linux systems,
	 * and not really necessary for my purposes.
	 * Uncomment if you want it bad.
	 *
	if(attr[IFLA_STATS]){
		struct rtnl_link_stats *s;

		s = RTA_DATA(attr[IFLA_STATS]);
		ifc->pktin = s->rx_packets;
		ifc->pktout = s->tx_packets;
		ifc->errin = s->rx_errors;
		ifc->errout = s->tx_errors;
	}
	 *
	 */

	if((fd = devsocket()) > 0){
		struct ifreq ifr;

		memset(&ifr, 0, sizeof ifr);
		strncpy(ifr.ifr_name, p, IFNAMSIZ);
		ifr.ifr_mtu = 0;
		if(ioctl(fd, SIOCGIFMTU, &ifr) >= 0)
			ifc->rp.linkmtu = ifr.ifr_mtu;

		memset(&ifr, 0, sizeof ifr);
		strncpy(ifr.ifr_name, p, IFNAMSIZ);
		if(ioctl(fd, SIOCGIFHWADDR, &ifr) >= 0
		&& ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER)
			memmove(ifc->ether, ifr.ifr_hwaddr.sa_data, 6);

		close(fd);
	}
	return 0;
}

static int
getaddr(struct nlmsghdr *h, Ipifc **ipifclist, int index)
{
	int mask;
	Ipifc *ifc;
	Iplifc *lifc, **l;
	struct ifaddrmsg *ifa;
	struct rtattr *attr[IFA_MAX+1];

	USED(index);

	ifa = (struct ifaddrmsg*)NLMSG_DATA(h);
	for(ifc=*ipifclist; ifc; ifc=ifc->next)
		if(ifc->index == ifa->ifa_index)
			break;
	if(ifc == nil)
		return 0;
	if(parsertattr(attr, nelem(attr), h, RTM_NEWADDR, sizeof(struct ifaddrmsg)) < 0)
		return -1;

	lifc = mallocz(sizeof *lifc, 1);
	if(lifc == nil)
		return -1;
	for(l=&ifc->lifc; *l; l=&(*l)->next)
		;
	*l = lifc;

	if(attr[IFA_ADDRESS] == nil)
		attr[IFA_ADDRESS] = attr[IFA_LOCAL];
	if(attr[IFA_ADDRESS] == nil)
		return 0;

	rta2ip(ifa->ifa_family, lifc->ip, attr[IFA_ADDRESS]);

	mask = ifa->ifa_prefixlen/8;
	if(ifa->ifa_family == AF_INET)
		mask += IPv4off;
	memset(lifc->mask, 0xFF, mask);
	memmove(lifc->net, lifc->ip, mask);

	if(attr[IFA_CACHEINFO]){
		struct ifa_cacheinfo *ci;

		ci = RTA_DATA(attr[IFA_CACHEINFO]);
		lifc->preflt = ci->ifa_prefered;
		lifc->validlt = ci->ifa_valid;
	}
	return 0;
}

Ipifc*
readipifc(char *net, Ipifc *ifc, int index)
{
	int fd;

	USED(net);
	freeipifc(ifc);
	ifc = nil;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if(fd < 0)
		return nil;
	ifc = nil;
	if(netlinkrequest(fd, RTM_GETLINK, getlink, &ifc, index) < 0
	|| netlinkrequest(fd, RTM_GETADDR, getaddr, &ifc, index) < 0){
		close(fd);
		freeipifc(ifc);
		return nil;
	}
	close(fd);
	return ifc;
}
