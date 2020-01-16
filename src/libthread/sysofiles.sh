#!/bin/sh

test -f $PLAN9/config && . $PLAN9/config

tag="$OBJTYPE-$SYSNAME-${SYSVERSION:-`uname -r`}-${CC9:-cc}"
case "$tag" in
*-NetBSD-*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o $SYSNAME.o pthread.o stkmalloc.o
	;;
*-Darwin-10.[5-6].* | *-Darwin-[89].*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o $SYSNAME-${OBJTYPE}.o pthread.o stkmalloc.o
	;;
*-OpenBSD-*)
	echo ${SYSNAME}-${OBJTYPE}-asm.o ${SYSNAME}-${OBJTYPE}.o pthread.o stkmmap.o
	;;
*)
	echo pthread.o stkmalloc.o
esac

case "$OBJTYPE-$SYSNAME" in
sparc64-Linux)
	# Debian glibc doesn't supply swapcontext, makecontext
	# so we supply our own copy from the latest glibc.
	echo Linux-sparc64-context.o Linux-sparc64-swapcontext.o
	;;
arm-Linux)
	# ARM doesn't supply them either.
	echo Linux-arm-context.o Linux-arm-swapcontext.o
	;;
x86_64-Darwin)
	echo Darwin-x86_64-asm.o Darwin-x86_64-swapcontext.o
	;;
esac

