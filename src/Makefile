LIBS=\
	lib9\
	libbio\
	libdraw\
	libfmt\
	libframe\
	libregexp\
	libthread\
	libutf\

CMDS=\
	cmd/mk\
	cmd/sam\
	cmd/samterm\

all clean install nuke:
	for i in $(LIBS) $(CMDS); \
	do \
		(echo $$i; cd $$i; $(MAKE) $@); \
	done

