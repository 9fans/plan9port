#include <u.h>
#include <libc.h>
#include <ip.h>


typedef struct Icmp Icmp;
struct Icmp
{
	uchar	vihl;		/* Version and header length */
	uchar	tos;		/* Type of service */
	uchar	length[2];	/* packet length */
	uchar	id[2];		/* Identification */
	uchar	frag[2];	/* Fragment information */
	uchar	ttl;		/* Time to live */
	uchar	proto;		/* Protocol */
	uchar	ipcksum[2];	/* Header checksum */
	uchar	src[4];		/* Ip source */
	uchar	dst[4];		/* Ip destination */
	uchar	type;
	uchar	code;
	uchar	cksum[2];
	uchar	icmpid[2];
	uchar	seq[2];
	uchar	data[1];
};

enum
{			/* Packet Types */
	EchoReply	= 0,
	Unreachable	= 3,
	SrcQuench	= 4,
	EchoRequest	= 8,
	TimeExceed	= 11,
	Timestamp	= 13,
	TimestampReply	= 14,
	InfoRequest	= 15,
	InfoReply	= 16,

	ICMP_IPSIZE	= 20,
	ICMP_HDRSIZE	= 8
};

static void
catch(void *a, char *msg)
{
	USED(a);
	if(strstr(msg, "alarm"))
		noted(NCONT);
	else
		noted(NDFLT);
}

#define MSG "dhcp probe"

/*
 *  make sure noone is using the address
 */
int
icmpecho(uchar *a)
{
	int fd;
	char buf[512];
	Icmp *ip;
	int i, n, len;
	ushort sseq, x;
	int rv;

return 0;
	rv = 0;

	sprint(buf, "%I", a);
	fd = dial(netmkaddr(buf, "icmp", "1"), 0, 0, 0);
	if(fd < 0){
		return 0;
	}

	sseq = getpid()*time(0);

	ip = (Icmp*)buf;
	notify(catch);
	for(i = 0; i < 3; i++){
		ip->type = EchoRequest;
		ip->code = 0;
		strcpy((char*)ip->data, MSG);
		ip->seq[0] = sseq;
		ip->seq[1] = sseq>>8;
		len = ICMP_IPSIZE+ICMP_HDRSIZE+sizeof(MSG);

		/* send a request */
		if(write(fd, buf, len) < len)
			break;

		/* wait 1/10th second for a reply and try again */
		alarm(100);
		n = read(fd, buf, sizeof(buf));
		alarm(0);
		if(n <= 0)
			continue;

		/* an answer to our echo request? */
		x = (ip->seq[1]<<8)|ip->seq[0];
		if(n >= len)
		if(ip->type == EchoReply)
		if(x == sseq)
		if(strcmp((char*)ip->data, MSG) == 0){
			rv = 1;
			break;
		}
	}
	close(fd);
	return rv;
}
