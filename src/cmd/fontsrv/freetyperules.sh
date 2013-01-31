#!/bin/sh

if [ "x$1" = "xx11" ]; then
	if [ "x$2" = "x" ]; then
		i="-I/usr/include"
	else
		i=$2
	fi
	echo 'CFLAGS=$CFLAGS '$i'/freetype2' 
        echo 'LDFLAGS=$LDFLAGS -lfontconfig -lfreetype -lz'
fi

