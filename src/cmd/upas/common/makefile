CFLAGS=${UNIX} -g -I. -I../libc -I../common -I/usr/include ${SCFLAGS}
OBJS=mail.o aux.o string.o ${SYSOBJ}
AR=ar
.c.o: ; ${CC} -c ${CFLAGS} $*.c

common.a: ${OBJS}
	${AR} cr common.a ${OBJS}
	-ranlib common.a

aux.o:		aux.h string.h mail.h
string.o:	string.h mail.h
mail.o:		mail.h
syslog.o:	sys.h
mail.h:		sys.h

clean:
	-rm -f *.[oO] core a.out *.a *.sL common.a

