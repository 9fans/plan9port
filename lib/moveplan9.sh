#!/bin/sh

p=`cleanname $PLAN9`
cd $PLAN9
echo '
	X ,s;/usr/local/plan9;$p;g
	X/'/w
	q
' | sam -d `cat lib/moveplan9.files` man/man*/*.* # >/dev/null 2>&1

