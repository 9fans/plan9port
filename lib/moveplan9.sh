#!/bin/sh

case $# in
0)
	old=/usr/local/plan9
	;;
1)
	old=`cleanname $1`
	;;
*)
	echo 'usage: moveplan9.sh [oldpath]' 1>&2
	exit 1
esac

new=`cleanname $PLAN9_TARGET`

if [ X"$new" = X"" ]
then
	echo cleanname failed 1>&2
	exit 2
fi

cd $PLAN9
# Avoid broken builtin echo in dash that turns \1 into ^A
`which echo` '
	X ,s;'$old'($|/|});'$new'\1;g
	X/'"'"'/w
	q
' | sam -d `cat lib/moveplan9.files` >/dev/null 2>&1

