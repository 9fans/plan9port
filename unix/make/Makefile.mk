TARG=mk
VERSION=2.0
PORTPLACE=devel/mk
NAME=mk
CLEANFILES=mk mk.1a

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
	rc.$O\
	rule.$O\
	run.$O\
	sh.$O\
	shell.$O\
	shprint.$O\
	symtab.$O\
	var.$O\
	varsub.$O\
	word.$O\
	unix.$O\

HFILES=\
	mk.h\
	fns.h\

all: $(TARG)

TGZFILES+=mk.pdf

install: $(TARG)
	mkdir -p $(PREFIX)/bin
	install -m 0755 mk $(PREFIX)/bin/mk
	cat mk.1 | sed 's;DOCPREFIX;$(PREFIX);g' >mk.1a
	mkdir -p $(PREFIX)/man/man1
	install -m 0644 mk.1a $(PREFIX)/man/man1/mk.1
	mkdir -p $(PREFIX)/doc
	install -m 0644 mk.pdf $(PREFIX)/doc/mk.pdf

