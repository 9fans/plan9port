#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size >> (8 * sizeof(int) - 1) != 0) {
		return 0;
	}
	if (!fullrune(data, size) && UTFmax <= size) {
		__builtin_trap();
	}
	return 0;
}
