#!/bin/sh

case "`uname`-`uname -r`" in
Linux-2.[01234]*)
	echo Linux-clone.o ucontext.o
	exit 0
	;;
esac

echo pthread.o ucontext.o
exit 0
