9SRC=..
include $(9SRC)/Makehdr

LIB=libbio.a

OFILES=\
	bbuffered.$O\
	bfildes.$O\
	bflush.$O\
	bgetc.$O\
	bgetd.$O\
	binit.$O\
	boffset.$O\
	bprint.$O\
	bputc.$O\
	brdline.$O\
	brdstr.$O\
	bread.$O\
	bseek.$O\
	bwrite.$O\
	bgetrune.$O\
	bputrune.$O\

HFILES=\
	bio.h\

include $(9SRC)/Makesyslib

bcat: bcat.$O $(LIB)
	$(CC) -o bcat bcat.$O -L$(9SRC)/lib -lbio -lfmt -lutf

