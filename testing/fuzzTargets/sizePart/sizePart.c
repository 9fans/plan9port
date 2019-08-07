#include <u.h>
#include <libc.h>

// static int sizePart(const uint8_t *data, size_t size, size_t partSizes[], int n);
#include "../.sizePart.c"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	enum {
		maxPartitions = 4,
		headerSize = (maxPartitions - 1) * sizeof(uint32_t)
	};
	if (size != sizeof(char) + 2*sizeof(char) + headerSize) {
		return 0;
	}

	size_t partSizes[maxPartitions], partSizesBackup[maxPartitions];
	partSizesBackup[0] = partSizes[0] = (data[0] & 0x03) + 1;
	partSizesBackup[1] = partSizes[1] = ((data[0] & (0x03 << 2)) >> 2) + 1;
	partSizesBackup[2] = partSizes[2] = ((data[0] & (0x03 << 4)) >> 4) + 1;
	partSizesBackup[3] = partSizes[3] = ((data[0] & (0x03 << 6)) >> 6) + 1;

	size_t pretendSize = data[1] | (data[2] << 8);
	if (pretendSize < headerSize) {
		return 0;
	}

	uint8_t *pretendData = (uint8_t *)malloc(headerSize);
	memcpy(pretendData, &data[sizeof(char) + 2*sizeof(char)], headerSize);

	int n;
	for (n = 2; n <= maxPartitions; n++) {
		int r = sizePart(pretendData, pretendSize, partSizes, n);

		int i;

		for (i = 0; i < n; i++) {
			if (~(size_t)0 >> 2 < partSizes[i]) {
				__builtin_trap();
			}
			r += partSizes[i];
		}
		if (r + partSizesBackup[n - 1] - 1 < pretendSize) {
			__builtin_trap();
		}

		for (i = 0; i < n; i++) {
			if (partSizes[i] % partSizesBackup[i] != 0) {
				__builtin_trap();
			}
		}

		for (i = 0; i < n; i++) {
			partSizes[i] = partSizesBackup[i];
		}
	}

	free(pretendData);

	return 0;
}
