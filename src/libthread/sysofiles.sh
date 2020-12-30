#!/bin/sh

test -f $PLAN9/config && . $PLAN9/config

echo pthread.o

case "$SYSNAME" in
OpenBSD)
	echo stkmmap.o
	;;
*)
	echo stkmalloc.o
esac

# Various libc don't supply swapcontext, makecontext, so we do.
case "$SYSNAME-$OBJTYPE" in
Linux-arm | Linux-sparc64 | NetBSD-arm | OpenBSD-386 | OpenBSD-power | OpenBSD-x86_64)
	echo $OBJTYPE-ucontext.o
	;;
esac

# A few libc don't supply setcontext, getcontext, so we do.
case "$SYSNAME-$OBJTYPE" in
Linux-arm | Linux-sparc64 | OpenBSD-386 | OpenBSD-power | OpenBSD-x86_64)
	echo $SYSNAME-$OBJTYPE-asm.o
	;;
esac
