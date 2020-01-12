#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#include <netinet/in.h>

static char *nets[] = { "tcp", "udp", nil };
#define CLASS(p) ((*(uchar*)(p))>>6)

static struct {
	char *net;
	char *service;
	int port;
} porttbl[] = {
	"tcp",	"9fs",	564,
	"tcp",	"whoami",	565,
	"tcp",	"guard",	566,
	"tcp",	"ticket",	567,
	"tcp",	"exportfs",	17007,
	"tcp",	"rexexec",	17009,
	"tcp",	"ncpu",	17010,
	"tcp",	"cpu",	17013,
	"tcp",	"venti",	17034,
	"tcp",	"wiki",	17035,
	"tcp",	"secstore",	5356,
	"udp",	"dns",	53,
	"tcp",	"dns",	53,
};

static int
setport(struct sockaddr_storage *ss, int port)
{
	switch(ss->ss_family){
	case AF_INET:
		((struct sockaddr_in*)ss)->sin_port = htons(port);
		break;
	case AF_INET6:
		((struct sockaddr_in6*)ss)->sin6_port = htons(port);
		break;
	default:
		errstr("unknown protocol family %d", ss->ss_family);
		return -1;
	}
	return 0;
}

int
p9dialparse(char *addr, char **pnet, char **punix, void *phost, int *pport)
{
	char *net, *host, *port, *e;
	int i;
	struct servent *se;
	struct hostent *he;
	struct sockaddr_storage *ss;
	struct addrinfo *result;

	ss = phost;

	memset(ss, 0, sizeof *ss);

	*punix = nil;
	net = addr;
	if((host = strchr(net, '!')) == nil){
		werrstr("malformed address");
		return -1;
	}
	*host++ = 0;
	if((port = strchr(host, '!')) == nil){
		if(strcmp(net, "unix")==0 || strcmp(net, "net")==0){
		Unix:
			if(strlen(host)+1 > sizeof ((struct sockaddr_un*)ss)->sun_path){
				werrstr("unix socket name too long");
				return -1;
			}
			*punix = host;
			*pnet = "unix";
			ss->ss_family = AF_UNIX;
			strcpy(((struct sockaddr_un*)ss)->sun_path, host);
			*pport = 0;
			return 0;
		}
		werrstr("malformed address");
		return -1;
	}
	*port++ = 0;

	if(*host == 0){
		werrstr("malformed address (empty host)");
		return -1;
	}
	if(*port == 0){
		werrstr("malformed address (empty port)");
		return -1;
	}

	if(strcmp(net, "unix") == 0)
		goto Unix;

	if(strcmp(net, "tcp")!=0 && strcmp(net, "udp")!=0 && strcmp(net, "net") != 0){
		werrstr("bad network %s!%s!%s", net, host, port);
		return -1;
	}

	/* translate host */
	if(strcmp(host, "*") == 0){
		ss->ss_family = AF_INET6;
		((struct sockaddr_in6*)ss)->sin6_addr = in6addr_any;
	}else if((he = gethostbyname(host)) != nil && he->h_addr_list[0] != nil){
		ss->ss_family = he->h_addrtype;
		switch(ss->ss_family){
		case AF_INET:
			((struct sockaddr_in*)ss)->sin_addr = *(struct in_addr*) *(he->h_addr_list);
			break;
		case AF_INET6:
			((struct sockaddr_in6*)ss)->sin6_addr = *(struct in6_addr*) *(he->h_addr_list);
			break;
		default:
			errstr("unknown protocol family %d", ss->ss_family);
			return -1;
		}
	}else if(getaddrinfo(host, NULL, NULL, &result) == 0) {
		switch (result->ai_family) {
		case AF_INET:
			memmove((struct sockaddr_in*)ss, result->ai_addr, result->ai_addrlen);
			break;
		case AF_INET6:
			memmove((struct sockaddr_in6*)ss, result->ai_addr, result->ai_addrlen);
			break;
		default:
			errstr("unknown protocol family %d", ss->ss_family);
			return -1;
		}
	}else{
		werrstr("unknown host %s", host);
		return -1;
	}

	/* translate network and port; should return list rather than first */
	if(strcmp(net, "net") == 0){
		for(i=0; nets[i]; i++){
			if((se = getservbyname(port, nets[i])) != nil){
				*pnet = nets[i];
				*pport = ntohs(se->s_port);
				return setport(ss, *pport);
			}
		}
	}

	for(i=0; i<nelem(porttbl); i++){
		if(strcmp(net, "net") == 0 || strcmp(porttbl[i].net, net) == 0)
		if(strcmp(porttbl[i].service, port) == 0){
			*pnet = porttbl[i].net;
			*pport = porttbl[i].port;
			return setport(ss, *pport);
		}
	}

	if(strcmp(net, "net") == 0){
		werrstr("unknown service net!*!%s", port);
		return -1;
	}

	if(strcmp(net, "tcp") != 0 && strcmp(net, "udp") != 0){
		werrstr("unknown network %s", net);
		return -1;
	}

	*pnet = net;
	i = strtol(port, &e, 0);
	if(*e == 0){
		*pport = i;
		return setport(ss, *pport);
	}

	if((se = getservbyname(port, net)) != nil){
		*pport = ntohs(se->s_port);
		return setport(ss, *pport);
	}
	werrstr("unknown service %s!*!%s", net, port);
	return -1;
}
