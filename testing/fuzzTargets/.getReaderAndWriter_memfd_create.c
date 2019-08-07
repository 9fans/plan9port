#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#define OPEN_GETS_READERWRITER

static
void
getReaderAndWriter(int fDs[2]) {
	int fD = syscall(SYS_memfd_create, "plan9portTesting", 0);
	if (fD < 0) {
		int e = errno;
		fprintf(stderr, "errno: %2d\n", e);

		// |open| failing is a process or system level error, there is nothing smart we can do (?).
		__builtin_trap();
	}
	fDs[0] = fDs[1] = fD;
}
