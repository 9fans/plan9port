
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

LIB=libframe.a
VERSION=2.0
PORTPLACE=devel/libframe
NAME=libdraw

OFILES=\
	frbox.$O\
	frdelete.$O\
	frdraw.$O\
	frinit.$O\
	frinsert.$O\
	frptofchar.$O\
	frselect.$O\
	frstr.$O\
	frutil.$O\

all: $(LIB)

install: $(LIB)
	install -c -m 0644 $(LIB) $(PREFIX)/lib/$(LIB)
	install -c -m 0644 frame.h $(PREFIX)/include/frame.h
$(LIB): $(OFILES)
	$(AR) $(ARFLAGS) $(LIB) $(OFILES)

NUKEFILES+=$(LIB)
.c.$O:
	$(CC) $(CFLAGS) -I/usr/X11R6/include -I$(PREFIX)/include $*.c

%.$O: %.c
	$(CC) $(CFLAGS) -I/usr/X11R6/include -I$(PREFIX)/include $*.c


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
