9SRC=..
include $(9SRC)/Makehdr

LIB=libfmt.a

NUM=\
	charstod.$O\
	pow10.$O\

OFILES=\
	dofmt.$O\
	fltfmt.$O\
	fmt.$O\
	fmtfd.$O\
	fmtfdflush.$O\
	fmtlock.$O\
	fmtprint.$O\
	fmtquote.$O\
	fmtrune.$O\
	fmtstr.$O\
	fmtvprint.$O\
	fprint.$O\
	print.$O\
	runefmtstr.$O\
	runeseprint.$O\
	runesmprint.$O\
	runesnprint.$O\
	runesprint.$O\
	runevseprint.$O\
	runevsmprint.$O\
	runevsnprint.$O\
	seprint.$O\
	smprint.$O\
	snprint.$O\
	sprint.$O\
	strtod.$O\
	vfprint.$O\
	vseprint.$O\
	vsmprint.$O\
	vsnprint.$O\
	$(NUM)\
	$(NAN)\

HFILES=\
	fmtdef.h\
	$(9SRC)/include/fmt.h\

include $(9SRC)/Makesyslib

$(NAN).$O: nan.h
strtod.$O: nan.h

test: $(LIB) test.$O
	$(CC) -o test test.$O $(LIB) -L$(9SRC)/lib -lutf

