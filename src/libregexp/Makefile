9SRC=..
include $(9SRC)/Makehdr

LIB=libregexp9.a

OFILES=\
	regcomp.$O\
	regerror.$O\
	regexec.$O\
	regsub.$O\
	regaux.$O\
	rregaux.$O\
	rregexec.$O\
	rregsub.$O\

HFILES=\
	$(9SRC)/include/regexp9.h\
	regcomp.h\

include $(9SRC)/Makesyslib

test: test.$O $(LIB)
	$(CC) -o test test.$O $(LIB) -L/usr/local/lib -lfmt -lutf

test2: test2.$O $(LIB)
	$(CC) -o test2 test2.$O $(LIB) -L/usr/local/lib -lfmt -lutf

