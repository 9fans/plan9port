
$(TARG): $(OFILES)
	$(CC) -o $(TARG) $(OFILES) -L$(PREFIX)/lib -L../libutf -L../libfmt -L../libbio -L../libregexp -lregexp9 -lbio -lfmt -lutf

