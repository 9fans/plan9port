#!/bin/sh

if [ `uname` = Linux ]
then
	case `uname -r` in
	2.[6789]*)
		echo pthread
		;;
	*)
		echo Linux-clone
		;;
	esac
else
	echo pthread
fi
