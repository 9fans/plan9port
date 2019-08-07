void *
emalloc(ulong size) {
	return calloc(1, size);
}

void *
erealloc(void* p, ulong size) {
	return realloc(p, size);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	Rune pagesrc[] = {0};
	Docinfo *pdi;
	freeitems(parsehtml(data, size, pagesrc, mType, chSet, &pdi));
	freedocinfo(pdi);
	return 0;
}
