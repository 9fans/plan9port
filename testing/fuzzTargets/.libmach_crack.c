int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size == 0) {
		return 0;
	}
	Fhdr *fHdr = calloc(1, sizeof(*fHdr));
	fHdr->filename = strdup("forTesting/notAFile");
	int fDs[2];
	readerFilDesFromBuf(data, size, fDs);
	int fD = fDs[0];
	if (CRACK(fD, fHdr) < 0) {
		free(fHdr->filename);
		free(fHdr);
		close(fD);
		goto OUT;
	}
	uncrackhdr(fHdr);
OUT:
	closeSecondFD(fDs);
	return 0;
}
