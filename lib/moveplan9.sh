#!/bin/sh

p=`cleanname $PLAN9`
cd $PLAN9
echo "
	X ,s;/usr/local/plan9($|/|});$p\\1;g
	X/'/w
	q
" | sam -d `cat lib/moveplan9.files` >/dev/null 2>&1

