
$(TARG): $(OFILES)
	$(CC) -o $(TARG) $(OFILES) -L$(PREFIX)/lib -lframe -ldraw -lthread -l9 -lregexp9 -lbio -lfmt -lutf -L/usr/X11R6/lib -lX11 -lm


