#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	int outLen = size / 8 * 5;
	uchar *out = (uchar *)malloc(outLen);
	dec32(out, outLen, data, size);
	free(out);
	return 0;
}
