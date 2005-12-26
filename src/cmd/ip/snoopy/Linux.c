#include <u.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <fcall.h>
#include <libsec.h>
#include "dat.h"
#include "protos.h"
#include "y.tab.h"

int
opendevice(char *dev, int promisc)
{
	int fd;
	struct ifreq ifr;
	struct sockaddr_ll sa;

	if((fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
		return -1;

	if(dev){
		memset(&ifr, 0, sizeof ifr);
		strncpy(ifr.ifr_name, dev, sizeof ifr.ifr_name);
		if(ioctl(fd, SIOCGIFINDEX, &ifr) < 0){
			close(fd);
			return -1;
		}
		memset(&sa, 0, sizeof sa);
		sa.sll_family = AF_PACKET;
		sa.sll_protocol = htons(ETH_P_ALL);
		sa.sll_ifindex = ifr.ifr_ifindex;
		if(bind(fd, (struct sockaddr*)&sa, sizeof sa) < 0){
			close(fd);
			return -1;
		}
	}

	if(promisc){
		memset(&ifr, 0, sizeof ifr);
		strncpy(ifr.ifr_name, dev, sizeof ifr.ifr_name);
		if(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0){
			close(fd);
			return -1;
		}
		ifr.ifr_flags |= IFF_PROMISC;
		if(ioctl(fd, SIOCSIFFLAGS, &ifr) < 0){
			close(fd);
			return -1;
		}
	}
	return fd;
}
