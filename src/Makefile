DIRS=\
	lib9\
	libbio\
	libdraw\
	libfmt\
	libframe\
	libregexp\
	libthread\
	libutf\
	mk\
	sam\
	samterm\

clean all install:
	for i in $(DIRS); \
	do \
		(echo $$i; cd $$i; $(MAKE) $*); \
	done

