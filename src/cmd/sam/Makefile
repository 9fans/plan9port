H=errors.h mesg.h parse.h plumb.h sam.h
SRC= address.c buff.c cmd.c disk.c error.c file.c io.c\
     list.c mesg.c moveto.c multi.c unix.c rasp.c regexp.c\
     sam.c shell.c string.c sys.c util.c xec.c plumb.c

CC=gcc
PREFIX=$(HOME)
#PREFIX=/usr/local
CFLAGS=-I. -I$(PREFIX)/include -O -g
LDFLAGS=-L$(PREFIX)/lib
LDLIBS=-l9 -lfmt -lutf

all: sam
sam: $(SRC) $(H)
	$(CC) -o $@ $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS)
clean:
	rm -f *.o *~
	rm -f sam
