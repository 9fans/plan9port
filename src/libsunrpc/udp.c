#include <u.h>
#include <libc.h>
#include <ip.h>
#include <thread.h>
#include <sunrpc.h>

typedef struct SunMsgUdp SunMsgUdp;
struct SunMsgUdp
{
	SunMsg msg;
	Udphdr udp;
};

typedef struct Arg Arg;
struct Arg
{
	SunSrv *srv;
	Channel *creply;
	Channel *csync;
	int fd;
};

enum
{
	UdpMaxRead = 65536+Udphdrsize
};
static void
sunudpread(void *v)
{
	int n, paraport, port;
	uchar *buf;
	Arg arg = *(Arg*)v;
	SunMsgUdp *msg;
	SunSrv *srv;
	Udphdr udp;
	uchar localip[IPaddrlen];

	sendp(arg.csync, 0);
	srv = arg.srv;
	paraport = -1;

	/* 127.1 */
	memmove(localip, v4prefix, IPaddrlen);
	localip[12] = 127;
	localip[15] = 1;

	buf = emalloc(UdpMaxRead);
	while((n = udpread(arg.fd, &udp, buf, UdpMaxRead)) > 0){
		if(arg.srv->chatty)
			fprint(2, "udpread got %d (%d) from %I\n", n, Udphdrsize, udp.raddr);
		msg = emalloc(sizeof(SunMsgUdp));
		msg->udp = udp;
		msg->msg.data = emalloc(n);
		msg->msg.count = n;
		memmove(msg->msg.data, buf, n);
		msg->msg.creply = arg.creply;
		msg->msg.srv = arg.srv;
		if(arg.srv->chatty)
			fprint(2, "message %p count %d\n", msg, msg->msg.count);
		if((srv->localonly || srv->localparanoia) && ipcmp(udp.raddr, localip) != 0){
			fprint(2, "dropping message from %I: not local\n", udp.raddr);
			sunmsgreplyerror(&msg->msg, SunAuthTooWeak);
			continue;
		}
		if(srv->localparanoia){
			port = nhgets(udp.rport);
			if(paraport == -1){
				fprint(2, "paranoid mode: only %I/%d allowed\n", localip, port);
				paraport = port;
			}else if(paraport != port){
				fprint(2, "dropping message from %I: not port %d\n", udp.raddr, port);
				sunmsgreplyerror(&msg->msg, SunAuthTooWeak);
				continue;
			}
		}
		if(srv->ipokay && !srv->ipokay(udp.raddr, nhgets(udp.rport)))
			msg->msg.rpc.status = SunProgUnavail;
		sendp(arg.srv->crequest, msg);
	}
}

static void
sunudpwrite(void *v)
{
	Arg arg = *(Arg*)v;
	SunMsgUdp *msg;

	sendp(arg.csync, 0);

	while((msg = recvp(arg.creply)) != nil){
		if(udpwrite(arg.fd, &msg->udp, msg->msg.data, msg->msg.count) != msg->msg.count)
			fprint(2, "udpwrite: %r\n");
		sunmsgdrop(&msg->msg);
	}
}

int
sunsrvudp(SunSrv *srv, char *address)
{
	int fd;
	char adir[40];
	Arg *arg;

	fd = announce(address, adir);
	if(fd < 0)
		return -1;

	arg = emalloc(sizeof(Arg));
	arg->fd = fd;
	arg->srv = srv;
	arg->creply = chancreate(sizeof(SunMsg*), 10);
	arg->csync = chancreate(sizeof(void*), 10);

	proccreate(sunudpread, arg, SunStackSize);
	proccreate(sunudpwrite, arg, SunStackSize);
	recvp(arg->csync);
	recvp(arg->csync);
	chanfree(arg->csync);
	free(arg);

	return 0;
}
