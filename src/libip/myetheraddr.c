#include <u.h>
#include <libc.h>
#include <ip.h>

int
myetheraddr(uchar *to, char *dev)
{
	int n, fd;
	char buf[256], *ptr;

	/* Make one exist */
	if(*dev == '/')
		sprint(buf, "%s/clone", dev);
	else
		sprint(buf, "/net/%s/clone", dev);
	fd = open(buf, ORDWR);
	if(fd >= 0)
		close(fd);

	if(*dev == '/')
		sprint(buf, "%s/0/stats", dev);
	else
		sprint(buf, "/net/%s/0/stats", dev);
	fd = open(buf, OREAD);
	if(fd < 0)
		return -1;

	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return -1;
	buf[n] = 0;

	ptr = strstr(buf, "addr: ");
	if(!ptr)
		return -1;
	ptr += 6;

	parseether(to, ptr);
	return 0;
}
