PLAN9=../../..
include $(PLAN9)/src/Makehdr

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
	$(PLAN9)/include/frame.h\
	$(PLAN9)/include/draw.h\

CFLAGS+=-I../sam
LDFLAGS+=-lframe -ldraw -lthread -l9 -lregexp9 -lbio -lfmt -lutf -L$(X11)/lib -lX11 -lm

samterm: $(PLAN9)/lib/libdraw.a

include $(PLAN9)/src/Makeone
