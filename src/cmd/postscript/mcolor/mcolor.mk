#
# Not installing the man page.
#

MAKE=/bin/make
MAKEFILE=mcolor.mk

SYSTEM=V9
VERSION=3.3.2

OWNER=bin
GROUP=bin

POSTLIB=/usr/lib/postscript
TMACDIR=/usr/lib/tmac

all :
	@cp color.sr tmac.color

install : all
	@if [ ! -d $(TMACDIR) ]; then \
	    mkdir $(TMACDIR); \
	    chmod 755 $(TMACDIR); \
	    chgrp $(GROUP) $(TMACDIR); \
	    chown $(OWNER) $(TMACDIR); \
	fi
	cp tmac.color $(TMACDIR)/tmac.color
	@chmod 644 $(TMACDIR)/tmac.color
	@chgrp $(GROUP) $(TMACDIR)/tmac.color
	@chown $(OWNER) $(TMACDIR)/tmac.color

clean :
	rm -f tmac.color

clobber : clean

changes :
	@trap "" 1 2 3 15; \
	sed \
	    -e "s'^SYSTEM=.*'SYSTEM=$(SYSTEM)'" \
	    -e "s'^VERSION=.*'VERSION=$(VERSION)'" \
	    -e "s'^GROUP=.*'GROUP=$(GROUP)'" \
	    -e "s'^OWNER=.*'OWNER=$(OWNER)'" \
	    -e "s'^POSTLIB=.*'POSTLIB=$(POSTLIB)'" \
	    -e "s'^TMACDIR=.*'TMACDIR=$(TMACDIR)'" \
	$(MAKEFILE) >X$(MAKEFILE); \
	mv X$(MAKEFILE) $(MAKEFILE); \
	sed \
	    -e "s'^.ds dP.*'.ds dP $(POSTLIB)'" \
	    -e "s'^.ds dT.*'.ds dT $(TMACDIR)'" \
	mcolor.5 >Xmcolor.5; \
	mv Xmcolor.5 mcolor.5

