#!/bin/sh
PATH=/bin:/usr/bin
message=${1-/usr/lib/upas/gone.msg}
return=`sed '2,$s/^From[ 	]/>&/'|tee -a $HOME/gone.mail|sed -n '1s/^From[ 	]\([^ 	]*\)[ 	].*$/\1/p'`
echo '' >>$HOME/gone.mail
grep "^$return" $HOME/gone.addrs >/dev/null 2>/dev/null || {
	echo $return >>$HOME/gone.addrs
	mail $return < $message
}
