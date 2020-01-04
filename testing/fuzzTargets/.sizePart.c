// Computes a possible partition of |data| into |n| partitions. |data|
// has size |size|. The return value is the size in bytes of the |data|
// header that determines the partition of the rest of |data|.
//
// Used for giving the fuzzer the chance for deciding how large each of
// a fixed number of buffers derived from |data| is going to be.
static
size_t
sizePart(const uint8_t *data, size_t size, size_t partSizes[], int n) {
	size_t headerSize = (n - 1) * sizeof(uint32_t);
	if (size < headerSize) {
		return headerSize;
	}
	size -= headerSize;
	int i;
	for (i = 0; i < n - 1; i++) {
		uint32_t rat;
		memcpy(&rat, &data[sizeof(rat) * i], sizeof(rat));
		size_t partSize = (size_t)((double)size * (double)rat / (double)~(uint32_t)0);
		partSize -= partSize % partSizes[i];
		size -= partSize;
		partSizes[i] = partSize;
	}
	size -= size % partSizes[n - 1];
	partSizes[n - 1] = size;
	return headerSize;
}
