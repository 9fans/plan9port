9SRC=..
include $(9SRC)/Makehdr

TARG=samterm
OFILES=\
	main.$O\
	icons.$O\
	menu.$O\
	mesg.$O\
	rasp.$O\
	scroll.$O\
	flayer.$O\
	io.$O\
	plan9.$O\

HFILES=\
	samterm.h\
	flayer.h\
	../sam/sam.h\
	../sam/mesg.h\
	$(9SRC)/include/frame.h\
	$(9SRC)/include/draw.h\

CFLAGS+=-I../sam
LDFLAGS+=-lframe -ldraw -lthread -l9 -lregexp9 -lbio -lfmt -lutf -L$(X11)/lib -lX11 -lm

samterm: $(9SRC)/lib/libdraw.a

include ../Makeone
