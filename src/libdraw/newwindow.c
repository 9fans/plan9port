#include <u.h>
#include <libc.h>
#include <draw.h>

/* Connect us to new window, if possible */
int
newwindow(char *str)
{
	int fd;
	char *wsys;
	char buf[256];

	wsys = getenv("wsys");
	if(wsys == nil)
		return -1;
	fd = open(wsys, ORDWR);
	free(wsys);
	if(fd < 0)
		return -1;
	rfork(RFNAMEG);
	if(str)
		snprint(buf, sizeof buf, "new %s", str);
	else
		strcpy(buf, "new");
	return mount(fd, -1, "/dev", MBEFORE, buf);
}

