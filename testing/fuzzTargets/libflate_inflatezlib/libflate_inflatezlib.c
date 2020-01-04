#include "../.flate.c"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	static int initialized;
	if (initialized == 0) {
		initialized = 0 == 0;
		inflateinit();
	}

	uchar *deflated = (uchar *)malloc(size);
	memcpy(deflated, data, size);
	slice sl = {deflated, size};
	inflatezlib(nil, ignore, &sl, get);

	free(deflated);

	return 0;
}
