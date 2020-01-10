#include <u.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libc.h>
#include "mountnfs.h"

void
mountnfs(int proto, struct sockaddr_in *addr, uchar *handle, int hlen, char *mtpt)
{
	sysfatal("mountnfs not implemented");
}
