#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include <sys/socket.h>

/* BUG: would like to preserve delimiters on systems that can */
int
p9pipe(int fd[2])
{
	return socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
}
