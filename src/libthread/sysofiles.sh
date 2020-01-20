#!/bin/sh

test -f $PLAN9/config && . $PLAN9/config

case "$SYSNAME" in
NetBSD)
	echo ${SYSNAME}-${OBJTYPE}-asm.o $SYSNAME.o stkmalloc.o
	;;
OpenBSD)
	echo ${SYSNAME}-${OBJTYPE}-asm.o pthread.o stkmmap.o
	;;
*)
	echo pthread.o stkmalloc.o
esac

# Various libc don't supply swapcontext, makecontext, so we do.
case "$OBJTYPE-$SYSNAME" in
386-OpenBSD)
	echo 386-ucontext.o
	;;
arm-Linux)
	echo arm-ucontext.o
	echo Linux-arm-context.o # setcontext, getcontext
	;;
arm-NetBSD)
	echo arm-ucontext.o
	;;
power-OpenBSD)
	echo power-ucontext.o
	;;
sparc64-Linux)
	echo sparc64-ucontext.o
	echo Linux-sparc64-swapcontext.o # setcontext, getcontext
	;;
x86_64-Darwin)
	echo x86_64-ucontext.o
	echo Darwin-x86_64-asm.o # setcontext, getcontext
	;;
x86_64-OpenBSD)
	echo x86_64-ucontext.o
	;;
esac

