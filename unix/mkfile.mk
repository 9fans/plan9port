MAKEALL=`{cd ../make; ls Make.*}
TARG=\
	$MAKEALL\
	NOTICE\
	README\
	Makefile\
	mk.1\
	mk.h\
	sys.h\
	fns.h\
	mk.pdf\
	`{9 ls -p $PLAN9/src/cmd/mk/*.c}\

WHAT=mk

<../mkfile.what

%: $PLAN9/src/cmd/mk/%
	cp $prereq $target

sys.h: $PLAN9/src/cmd/mk/sys.std.h
	cp $prereq $target
