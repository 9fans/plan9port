9SRC=..
include $(9SRC)/Makehdr

LIB=libthread.a

OFILES=\
	$(OBJTYPE).$O\
	asm-$(SYSNAME)-$(OBJTYPE).$O\
	channel.$O\
	chanprint.$O\
	create.$O\
	debug.$O\
	exec-unix.$O\
	exit.$O\
	getpid.$O\
	id.$O\
	iocall.$O\
	ioclose.$O\
	ioopen.$O\
	ioproc.$O\
	ioread.$O\
	ioreadn.$O\
	iowrite.$O\
	kill.$O\
	lib.$O\
	main.$O\
	memset.$O\
	memsetd.$O\
	note.$O\
	proctab.$O\
	ref.$O\
	rendez.$O\
	sched.$O\

HFILES=\
	$(9SRC)/include/thread.h\
	label.h\
	threadimpl.h\

include $(9SRC)/Makesyslib

tprimes: tprimes.$O $(9SRC)/lib/$(LIB)
	$(LD) -o tprimes tprimes.$O $(LDFLAGS) -lthread -l9 -lfmt -lutf

texec: texec.$O $(9SRC)/lib/$(LIB)
	$(LD) -o texec texec.$O $(LDFLAGS) -lthread -l9 -lfmt -lutf

trend: trend.$O $(9SRC)/lib/$(LIB)
	$(LD) -o trend trend.$O $(LDFLAGS) -lthread -l9 -lfmt -lutf

CLEANFILES+=tprimes texec

