#!/bin/sh
case $1 in
-n)
	exec LIBDIR/notify
	exit $? ;;
-m*|-f*|-r*|-p*|-e*|"")
	exec LIBDIR/edmail $*
	exit $? ;;
*)
	exec LIBDIR/send $*
	exit $? ;;
esac
