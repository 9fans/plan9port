#!/bin/sh

OBJTYPE=${OBJTYPE:-`uname -m | sed 's/Power Macintosh/power/;s/i.86/386/'`}
SYSNAME=${SYSNAME:-`uname`}
tag="$OBJTYPE-$SYSNAME-`uname -r`-${CC9:-cc}"
case "$tag" in
*-Linux-2.6.*)
	echo pthread.o
	;;
*-FreeBSD-5.*)
	echo pthread.o
	;;
*-Linux-*)
	# will have to fix this for linux power pc
	echo $SYSNAME.o ${SYSNAME}asm.o
	;;
*-FreeBSD-*)
	echo $SYSNAME.o ${SYSNAME}asm.o
	;;
*-Darwin-*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o ${SYSNAME}-${OBJTYPE}.o pthread.o
*)
	echo pthread.o
esac

