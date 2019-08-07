#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

static
void
readerFilDesFromBuf(const char *buf, size_t size, int fDs[2]) {
	getReaderAndWriter(fDs);

	// TODO: threaded version that writes in a loop in a new thread?
	if (write(fDs[1], buf, size) <= 0) {
		int e = errno;
		fprintf(stderr, "errno: %2d\n", e);

		// TODO: check for partial reads?
		// TODO: check errno?
		__builtin_trap();
	}
	lseek(fDs[0], 0, SEEK_SET);
#ifndef OPEN_GETS_READERWRITER
	// Set the file descriptor that will be used by the fuzz target to be non-blocking, as we do not want it to block eternally.
	//
	// We could instead try getting really fine grained control of timeouts in the fuzzed plan9port APIs, but I do not see what is wrong with this approach.
	int fl = fcntl(fDs[0], F_GETFL);
	if (fl < 0) {
		int e = errno;
		fprintf(stderr, "errno: %2d\n", e);

		// |fcntl| failing is a process or system level error, there is nothing smart we can do (?).
		__builtin_trap();
	}
	fl = fcntl(fDs[0], F_SETFL, fl | O_NONBLOCK);
	if (fl < 0) {
		int e = errno;
		fprintf(stderr, "errno: %2d\n", e);

		// |fcntl| failing is a process or system level error, there is nothing smart we can do (?).
		__builtin_trap();
	}
#endif
}

static
void
closeSecondFD(const int fDs[2]) {
#ifndef OPEN_GETS_READERWRITER
	if (close(fDs[1]) < 0) {
		int e = errno;
		fprintf(stderr, "errno: %2d\n", e);

		// |close| failing is a process or system level error, there is nothing smart we can do (?).
		__builtin_trap();
	}
#endif
}
