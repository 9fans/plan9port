../bin/mk: mkmk.sh
	PATH=`pwd`/../bin:$$PATH export PATH; \
	PLAN9=`pwd`/.. export PLAN9; \
	sh -x mkmk.sh

all clean install nuke: ../bin/mk
	mk $@

