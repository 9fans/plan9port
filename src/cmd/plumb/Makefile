
CFLAGS=-I../../../include
LDLIBS=-L../../../lib -lplumb -lbio -lregexp9 -lfmt -l9 -lutf 
all:	plumb plumber

plumb: plumb.o
	cc -o $@ plumb.o $(LDLIBS)

plumber: match.o rules.o plumber.o
	cc -o $@ match.o rules.o plumber.o $(LDLIBS)

clean:
	rm -f *.o

install: plumb plumber
	cp plumb plumber ../../../bin
