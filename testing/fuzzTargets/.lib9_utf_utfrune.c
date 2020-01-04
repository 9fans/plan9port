int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size < sizeof(long)) {
		return 0;
	}
	long c;
	memcpy(&c, data, sizeof(c));
	size -= sizeof(long);
	char *s = malloc(size + sizeof(""));
	if (size != 0) {
		memcpy(s, &data[sizeof(long)], size);
	}
	s[size] = '\0';
	F(s, c);
	free(s);
	return 0;
}
