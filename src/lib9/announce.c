#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

extern int _p9dialparse(char*, char**, char**, u32int*, int*);

static int
getfd(char *dir)
{
	int fd;

	if(strncmp(dir, "/dev/fd/", 8) != 0)
		return -1;
	fd = strtol(dir+8, &dir, 0);
	if(*dir != 0)
		return -1;
	return fd;
}

static void
putfd(char *dir, int fd)
{
	snprint(dir, NETPATHLEN, "/dev/fd/%d", fd);
}

#undef unix

int
p9announce(char *addr, char *dir)
{
	int proto;
	char *buf, *unix;
	char *net;
	u32int host;
	int port, s;
	int n, sn;
	struct sockaddr_in sa;
	struct sockaddr_un sun;

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
	sn = sizeof n;
	if(port && getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&n, &sn) >= 0
	&& n == SOCK_STREAM){
		n = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof n);
	}
	if(bind(s, (struct sockaddr*)&sa, sizeof sa) < 0){
		close(s);
		return -1;
	}
	if(proto == SOCK_STREAM){
		listen(s, 8);
		putfd(dir, s);
print("announce dir: %s\n", dir);
	}
	return s;

Unix:
	memset(&sun, 0, sizeof sun);
	sun.sun_family = AF_UNIX;
	sun.sun_len = sizeof sun;
	strcpy(sun.sun_path, unix);
	if((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
	sn = sizeof sun;
	if(bind(s, (struct sockaddr*)&sun, sizeof sun) < 0){
		close(s);
		return -1;
	}
	listen(s, 8);
	putfd(dir, s);
	return s;
}

int
p9listen(char *dir, char *newdir)
{
	int fd;

	if((fd = getfd(dir)) < 0){
		werrstr("bad 'directory' in listen: %s", dir);
		return -1;
	}

print("accept %d", fd);
	if((fd = accept(fd, nil, nil)) < 0)
		return -1;
print(" -> %d\n", fd);

	putfd(newdir, fd);
print("listen dir: %s\n", newdir);
	return fd;
}

int
p9accept(int cfd, char *dir)
{
	int fd;

	if((fd = getfd(dir)) < 0){
		werrstr("bad 'directory' in accept");
		return -1;
	}
	/* need to dup because the listen fd will be closed */
	return dup(fd);
}

