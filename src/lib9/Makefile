
# this works in gnu make
SYSNAME:=${shell uname}
OBJTYPE:=${shell uname -m | sed 's;i.86;386;; s;/.*;;; s; ;;g'}

# this works in bsd make
SYSNAME!=uname
OBJTYPE!=uname -m | sed 's;i.86;386;; s;/.*;;; s; ;;g'

# the gnu rules will mess up bsd but not vice versa,
# hence the gnu rules come first.

include Make.$(SYSNAME)-$(OBJTYPE)

PREFIX=/usr/local

NUKEFILES=

TGZFILES=

LIB=lib9.a
VERSION=2.0
PORTPLACE=devel/lib9
NAME=lib9

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
	rendez.$O\
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
	lib9.h\

all: $(LIB)

install: $(LIB)
	test -d $(PREFIX)/man/man3 || mkdir $(PREFIX)/man/man3
	# install -m 0644 lib9.3 $(PREFIX)/man/man3/lib9.3
	install -m 0644 lib9.h $(PREFIX)/include/lib9.h
	install -m 0644 $(LIB) $(PREFIX)/lib/$(LIB)

test: $(LIB) test.$O
	$(CC) -o test test.$O $(LIB) -L$(PREFIX)/lib -lfmt -lutf

testfork: $(LIB) testfork.$O
	$(CC) -o testfork testfork.$O $(LIB) -L$(PREFIX)/lib -lfmt -lutf

$(LIB): $(OFILES)
	$(AR) $(ARFLAGS) $(LIB) $(OFILES)

NUKEFILES+=$(LIB)
.c.$O:
	$(CC) $(CFLAGS) -I$(PREFIX)/include $*.c

%.$O: %.c
	$(CC) $(CFLAGS) -I$(PREFIX)/include $*.c


$(OFILES): $(HFILES)

tgz:
	rm -rf $(NAME)-$(VERSION)
	mkdir $(NAME)-$(VERSION)
	cp Makefile Make.* README LICENSE NOTICE *.[ch137] rpm.spec bundle.ports $(TGZFILES) $(NAME)-$(VERSION)
	tar cf - $(NAME)-$(VERSION) | gzip >$(NAME)-$(VERSION).tgz
	rm -rf $(NAME)-$(VERSION)

clean:
	rm -f $(OFILES) $(LIB)

nuke:
	rm -f $(OFILES) *.tgz *.rpm $(NUKEFILES)

rpm:
	make tgz
	cp $(NAME)-$(VERSION).tgz /usr/src/RPM/SOURCES
	rpm -ba rpm.spec
	cp /usr/src/RPM/SRPMS/$(NAME)-$(VERSION)-1.src.rpm .
	cp /usr/src/RPM/RPMS/i586/$(NAME)-$(VERSION)-1.i586.rpm .
	scp *.rpm rsc@amsterdam.lcs.mit.edu:public_html/software

PORTDIR=/usr/ports/$(PORTPLACE)

ports:
	make tgz
	rm -rf $(PORTDIR)
	mkdir $(PORTDIR)
	cp $(NAME)-$(VERSION).tgz /usr/ports/distfiles
	cat bundle.ports | (cd $(PORTDIR) && awk '$$1=="---" && $$3=="---" { ofile=$$2; next} {if(ofile) print >ofile}')
	(cd $(PORTDIR); make makesum)
	(cd $(PORTDIR); make)
	(cd $(PORTDIR); /usr/local/bin/portlint)
	rm -rf $(PORTDIR)/work
	shar `find $(PORTDIR)` > ports.shar
	(cd $(PORTDIR); tar cf - *) | gzip >$(NAME)-$(VERSION)-ports.tgz
	scp *.tgz rsc@amsterdam.lcs.mit.edu:public_html/software

.phony: all clean nuke install tgz rpm ports
