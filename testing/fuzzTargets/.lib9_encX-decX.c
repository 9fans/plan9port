#include <stdio.h>

#if ENCODING_BASE == 32
#	define ENCODED_SIZE(N) ((N) + 4) / 5 * 8
#	define ENC enc32
#	define DEC dec32
#elif ENCODING_BASE == 64
#	define ENCODED_SIZE(N) ((N) + 2) / 3 * 4
#	define ENC enc64
#	define DEC dec64
#else
#	error "Unexpected definition of ENCODING_BASE!"
#endif

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	int outLen = ENCODED_SIZE(size) + sizeof("");
	uchar *out = (uchar *)malloc(outLen);
	int n = ENC(out, outLen, data, size);
	if (n < 0) {
		__builtin_trap();
	}

#if ENCODING_BASE == 32
	if (n % 8 != 0) {
		goto OUT;
	}
#endif

	uchar *dec = (uchar *)malloc(size);
	int m = DEC(dec, size, out, n);
	if (m != size) {
		fprintf(stderr, "n = %d\nm = %d\n", n, m);
		__builtin_trap();
	}
	if (memcmp(data, dec, size) != 0) {
		__builtin_trap();
	}
	free(dec);

OUT:
	free(out);
	return 0;
}
