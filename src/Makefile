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
	mk\
	sam\
	samterm\

all clean install nuke:
	for i in $(LIBS) $(CMDS); \
	do \
		(echo $$i; cd $$i; $(MAKE) $@); \
	done

