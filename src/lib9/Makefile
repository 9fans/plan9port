9SRC=..
include $(9SRC)/Makehdr

LIB=lib9.a

OFILES=\
	_exits.$O\
	argv0.$O\
	await.$O\
	encodefmt.$O\
	errstr.$O\
	exits.$O\
	ffork-$(SYSNAME).$O\
	getcallerpc-$(OBJTYPE).$O\
	getfields.$O\
	lock.$O\
	malloctag.$O\
	mallocz.$O\
	nrand.$O\
	qlock.$O\
	readn.$O\
	rendez-$(SYSNAME).$O\
	strecpy.$O\
	sysfatal.$O\
	tas-$(OBJTYPE).$O\
	tokenize.$O\
	u16.$O\
	u32.$O\
	u64.$O\
	wait.$O\
	werrstr.$O\

HFILES=\
	$(9SRC)/include/lib9.h\

include $(9SRC)/Makesyslib

