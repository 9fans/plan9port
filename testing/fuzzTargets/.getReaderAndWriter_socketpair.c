#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

static
void
getReaderAndWriter(int fDs[2]) {
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fDs) < 0) {
		int e = errno;
		fprintf(stderr, "errno: %2d\n", e);

		// |socketpair| failing is a process or system level error, there is nothing smart we can do (?).
		__builtin_trap();
	}
}
