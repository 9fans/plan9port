#!/bin/sh

tag="${OBJTYPE:-`uname -m`}-${SYSNAME:-`uname`}-`uname -r`-${CC9:-cc}"
case "$tag" in
*-Linux-2.6.*)
	echo pthread.o
	;;
*-FreeBSD-5.*)
	echo pthread.o
	;;
*-Linux-*)
	echo `uname`.o `uname`asm.o
	;;
*-FreeBSD-*)
	echo `uname`.o `uname`asm.o
	;;
*)
	echo pthread.o
esac

