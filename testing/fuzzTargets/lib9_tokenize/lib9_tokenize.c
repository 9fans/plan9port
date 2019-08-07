#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	unsigned char maxargs;
	if (size < sizeof(maxargs)) {
		return 0;
	}
	memcpy(&maxargs, data, sizeof(maxargs));
	data = &data[sizeof(maxargs)];
	size -= sizeof(maxargs);
	uint8_t *safeData = malloc(size + sizeof(""));
	memcpy(safeData, data, size);
	safeData[size] = '\0';

	int max = ((int)maxargs + 1);
	char **args = malloc(sizeof(char *) * max);
	tokenize(safeData, args, max);
	free(args);
	free(safeData);

	return 0;
}
