#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

typedef struct Sendfd Sendfd;
struct Sendfd {
	struct cmsghdr cmsg;
	int fd;
};

int
sendfd(int s, int fd)
{
	char buf[1];
	struct iovec iov;
	struct msghdr msg;
	int n;
	Sendfd sfd;

	buf[0] = 0;
	iov.iov_base = buf;
	iov.iov_len = 1;

	memset(&msg, 0, sizeof msg);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	sfd.cmsg.cmsg_len = sizeof sfd;
	sfd.cmsg.cmsg_level = SOL_SOCKET;
	sfd.cmsg.cmsg_type = SCM_RIGHTS;
	sfd.fd = fd;

	msg.msg_control = &sfd;
	msg.msg_controllen = sizeof sfd;

	if((n=sendmsg(s, &msg, 0)) != iov.iov_len)
		return -1;
	return 0;
}

int
recvfd(int s)
{
	int n;
	char buf[1];
	struct iovec iov;
	struct msghdr msg;
	Sendfd sfd;

	iov.iov_base = buf;
	iov.iov_len = 1;

	memset(&msg, 0, sizeof msg);
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	memset(&sfd, 0, sizeof sfd);
	sfd.fd = -1;
	sfd.cmsg.cmsg_len = sizeof sfd;
	sfd.cmsg.cmsg_level = SOL_SOCKET;
	sfd.cmsg.cmsg_type = SCM_RIGHTS;

	msg.msg_control = &sfd;
	msg.msg_controllen = sizeof sfd;

	if((n=recvmsg(s, &msg, 0)) < 0)
		return -1;
	if(n==0 && sfd.fd==-1){
		werrstr("eof in recvfd");
		return -1;
	}
	return sfd.fd;
}
