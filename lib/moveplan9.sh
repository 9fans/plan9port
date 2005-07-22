#!/bin/sh

p=`cleanname $PLAN9`
if [ X"$p" = X"" ]
then
	echo cleanname failed 1>&2
	exit 1
fi

cd $PLAN9
echo "
	X ,s;/usr/local/plan9($|/|});$p\\1;g
	X/'/w
	q
" | sam -d `cat lib/moveplan9.files` >/dev/null 2>&1

