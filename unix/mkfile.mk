MAKEALL=`{cd ../make; ls Make.*}
TARG=\
	$MAKEALL\
	NOTICE\
	README\
	Makefile\
	mk.1\
	mk.h\
	fns.h\
	`{ls -p $PLAN9/src/cmd/mk/*.c}\

all:V: $TARG

%: $PLAN9/include/%
	cp $prereq $target

%: $PLAN9/src/cmd/mk/%
	cp $prereq $target

%: $PLAN9/man/man1/%
	cp $prereq $target

%: $PLAN9/man/man3/%
	cp $prereq $target

%: $PLAN9/man/man7/%
	cp $prereq $target

%: ../make/%
	cp $prereq $target

Makefile:D: ../make/Makefile.TOP ../make/Makefile.mk ../make/Makefile.CMD ../make/Makefile.BOT
	cat $prereq >$target

README: ../README
	cp $prereq $target

NOTICE: ../NOTICE.mk
	cp $prereq $target
