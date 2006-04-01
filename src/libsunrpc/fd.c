#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>

enum
{
	MaxRead = 17000
};

typedef struct SunMsgFd SunMsgFd;
struct SunMsgFd
{
	SunMsg msg;
	int fd;
};

typedef struct Arg Arg;
struct Arg
{
	SunSrv *srv;
	Channel *creply;
	Channel *csync;
	int fd;
};

static void
sunfdread(void *v)
{
	uint n, tot;
	int done;
	uchar buf[4], *p;
	Arg arg = *(Arg*)v;
	SunMsgFd *msg;

	sendp(arg.csync, 0);

	p = nil;
	tot = 0;
	for(;;){
		n = readn(arg.fd, buf, 4);
		if(n != 4)
			break;
		n = (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
if(arg.srv->chatty) fprint(2, "%.8ux...", n);
		done = n&0x80000000;
		n &= ~0x80000000;
		p = erealloc(p, tot+n);
		if(readn(arg.fd, p+tot, n) != n)
			break;
		tot += n;
		if(done){
			msg = emalloc(sizeof(SunMsgFd));
			msg->msg.data = p;
			msg->msg.count = tot;
			msg->msg.creply = arg.creply;
			sendp(arg.srv->crequest, msg);
			p = nil;
			tot = 0;
		}
	}
}

static void
sunfdwrite(void *v)
{
	uchar buf[4];
	u32int n;
	Arg arg = *(Arg*)v;
	SunMsgFd *msg;

	sendp(arg.csync, 0);

	while((msg = recvp(arg.creply)) != nil){
		n = msg->msg.count;
		buf[0] = (n>>24)|0x80;
		buf[1] = n>>16;
		buf[2] = n>>8;
		buf[3] = n;
		if(write(arg.fd, buf, 4) != 4
		|| write(arg.fd, msg->msg.data, msg->msg.count) != msg->msg.count)
			fprint(2, "sunfdwrite: %r\n");
		free(msg->msg.data);
		free(msg);
	}
}

int
sunsrvfd(SunSrv *srv, int fd)
{
	Arg *arg;

	arg = emalloc(sizeof(Arg));
	arg->fd = fd;
	arg->srv = srv;
	arg->csync = chancreate(sizeof(void*), 0);
	arg->creply = chancreate(sizeof(SunMsg*), 10);

	proccreate(sunfdread, arg, SunStackSize);
	proccreate(sunfdwrite, arg, SunStackSize);
	recvp(arg->csync);
	recvp(arg->csync);

	chanfree(arg->csync);
	free(arg);
	return 0;
}
