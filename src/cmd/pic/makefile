CC = cc     # the usual situation
CFLAGS =    # the usual situation

CC = lcc        # you will probably want to remove this 
CFLAGS = -g -N -I/usr/include/lcc -I/usr/include       # and this

YFLAGS = -d

OFILES = picl.o main.o print.o misc.o symtab.o blockgen.o boxgen.o \
	 circgen.o arcgen.o linegen.o movegen.o textgen.o \
	 input.o for.o pltroff.o $(ALLOC)
CFILES = main.c print.c misc.c symtab.c blockgen.c boxgen.c circgen.c \
	 arcgen.c linegen.c movegen.c textgen.c \
	 input.c for.c pltroff.c
SRCFILES = picy.y picl.l pic.h $(CFILES) makefile FIXES README PS-PEmacros

pic:	picy.o $(OFILES) pic.h
	$(CC) $(CFLAGS) picy.o $(OFILES) -lm

$(OFILES):	pic.h prevy.tab.h

picy.o:	pic.h

prevy.tab.h:	y.tab.h
	-cmp -s y.tab.h prevy.tab.h || cp y.tab.h prevy.tab.h

bundle:
	@bundle $(SRCFILES) 

bowell:	 $(SRCFILES)  pictest.a 
	push bowell $? /usr/src/cmd/pic
	touch bowell

clean:
	rm *.o a.out *y.tab.h

install:
	cp a.out /usr/bin/pic
	strip /usr/bin/pic
