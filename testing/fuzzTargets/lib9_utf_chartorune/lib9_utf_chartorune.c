#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size != UTFmax) {
		return 0;
	}
	Rune r;
	int bytes = chartorune(&r, data);
	if (bytes < 1 || UTFmax < bytes) {
		__builtin_trap();
	}
	return 0;
}
