#!/bin/sh

[ -f $PLAN9/config ] && . $PLAN9/config

if [ "x$X11" = "x" ]; then 
	if [ -d /usr/X11R6 ]; then
		X11=/usr/X11R6
	elif [ -d /usr/local/X11R6 ]; then
		X11=/usr/local/X11R6
	elif [ -d /usr/X ]; then
		X11=/usr/X
	elif [ -d /usr/openwin ]; then	# for Sun
		X11=/usr/openwin
	else
		X11=noX11dir
	fi
fi

if [ "x$WSYSTYPE" = "x" ]; then
	if [ -d "$X11" ]; then
		WSYSTYPE=x11
	else
		WSYSTYPE=nowsys
	fi
fi

if [ "x$WSYSTYPE" = "xx11" -a "x$X11H" = "x" ]; then
	if [ -d "$X11/include" ]; then
		X11H="-I$X11/include"
	else
		X11H=""
	fi
fi
	

echo 'WSYSTYPE='$WSYSTYPE
echo 'X11='$X11

if [ $WSYSTYPE = x11 ]; then
	echo 'CFLAGS=$CFLAGS '$X11H
	echo 'HFILES=$HFILES $XHFILES'
	XO=`ls x11-*.c 2>/dev/null | sed 's/\.c$/.o/'`
	echo 'WSYSOFILES=$WSYSOFILES '$XO
fi
if [ $WSYSTYPE = nowsys ]; then
	echo 'WSYSOFILES=nowsys.o'
fi
