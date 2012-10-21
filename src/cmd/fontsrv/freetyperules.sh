#!/bin/sh

if [ "x$1" = "xx11" ]; then
	echo 'CFLAGS=$CFLAGS -I/usr/include/freetype2' 
        echo 'LDFLAGS=$LDFLAGS -lfontconfig -lfreetype -lz'
fi

