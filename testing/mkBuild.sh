#! /bin/sh
set -eu

cd "$PLAN9/src"

. ../buildEnvironmentVariables.sh

(
	for i in lib9 lib9p lib9pclient libacme libauth libauthsrv libavl libbin libbio libcomplete libdisk libdiskfs libdraw libflate libframe libgeometry libhtml libhttpd libip libmach libmemdraw libmemlayer libmp libmux libndb libplumb libregexp libsec libString libsunrpc libthread libventi cmd; do
		printf '%s\n' "cd $i"
		(cd "$i"; mk -a -n all)
		printf 'cd ..\n\n'
	done
) | sed '
	s/'"$SYSNAME"'/$SYSNAME/g
	s/'"$OBJTYPE"'/$OBJTYPE/g
	s;'"$PLAN9"';$PLAN9;g
	s/9[ac] *getcallerpc-.*/9c getcallerpc-$OBJTYPE.c || 9a getcallerpc-$OBJTYPE.s/
	s/^9[ac] *tas-.*/9a tas-$OBJTYPE.s || 9c tas-$OBJTYPE.c/
'
