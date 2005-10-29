SSRC=	message.c main.c bind.c rewrite.c local.c dest.c process.c translate.c\
	log.c chkfwd.c notify.c gateway.c authorize.o ../common/*.c
SOBJ=	message.o main.o bind.o rewrite.o local.o dest.o process.o translate.o\
	log.o chkfwd.o notify.o gateway.o authorize.o\
	../config/config.o ../common/common.a ../libc/libc.a
SINC=	../common/mail.h ../common/string.h ../common/aux.h
CFLAGS=${UNIX} -g -I. -I../libc -I../common -I/usr/include ${SCFLAGS}
LFLAGS=-g
.c.o: ; $(CC) -c $(CFLAGS) $*.c
LIB=/usr/lib/upas

all: send

send: $(SOBJ)
	$(CC) $(SOBJ) $(LFLAGS) -o send

chkfwd.o: $(SINC) message.h dest.h
dest.o: $(SINC) dest.h
local.o: $(SINC) dest.h process.h
log.o: $(SINC) message.h
main.o: $(SINC) message.h dest.h process.h
bind.o: $(SINC) dest.h message.h
process.o: $(SINC) process.h
rewrite.o: $(SINC) dest.h
translate.o: $(SINC) dest.h process.h
message.o: $(SINC) message.h
notify.o: $(SINC) message.h
gateway.o: $(SINC) dest.h message.h

prcan:
	prcan $(SSRC)

clean:
	-rm -f send *.[oO] a.out core *.sL rmail

cyntax:
	cyntax $(CFLAGS) $(SSRC)

install: send
	rm -f $(LIB)/send /bin/rmail
	cp send $(LIB)/send
	cp send /bin/rmail
	strip /bin/rmail
	strip $(LIB)/send
	chown root $(LIB)/send /bin/rmail
	chmod 4755 $(LIB)/send /bin/rmail
