PLAN9=../../..
include $(PLAN9)/src/Makehdr

TARG=sam
OFILES=sam.$O\
	address.$O\
	buff.$O\
	cmd.$O\
	disk.$O\
	error.$O\
	file.$O\
	io.$O\
	list.$O\
	mesg.$O\
	moveto.$O\
	multi.$O\
	plumb.$O\
	rasp.$O\
	regexp.$O\
	shell.$O\
	string.$O\
	sys.$O\
	unix.$O\
	util.$O\
	xec.$O\

HFILES=sam.h\
	errors.h\
	mesg.h\

LDFLAGS+=-l9 -lfmt -lutf

include $(PLAN9)/src/Makeone

