#include <u.h>
#include <libc.h>

#undef	accept
#undef	announce
#undef	dial
#undef	setnetmtpt
#undef	hangup
#undef	listen
#undef	netmkaddr
#undef	reject

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <netdb.h>

#undef unix
#define unix xunix

static int
isany(struct sockaddr_storage *ss)
{
	switch(ss->ss_family){
	case AF_INET:
		return (((struct sockaddr_in*)ss)->sin_addr.s_addr == INADDR_ANY);
	case AF_INET6:
		return (memcmp(((struct sockaddr_in6*)ss)->sin6_addr.s6_addr,
			in6addr_any.s6_addr, sizeof (struct in6_addr)) == 0);
	}
	return 0;
}

static int
addrlen(struct sockaddr_storage *ss)
{
	switch(ss->ss_family){
	case AF_INET:
		return sizeof(struct sockaddr_in);
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
	case AF_UNIX:
		return sizeof(struct sockaddr_un);
	}
	return 0;
}

int
p9dial(char *addr, char *local, char *dummy2, int *dummy3)
{
	char *buf;
	char *net, *unix;
	int port;
	int proto;
	socklen_t sn;
	int n;
	struct sockaddr_storage ss, ssl;
	int s;

	if(dummy2 || dummy3){
		werrstr("cannot handle extra arguments in dial");
		return -1;
	}

	buf = strdup(addr);
	if(buf == nil)
		return -1;

	if(p9dialparse(buf, &net, &unix, &ss, &port) < 0){
		free(buf);
		return -1;
	}
	if(strcmp(net, "unix") != 0 && isany(&ss)){
		werrstr("invalid dial address 0.0.0.0 (aka *)");
		free(buf);
		return -1;
	}

	if(strcmp(net, "tcp") == 0)
		proto = SOCK_STREAM;
	else if(strcmp(net, "udp") == 0)
		proto = SOCK_DGRAM;
	else if(strcmp(net, "unix") == 0)
		goto Unix;
	else{
		werrstr("can only handle tcp, udp, and unix: not %s", net);
		free(buf);
		return -1;
	}
	free(buf);

	if((s = socket(ss.ss_family, proto, 0)) < 0)
		return -1;

	if(local){
		buf = strdup(local);
		if(buf == nil){
			close(s);
			return -1;
		}
		if(p9dialparse(buf, &net, &unix, &ss, &port) < 0){
		badlocal:
			free(buf);
			close(s);
			return -1;
		}
		if(unix){
			werrstr("bad local address %s for dial %s", local, addr);
			goto badlocal;
		}
		sn = sizeof n;
		if(port && getsockopt(s, SOL_SOCKET, SO_TYPE, (void*)&n, &sn) >= 0
		&& n == SOCK_STREAM){
			n = 1;
			setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof n);
		}
		if(bind(s, (struct sockaddr*)&ssl, addrlen(&ssl)) < 0)
			goto badlocal;
		free(buf);
	}

	n = 1;
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, &n, sizeof n);
	if(!isany(&ss)){
		if(connect(s, (struct sockaddr*)&ss, addrlen(&ss)) < 0){
			close(s);
			return -1;
		}
	}
	if(proto == SOCK_STREAM){
		int one = 1;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof one);
	}
	return s;

Unix:
	if(local){
		werrstr("local address not supported on unix network");
		free(buf);
		return -1;
	}
	/* Allow regular files in addition to Unix sockets. */
	if((s = open(unix, ORDWR)) >= 0){
		free(buf);
		return s;
	}
	free(buf);
	if((s = socket(ss.ss_family, SOCK_STREAM, 0)) < 0){
		werrstr("socket: %r");
		return -1;
	}
	if(connect(s, (struct sockaddr*)&ss, addrlen(&ss)) < 0){
		werrstr("connect %s: %r", ((struct sockaddr_un*)&ss)->sun_path);
		close(s);
		return -1;
	}
	return s;
}
