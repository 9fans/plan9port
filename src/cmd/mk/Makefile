9SRC=..
include $(9SRC)/Makehdr

TARG=mk

OFILES=\
	arc.$O\
	archive.$O\
	bufblock.$O\
	env.$O\
	file.$O\
	graph.$O\
	job.$O\
	lex.$O\
	main.$O\
	match.$O\
	mk.$O\
	parse.$O\
	recipe.$O\
	rule.$O\
	run.$O\
	sh.$O\
	shprint.$O\
	symtab.$O\
	var.$O\
	varsub.$O\
	word.$O\
	unix.$O\

HFILES=\
	mk.h\
	fns.h\

LDFLAGS+=-lregexp9 -lbio -lfmt -lutf

include $(9SRC)/Makeone

