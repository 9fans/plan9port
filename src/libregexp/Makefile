PLAN9=../..
include $(PLAN9)/src/Makehdr

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
	$(PLAN9)/include/regexp9.h\
	regcomp.h\

include $(PLAN9)/src/Makesyslib

test: test.$O $(LIB)
	$(CC) -o test test.$O $(LIB) -L/usr/local/lib -lfmt -lutf

test2: test2.$O $(LIB)
	$(CC) -o test2 test2.$O $(LIB) -L/usr/local/lib -lfmt -lutf

