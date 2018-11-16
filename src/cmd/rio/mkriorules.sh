if [ "x$WSYSTYPE" != xx11 ]; then
	echo 'default:V: all'
	echo
	echo 'all install clean nuke:'
	echo '	# WSYSTYPE is not x11, and rio is only for x11'
	exit 0
fi
cat $PLAN9/src/mkmany
