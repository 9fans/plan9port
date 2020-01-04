#if F == 0
#	define FF regexec
typedef char T;
#elif F == 1
#	define FF rregexec
typedef Rune T;
#else
#	error "Unexpected definition of F!"
#endif

// static size_t sizePart(const uint8_t *data, size_t size, size_t partSizes[], int n);
#include ".sizePart.c"

void
regerror(char *unused) {
	return;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	size_t partSizes[2] = {sizeof(char), sizeof(T)};
	size_t headerSize = sizePart(data, size, partSizes, sizeof(partSizes) / sizeof(partSizes[0]));
	if (size < headerSize || partSizes[0] == 0 || partSizes[1] == 0) {
		return 0;
	}
	char *part0 = (char *)malloc(partSizes[0]);
	T *part1 = (T *)malloc(partSizes[1]);
	// We ignore the last member of these two parts of |data|, maybe that could be fixed.
	memcpy(part0, &data[headerSize], partSizes[0] - sizeof(part0[0]));
	part0[partSizes[0] / sizeof(part0[0]) - 1] = 0;
	headerSize += partSizes[0];
	memcpy(part1, &data[headerSize], partSizes[1] - sizeof(part1[0]));
	part1[partSizes[1] / sizeof(part1[0]) - 1] = 0;

	Reprog *pr = regcomplit(part0);
	if (pr == 0) {
		goto OUT;
	}

	FF(pr, part1, nil, 0);

OUT:
	free(pr);
	free(part1);
	free(part0);

	return 0;
}
