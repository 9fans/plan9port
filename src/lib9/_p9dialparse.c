#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/types.h>
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
parseip(char *host, u32int *pip)
{
	uchar addr[4];
	int x, i;
	char *p;

	p = host;
	for(i=0; i<4 && *p; i++){
		x = strtoul(p, &p, 0);
		if(x < 0 || x >= 256)
			return -1;
		if(*p != '.' && *p != 0)
			return -1;
		if(*p == '.')
			p++;
		addr[i] = x;
	}

	switch(CLASS(addr)){
	case 0:
	case 1:
		if(i == 3){
			addr[3] = addr[2];
			addr[2] = addr[1];
			addr[1] = 0;
		}else if(i == 2){
			addr[3] = addr[1];
			addr[2] = 0;
			addr[1] = 0;
		}else if(i != 4)
			return -1;
		break;
	case 2:
		if(i == 3){
			addr[3] = addr[2];
			addr[2] = 0;
		}else if(i != 4)
			return -1;
		break;
	}
	memmove(pip, addr, 4);
	return 0;
}

int
p9dialparse(char *addr, char **pnet, char **punix, u32int *phost, int *pport)
{
	char *net, *host, *port, *e;
	int i;
	struct servent *se;
	struct hostent *he;
	struct sockaddr_un *sockun;

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
			if(strlen(host)+1 > sizeof sockun->sun_path){
				werrstr("unix socket name too long");
				return -1;
			}
			*punix = host;
			*pnet = "unix";
			*phost = 0;
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
	if(strcmp(host, "*") == 0)
		*phost = 0;
	else if(parseip(host, phost) == 0)
		{}
	else if((he = gethostbyname(host)) != nil)
		*phost = *(u32int*)(he->h_addr);
	else{
		werrstr("unknown host %s", host);
		return -1;
	}

	/* translate network and port; should return list rather than first */
	if(strcmp(net, "net") == 0){
		for(i=0; nets[i]; i++){
			if((se = getservbyname(port, nets[i])) != nil){
				*pnet = nets[i];
				*pport = ntohs(se->s_port);
				return 0;
			}
		}
	}

	for(i=0; i<nelem(porttbl); i++){
		if(strcmp(net, "net") == 0 || strcmp(porttbl[i].net, net) == 0)
		if(strcmp(porttbl[i].service, port) == 0){
			*pnet = porttbl[i].net;
			*pport = porttbl[i].port;
			return 0;
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
		return 0;
	}

	if((se = getservbyname(port, net)) != nil){
		*pport = ntohs(se->s_port);
		return 0;
	}
	werrstr("unknown service %s!*!%s", net, port);
	return -1;
}
