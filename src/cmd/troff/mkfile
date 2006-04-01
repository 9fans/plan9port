<$PLAN9/src/mkhdr

TARG=troff
OFILES=n1.$O\
	n2.$O\
	n3.$O\
	n4.$O\
	n5.$O\
	t6.$O\
	n6.$O\
	n7.$O\
	n8.$O\
	n9.$O\
	t10.$O\
	n10.$O\
	t11.$O\
	ni.$O\
	hytab.$O\
	suftab.$O\
	dwbinit.$O\
	mbwc.$O

HFILES=tdef.h\
	fns.h\
	ext.h\
	dwbinit.h\


<$PLAN9/src/mkone
CFLAGS=-DUNICODE

TMACDIR='"tmac/tmac."'
FONTDIR='"troff/font"'
NTERMDIR='"troff/term/tab."'
ALTHYPHENS='"lib/hyphen.tex"'
TEXHYPHENS='"#9/lib/hyphen.tex"'
DWBHOME='"#9/"'
TDEVNAME='"utf"'
NDEVNAME='"utf"'

ni.$O:	ni.c $HFILES
	$CC $CFLAGS -DTMACDIR=$TMACDIR ni.c

t10.$O:	t10.c $HFILES
	$CC $CFLAGS -DTDEVNAME=$TDEVNAME t10.c

n1.$O:	n1.c $HFILES
	$CC $CFLAGS -DFONTDIR=$FONTDIR -DNTERMDIR=$NTERMDIR -DTEXHYPHENS=$TEXHYPHENS -DALTHYPHENS=$ALTHYPHENS -DDWBHOME=$DWBHOME n1.c

n10.$O:	n10.c $HFILES
	$CC $CFLAGS -DTDEVNAME=$NDEVNAME n10.c

n8.$O:	n8.c $HFILES
	$CC $CFLAGS -DTEXHYPHENS=$TEXHYPHENS n8.c

dwbinit.$O:	dwbinit.c
	$CC $CFLAGS -DDWBHOME=$DWBHOME dwbinit.c
