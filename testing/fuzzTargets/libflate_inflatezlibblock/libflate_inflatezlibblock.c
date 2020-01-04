#include <u.h>
#include <libc.h>
#include <flate.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	static int initialized;
	if (initialized == 0) {
		initialized = 0 == 0;
		inflateinit();
	}

	uchar *deflated = (uchar *)malloc(size);
	memcpy(deflated, data, size);
	enum {
		maxComprRatio = 12
	};
	uchar *inflated = (uchar *)malloc(maxComprRatio * size);
	inflatezlibblock(inflated, maxComprRatio * size, deflated, size);

	free(inflated);
	free(deflated);

	return 0;
}
