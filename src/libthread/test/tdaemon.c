#include <u.h>
#include <libc.h>
#include <thread.h>

void
threadmain(int argc, char **argv)
{
	threaddaemonize();
	sleep(5*1000);
	print("still running\n");
}
