../bin/mk: mkmk.sh
	SYSNAME=`uname` export SYSNAME; \
	OBJTYPE=`uname -m | sed 's;i.86;386;; s;amd64;x86_64;; s;/.*;;; s; ;;g; s/PowerMacintosh/power/; s/ppc64/power/g; s/ppc/power/g'` export OBJTYPE; \
	PATH=`pwd`/../bin:$$PATH export PATH; \
	PLAN9=`pwd`/.. export PLAN9; \
	sh -x mkmk.sh

all clean install nuke: ../bin/mk
	mk $@

