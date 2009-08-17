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

int
p9dial(char *addr, char *local, char *dummy2, int *dummy3)
{
	char *buf;
	char *net, *unix;
	u32int host;
	int port;
	int proto;
	socklen_t sn;
	int n;
	struct sockaddr_in sa, sal;	
	struct sockaddr_un su;
	int s;

	if(dummy2 || dummy3){
		werrstr("cannot handle extra arguments in dial");
		return -1;
	}

	buf = strdup(addr);
	if(buf == nil)
		return -1;

	if(p9dialparse(buf, &net, &unix, &host, &port) < 0){
		free(buf);
		return -1;
	}
	if(strcmp(net, "unix") != 0 && host == 0){
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

	if((s = socket(AF_INET, proto, 0)) < 0)
		return -1;
		
	if(local){
		buf = strdup(local);
		if(buf == nil){
			close(s);
			return -1;
		}
		if(p9dialparse(buf, &net, &unix, &host, &port) < 0){
		badlocal:
			free(buf);
			close(s);
			return -1;
		}
		if(unix){
			werrstr("bad local address %s for dial %s", local, addr);
			goto badlocal;
		}
		memset(&sal, 0, sizeof sal);
		memmove(&sal.sin_addr, &local, 4);
		sal.sin_family = AF_INET;
		sal.sin_port = htons(port);
		sn = sizeof n;
		if(port && getsockopt(s, SOL_SOCKET, SO_TYPE, (void*)&n, &sn) >= 0
		&& n == SOCK_STREAM){
			n = 1;
			setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof n);
		}
		if(bind(s, (struct sockaddr*)&sal, sizeof sal) < 0)
			goto badlocal;
		free(buf);
	}

	n = 1;
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, &n, sizeof n);
	if(host != 0){
		memset(&sa, 0, sizeof sa);
		memmove(&sa.sin_addr, &host, 4);
		sa.sin_family = AF_INET;
		sa.sin_port = htons(port);
		if(connect(s, (struct sockaddr*)&sa, sizeof sa) < 0){
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
	if((s = open(unix, ORDWR)) >= 0)
		return s;
	memset(&su, 0, sizeof su);
	su.sun_family = AF_UNIX;
	if(strlen(unix)+1 > sizeof su.sun_path){
		werrstr("unix socket name too long");
		free(buf);
		return -1;
	}
	strcpy(su.sun_path, unix);
	free(buf);
	if((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		werrstr("socket: %r");
		return -1;
	}
	if(connect(s, (struct sockaddr*)&su, sizeof su) < 0){
		werrstr("connect %s: %r", su.sun_path);
		close(s);
		return -1;
	}
	return s;
}

