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

clean all install nuke:
	for i in $(LIBS) $(DIRS); \
	do \
		(echo $$i; cd $$i; $(MAKE) $@); \
	done

