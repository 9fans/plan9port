LIB=/usr/lib/upas
CFLAGS=${UNIX} -g -I. -I../libc -I../common -I/usr/include -I/usr/include/sys
LFLAGS=-g
HOSTNAME=cat /etc/whoami

.c.o: ; $(CC) -c $(CFLAGS) $*.c
all: mail

sedfile:
	echo 's+LIBDIR+$(LIB)+g' >sed.file
	echo 's+HOSTNAME+$(HOSTNAME)+g' >>sed.file

install: sedfile install.fish install.mail.sh

install.fish:
	cp gone.msg $(LIB)
	sed -f sed.file gone.fishing >$(LIB)/gone.fishing
	-chmod 775 $(LIB)/gone.fishing
	-chown bin $(LIB)/gone.fishing $(LIB)/gone.msg

install.mail.sh:
	sed -f sed.file mail.sh >/bin/mail
	-chown bin /bin/mail
	-chmod 775 /bin/mail

install.notify: notify
	cp notify $(LIB)/notify
	-chmod 775 $(LIB)/notify
	-chown bin $(LIB)/notify

install.mail: mail
	cp mail /bin
	strip /bin/mail

notify: notify.o
	cc $(LFLAGS) notify.o -o notify

mail: mail.o ../config/config.o
	cc $(LFLAGS) mail.o ../config/config.o -o mail

clean:
	-rm -f *.[oOa] core a.out *.sL notify
	-rm -f sed.file mail

