int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size == 0) {
		return 0;
	}
	int fDs[2];
	readerFilDesFromBuf(data, size, fDs);
	int fD = fDs[0];

	HConnect *c = calloc(1, sizeof(*c));
	c->replog = nil;
	c->hpos = c->header;
	c->hstop = c->header;

	hinit(&c->hin, fD, Hread);
	hinit(&c->hout, fD, Hwrite);

	if(hparsereq(c, 0) < 0) {
		goto OUT;
	}

	hparseheaders(c, 0);

OUT:
	hreqcleanup(c);
	close(c->hin.fd);
	closeSecondFD(fDs);
	free(c);

	return 0;
}
