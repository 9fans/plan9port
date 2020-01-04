#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	char *buf = malloc(size + sizeof(""));
	memcpy(buf, data, size);
	buf[size] = '\0';
	utflen(buf);
	free(buf);
	return 0;
}
