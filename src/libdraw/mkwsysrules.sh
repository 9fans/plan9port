#!/bin/sh

[ -f $PLAN9/config ] && . $PLAN9/config

if [ "x$X11" = "x" ]; then 
	if [ -d /usr/X11R6 ]; then
		X11=/usr/X11R6
	elif [ -d /usr/local/X11R6 ]; then
		X11=/usr/local/X11R6
	else
		X11=noX11dir
	fi
fi

if [ "x$WSYSTYPE" = "x" ]; then
	if [ -d "$X11/include" ]; then
		WSYSTYPE=x11
	else
		WSYSTYPE=nowsys
	fi
fi

echo 'WSYSTYPE='$WSYSTYPE
echo 'X11='$X11

if [ $WSYSTYPE = x11 ]; then
	echo 'CFLAGS=$CFLAGS -I$X11/include'
	echo 'HFILES=$HFILES $XHFILES'
fi


