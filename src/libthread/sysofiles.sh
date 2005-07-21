#!/bin/sh

test -f $PLAN9/config && . $PLAN9/config

tag="$OBJTYPE-$SYSNAME-${SYSVERSION:-`uname -r`}-${CC9:-cc}"
case "$tag" in
*-Linux-2.6.*)
	echo pthread.o
	;;
*-FreeBSD-[5-9].*)
	echo pthread.o
	;;
*-Linux-*)
	# will have to fix this for linux power pc
	echo ${SYSNAME}-${OBJTYPE}-asm.o $SYSNAME.o
	;;
*-FreeBSD-*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o $SYSNAME.o
	;;
*-NetBSD-*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o $SYSNAME.o
	;;
*-Darwin-*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o ${SYSNAME}-${OBJTYPE}.o pthread.o
	;;
*-OpenBSD-*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o ${SYSNAME}-${OBJTYPE}.o $SYSNAME.o
	;;
*)
	echo pthread.o
esac

