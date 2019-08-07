#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	int outLen = size / 4 * 3;
	uchar *out = (uchar *)malloc(outLen);
	dec64(out, outLen, data, size);
	free(out);
	return 0;
}
