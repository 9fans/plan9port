
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

LIB=libbio.a
VERSION=2.0
PORTPLACE=devel/libbio
NAME=libbio

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

all: $(LIB)

install: $(LIB)
	test -d $(PREFIX)/man/man3 || mkdir $(PREFIX)/man/man3
	install -m 0644 bio.3 $(PREFIX)/man/man3/bio.3
	install -m 0644 bio.h $(PREFIX)/include
	install -m 0644 $(LIB) $(PREFIX)/lib/$(LIB)

bcat: bcat.$O $(LIB)
	$(CC) -o bcat bcat.$O $(LIB) -L/usr/local/lib -lfmt -lutf

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
