
LIB=../../lib/libplumb.a
OFILES=\
	mesg.o\

HFILES=../../include/plumb.h

INCLUDES=-I../../include

CFLAGS += $(INCLUDES) -D_POSIX_SOURCE

CC=cc

$(LIB): $(OFILES)
	ar r $(LIB) $(OFILES)

clean:
	rm -rf $(TARG) $(OFILES)

