#include <errno.h>
#include <stdio.h>
#include <unistd.h>

static
void
getReaderAndWriter(int fDs[2]) {
	if (pipe(fDs) < 0) {
		int e = errno;
		fprintf(stderr, "errno: %2d\n", e);

		// |pipe| failing is a process or system level error, there is nothing smart we can do (?).
		__builtin_trap();
	}
}
