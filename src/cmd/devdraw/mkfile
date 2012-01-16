<$PLAN9/src/mkhdr
<|osxvers

TARG=devdraw

WSYSOFILES=\
	devdraw.$O\
	latin1.$O\
	mouseswap.$O\
	winsize.$O\

<|sh ./mkwsysrules.sh

OFILES=$WSYSOFILES

HFILES=\
	devdraw.h\

<$PLAN9/src/mkone

$O.drawclient: drawclient.$O drawfcall.$O
	$LD -o $target $prereq

$O.snarf: x11-alloc.$O x11-cload.$O x11-draw.$O x11-fill.$O x11-get.$O x11-init.$O x11-itrans.$O x11-keysym2ucs.$O x11-load.$O x11-pixelbits.$O x11-unload.$O x11-wsys.$O snarf.$O latin1.$O devdraw.$O
	$LD -o $target $prereq

$O.mklatinkbd: mklatinkbd.$O
	$LD -o $target $prereq

latin1.$O: latin1.h

latin1.h: $PLAN9/lib/keyboard $O.mklatinkbd
	./$O.mklatinkbd -r $PLAN9/lib/keyboard | sed 's/, }/ }/' >$target

$O.macargv: $MACARGV
	$LD -o $target $prereq

%-objc.$O: %.m
	$CC $CFLAGS -o $target $stem.m

CLEANFILES=$O.macargv $O.mklatinkbd latin1.h

install: mklatinkbd.install
install:Q: 
	if [ $MACARGV ]; then
		mk $MKFLAGS macargv.install
	fi
