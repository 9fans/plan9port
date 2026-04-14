#!/bin/sh

[ -f $PLAN9/config ] && . $PLAN9/config

if [ "x$X11" = "x" ]; then 
	if [ -d /usr/X11R6 ]; then
		X11=/usr/X11R6
	elif [ -d /usr/local/X11R6 ]; then
		X11=/usr/local/X11R6
	elif [ -d /usr/X11R7 ]; then
		X11=/usr/X11R7
	elif [ -d /usr/X ]; then
		X11=/usr/X
	elif [ -d /usr/openwin ]; then	# for Sun
		X11=/usr/openwin
	elif [ -d /usr/include/X11 ]; then
		X11=/usr
	elif [ -d /usr/local/include/X11 ]; then
		X11=/usr/local
	else
		X11=noX11dir
	fi
fi

if [ "x$WSYSTYPE" = "x" ]; then
	if [ "x`uname`" = "xDarwin" ]; then
		if sw_vers | $egrep 'ProductVersion:	(10\.[0-9]\.|10\.1[012])$' >/dev/null; then
			echo 1>&2 'OS X 10.12 and older are not supported'
			exit 1
		fi
		WSYSTYPE=mac
	elif sdl3-config --version >/dev/null 2>&1 || pkg-config --exists sdl3 2>/dev/null; then
		WSYSTYPE=sdl3
	elif [ -d "$X11" ]; then
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
echo 'X11H='$X11H

if [ $WSYSTYPE = sdl3 ]; then
	SDL3_CFLAGS=`pkg-config --cflags sdl3 2>/dev/null || sdl3-config --cflags 2>/dev/null`
	SDL3_LDFLAGS=`pkg-config --libs sdl3 2>/dev/null || sdl3-config --libs 2>/dev/null`
	echo 'CFLAGS=$CFLAGS '"$SDL3_CFLAGS"
	echo 'LDFLAGS=$LDFLAGS '"$SDL3_LDFLAGS"
	echo 'WSYSOFILES=$WSYSOFILES sdl3-draw.o sdl3-screen.o'
	echo 'WSYSHFILES='
elif [ $WSYSTYPE = x11 ]; then
	echo 'CFLAGS=$CFLAGS '$X11H
	echo 'HFILES=$HFILES $XHFILES'
	XO=`ls x11-*.c 2>/dev/null | sed 's/\.c$/.o/'`
	echo 'WSYSOFILES=$WSYSOFILES '$XO
	echo 'WSYSHFILES=x11-inc.h x11-keysym2ucs.h x11-memdraw.h'
elif [ $WSYSTYPE = mac ]; then
	echo 'WSYSOFILES=$WSYSOFILES mac-draw.o mac-screen.o'
	echo 'WSYSHFILES='
	echo 'MACARGV=macargv.o'
elif [ $WSYSTYPE = nowsys ]; then
	echo 'WSYSOFILES=nowsys.o'
fi
