
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

LIB=libdraw.a
VERSION=2.0
PORTPLACE=devel/libdraw
NAME=libdraw

# keyboard.$O\
# newwindow.$O\
OFILES=\
	alloc.$O\
	allocimagemix.$O\
	arith.$O\
	bezier.$O\
	border.$O\
	buildfont.$O\
	bytesperline.$O\
	chan.$O\
	cloadimage.$O\
	computil.$O\
	creadimage.$O\
	debug.$O\
	defont.$O\
	draw.$O\
	drawrepl.$O\
	egetrect.$O\
	ellipse.$O\
	emenuhit.$O\
	font.$O\
	freesubfont.$O\
	getdefont.$O\
	getrect.$O\
	getsubfont.$O\
	icossin.$O\
	icossin2.$O\
	init.$O\
	line.$O\
	loadimage.$O\
	menuhit.$O\
	mkfont.$O\
	openfont.$O\
	poly.$O\
	readcolmap.$O\
	readimage.$O\
	readsubfont.$O\
	rectclip.$O\
	replclipr.$O\
	rgb.$O\
	string.$O\
	stringbg.$O\
	stringsubfont.$O\
	stringwidth.$O\
	subfont.$O\
	subfontcache.$O\
	subfontname.$O\
	unloadimage.$O\
	window.$O\
	writecolmap.$O\
	writeimage.$O\
	writesubfont.$O\
	md-alloc.$O\
	md-arc.$O\
	md-cload.$O\
	md-cmap.$O\
	md-cread.$O\
	md-defont.$O\
	md-draw.$O\
	md-ellipse.$O\
	md-fillpoly.$O\
	md-hwdraw.$O\
	md-iprint.$O\
	md-line.$O\
	md-load.$O\
	md-openmemsubfont.$O\
	md-poly.$O\
	md-read.$O\
	md-string.$O\
	md-subfont.$O\
	md-unload.$O\
	md-write.$O\
	ml-draw.$O\
	ml-lalloc.$O\
	ml-layerop.$O\
	ml-ldelete.$O\
	ml-lhide.$O\
	ml-line.$O\
	ml-load.$O\
	ml-lorigin.$O\
	ml-lsetrefresh.$O\
	ml-ltofront.$O\
	ml-ltorear.$O\
	ml-unload.$O\
	x11-alloc.$O\
	x11-cload.$O\
	x11-draw.$O\
	x11-event.$O\
	x11-fill.$O\
	x11-get.$O\
	x11-init.$O\
	x11-itrans.$O\
	x11-keyboard.$O\
	x11-load.$O\
	x11-mouse.$O\
	x11-pixelbits.$O\
	x11-unload.$O\
	devdraw.$O\
	unix.$O\

HFILES=\
	draw.h\
	memdraw.h

all: $(LIB)

install: $(LIB)
	install -c -m 0644 $(LIB) $(PREFIX)/lib/$(LIB)
	install -c -m 0644 draw.h $(PREFIX)/include/draw.h
	install -c -m 0644 event.h $(PREFIX)/include/event.h
	install -c -m 0644 cursor.h $(PREFIX)/include/cursor.h
	install -c -m 0644 mouse.h $(PREFIX)/include/mouse.h
	install -c -m 0644 keyboard.h $(PREFIX)/include/keyboard.h

test: test.o $(LIB)
	gcc -o test test.o $(LIB) -L$(PREFIX)/lib -l9 -lfmt -lutf -L/usr/X11R6/lib -lX11 -lm

$(LIB): $(OFILES)
	$(AR) $(ARFLAGS) $(LIB) $(OFILES)

NUKEFILES+=$(LIB)
.c.$O:
	$(CC) $(CFLAGS) -I/usr/X11R6/include -I../sam -I$(PREFIX)/include $*.c

%.$O: %.c
	$(CC) $(CFLAGS) -I/usr/X11R6/include -I../sam -I$(PREFIX)/include $*.c


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
