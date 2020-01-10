#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include <ip.h>

#include <sys/socket.h>
#include <netinet/in.h>

long
udpread(int fd, Udphdr *hdr, void *buf, long n)
{
	struct sockaddr_in sin;
	socklen_t len;

	len = sizeof sin;
	if(getsockname(fd, (struct sockaddr*)&sin, &len) < 0)
		return -1;
	if(len != sizeof sin){
		werrstr("getsockname acting weird");
		return -1;
	}
	memset(hdr, 0, sizeof *hdr);
	memmove(hdr->laddr, v4prefix, IPaddrlen);
	memmove(hdr->laddr+12, &sin.sin_addr, sizeof(u32int));
	memmove(hdr->lport, &sin.sin_port, sizeof(u16int));

	len = sizeof sin;
	n = recvfrom(fd, buf, n, 0, (struct sockaddr*)&sin, &len);
	if(n < 0)
		return -1;
	if(len != sizeof sin){
		werrstr("recvfrom acting weird");
		return -1;
	}
	memmove(hdr->raddr, v4prefix, IPaddrlen);
	memmove(hdr->raddr+12, &sin.sin_addr, sizeof(u32int));
	memmove(hdr->rport, &sin.sin_port, sizeof(u16int));

	return n;
}

long
udpwrite(int fd, Udphdr *hdr, void *buf, long n)
{
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	memmove(&sin.sin_addr, hdr->raddr+12, 4);
	memmove(&sin.sin_port, hdr->rport, 2);
	return sendto(fd, buf, n, 0, (struct sockaddr*)&sin, sizeof sin);
}
