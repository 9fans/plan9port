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
*-Darwin-[6-8].*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o ${SYSNAME}-${OBJTYPE}.o pthread.o
	;;
*-Darwin-*)
	echo pthread.o
	;;
*-OpenBSD-*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o ${SYSNAME}-${OBJTYPE}.o $SYSNAME.o
	;;
*)
	echo pthread.o
esac

case "$OBJTYPE-$SYSNAME" in
sparc64-Linux)
	# Debian glibc doesn't supply swapcontext, makecontext
	# so we supply our own copy from the latest glibc.
	echo Linux-sparc64-context.o Linux-sparc64-swapcontext.o
	;;
esac

