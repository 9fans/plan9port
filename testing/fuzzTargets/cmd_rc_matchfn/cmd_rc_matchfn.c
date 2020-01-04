#include <u.h>
#include <libc.h>

// static size_t sizePart(const uint8_t *data, size_t size, size_t partSizes[], int n);
#include "../.sizePart.c"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	size_t partSizes[2] = {sizeof(char), sizeof(char)};
	size_t headerSize = sizePart(data, size, partSizes, sizeof(partSizes) / sizeof(partSizes[0]));
	if (size < headerSize || partSizes[0] == 0 || partSizes[1] == 0) {
		return 0;
	}
	char *part0 = (char *)malloc(partSizes[0]);
	char *part1 = (char *)malloc(partSizes[1]);
	// We ignore the last member of these two parts of |data|, maybe that could be fixed.
	memcpy(part0, &data[headerSize], partSizes[0] - sizeof(part0[0]));
	part0[partSizes[0] / sizeof(part0[0]) - 1] = 0;
	headerSize += partSizes[0];
	memcpy(part1, &data[headerSize], partSizes[1] - sizeof(part1[0]));
	part1[partSizes[1] / sizeof(part1[0]) - 1] = 0;

	// src/cmd/rc/glob.c
	extern int matchfn(char *s, char *p);
	matchfn(part0, part1);

	free(part1);
	free(part0);

	return 0;
}
