#!/bin/sh

if [ `uname` = Linux ]
then
	case "`uname | awk '{print $3}'`" in
	*)
		echo Linux-clone
		;;
	2.[6789]*)
		echo pthread
		;;
	esac
else
	echo pthread
fi
