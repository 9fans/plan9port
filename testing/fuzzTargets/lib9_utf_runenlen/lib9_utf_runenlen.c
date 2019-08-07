#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size % sizeof(Rune) != 0 || size >> (8 * sizeof(int) - 1) != 0) {
		return 0;
	}
	int n = size / sizeof(Rune);
	int bytes = runenlen(data, n);
	if (bytes < n || UTFmax * n < bytes) {
		__builtin_trap();
	}
	return 0;
}
