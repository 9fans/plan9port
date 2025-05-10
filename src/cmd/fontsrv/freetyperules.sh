#!/bin/sh

if [ "x$1" = "xx11" ]; then
	if [ "x$2" = "x" ]; then
		i="-I/usr/include"
	else
		i=$2
	fi
	if [ "x`uname`" = "xAIX" ]; then
		i="-I/opt/freeware/include"
		l="-L/opt/freeware/lib"
	fi
	echo 'CFLAGS=$CFLAGS '$i' '$i'/freetype2'
        echo 'LDFLAGS=$LDFLAGS '$l' -lfontconfig -lfreetype -lz'
fi

