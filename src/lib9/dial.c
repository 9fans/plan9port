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
#include <sys/un.h>
#include <netdb.h>


extern int _p9dialparse(char*, char**, char**, u32int*, int*);
#undef unix

int
p9dial(char *addr, char *dummy1, char *dummy2, int *dummy3)
{
	char *buf;
	char *net, *unix;
	u32int host;
	int port;
	int proto;
	struct sockaddr_in sa;	
	struct sockaddr_un su;
	int s;

	if(dummy1 || dummy2 || dummy3){
		werrstr("cannot handle extra arguments in dial");
		return -1;
	}

	buf = strdup(addr);
	if(buf == nil)
		return -1;

	if(_p9dialparse(buf, &net, &unix, &host, &port) < 0){
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

	memset(&sa, 0, sizeof sa);
	memmove(&sa.sin_addr, &host, 4);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	if((s = socket(AF_INET, proto, 0)) < 0)
		return -1;
	if(connect(s, (struct sockaddr*)&sa, sizeof sa) < 0){
		close(s);
		return -1;
	}
	return s;

Unix:
	memset(&su, 0, sizeof su);
	su.sun_len = sizeof su;
	su.sun_family = AF_UNIX;
	if(strlen(unix)+1 > sizeof su.sun_path){
		werrstr("unix socket name too long");
		free(buf);
		return -1;
	}
	strcpy(su.sun_path, unix);
	free(buf);
	if((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
	if(connect(s, (struct sockaddr*)&su, sizeof su) < 0){
		close(s);
		return -1;
	}
	return s;
}

