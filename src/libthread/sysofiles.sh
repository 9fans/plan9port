#!/bin/sh

test -f $PLAN9/config && . $PLAN9/config

case "$SYSNAME" in
NetBSD)
	echo ${SYSNAME}-${OBJTYPE}-asm.o $SYSNAME.o stkmalloc.o
	;;
OpenBSD)
	echo pthread.o stkmmap.o
	;;
*)
	echo pthread.o stkmalloc.o
esac

# Various libc don't supply swapcontext, makecontext, so we do.
case "$SYSNAME-$OBJTYPE" in
Darwin-x86_64 | Linux-arm | Linux-sparc64 | NetBSD-arm | OpenBSD-386 | OpenBSD-power | OpenBSD-x86_64)
	echo $OBJTYPE-ucontext.o
	;;
esac

# A few libc don't supply setcontext, getcontext, so we do.
case "$SYSNAME-$OBJTYPE" in
Darwin-x86_64 | Linux-arm | Linux-sparc64 | OpenBSD-386 | OpenBSD-power | OpenBSD-x86_64)
	echo $SYSNAME-$OBJTYPE-asm.o
	;;
esac
