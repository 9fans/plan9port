../bin/mk: mkmk.sh
	PATH=`pwd`/../bin:$PATH export PATH
	sh -x mkmk.sh

all clean install nuke: ../bin/mk
	mk $@

