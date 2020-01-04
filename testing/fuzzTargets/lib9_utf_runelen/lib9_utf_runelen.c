#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size != sizeof(long)) {
		return 0;
	}
	long r;
	memcpy(&r, data, sizeof(r));
	int bytes = runelen(r);
	if (bytes < 1 || UTFmax < bytes) {
		__builtin_trap();
	}
	return 0;
}
