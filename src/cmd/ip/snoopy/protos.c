#include <u.h>
#include <libc.h>
#include "dat.h"
#include "protos.h"
Proto *protos[] =
{
	&ether,
	&ip,
	&ip6,
	&dump,
	&arp,
	&rarp,
	&udp,
	&bootp,
	&dhcp,
	&hdlc,
	&rtp,
	&rtcp,
	&tcp,
	&il,
	&icmp,
	&icmp6,
	&ninep,
	&ospf,
	&ppp,
	&ppp_ccp,
	&ppp_lcp,
	&ppp_chap,
	&ppp_ipcp,
	&pppoe_sess,
	&pppoe_disc,
	&dns,
	&p80211,
	&llc,
	&radiotap,
	&snap,
	0
};
