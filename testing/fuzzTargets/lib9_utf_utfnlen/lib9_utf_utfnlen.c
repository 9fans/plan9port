#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	utfnlen(data, size);
	return 0;
}
