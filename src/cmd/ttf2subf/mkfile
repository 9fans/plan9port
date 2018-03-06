<$PLAN9/src/mkhdr

CFLAGS=-p -Iinclude -w
LIB=libfreetype.a

TARG=ttf2subf
BIN=/$PLAN9/bin
MANPAGE=freetype

OFILES=\
	src/winfonts/winfnt.$O \
	src/type42/t42objs.$O \
	src/type42/t42parse.$O \
	src/type42/t42drivr.$O \
	src/type1/t1parse.$O \
	src/type1/t1load.$O \
	src/type1/t1driver.$O \
	src/type1/t1afm.$O \
	src/type1/t1gload.$O \
	src/type1/t1objs.$O \
	src/truetype/ttobjs.$O \
	src/truetype/ttpload.$O \
	src/truetype/ttgload.$O \
	src/truetype/ttinterp.$O \
	src/truetype/ttdriver.$O \
	src/smooth/ftgrays.$O \
	src/smooth/ftsmooth.$O \
	src/sfnt/ttload.$O \
	src/sfnt/ttcmap.$O \
	src/sfnt/ttcmap0.$O \
	src/sfnt/ttsbit.$O \
	src/sfnt/ttpost.$O \
	src/sfnt/sfobjs.$O \
	src/sfnt/sfdriver.$O \
	src/raster/ftraster.$O \
	src/raster/ftrend1.$O \
	src/psnames/psmodule.$O \
	src/pshinter/pshrec.$O \
	src/pshinter/pshglob.$O \
	src/pshinter/pshmod.$O \
	src/pshinter/pshalgo1.$O \
	src/pshinter/pshalgo2.$O \
	src/pshinter/pshalgo3.$O \
	src/psaux/psobjs.$O \
	src/psaux/t1decode.$O \
	src/psaux/t1cmap.$O \
	src/psaux/psauxmod.$O \
	src/cid/cidparse.$O \
	src/cid/cidload.$O  \
	src/cid/cidriver.$O \
	src/cid/cidgload.$O \
	src/cid/cidobjs.$O \
	src/cff/cffobjs.$O \
	src/cff/cffload.$O \
	src/cff/cffgload.$O \
	src/cff/cffparse.$O \
	src/cff/cffcmap.$O \
	src/cff/cffdrivr.$O \
	src/cache/ftlru.$O \
	src/cache/ftcmanag.$O \
	src/cache/ftccache.$O \
	src/cache/ftcglyph.$O \
	src/cache/ftcsbits.$O \
	src/cache/ftcimage.$O \
	src/cache/ftccmap.$O \
	src/bdf/bdfdrivr.$O \
	src/bdf/bdflib.$O \
	src/base/ftcalc.$O \
	src/base/fttrigon.$O \
	src/base/ftutil.$O \
	src/base/ftstream.$O \
	src/base/ftgloadr.$O \
	src/base/ftoutln.$O \
	src/base/ftobjs.$O \
	src/base/ftapi.$O \
	src/base/ftnames.$O \
	src/base/ftdbgmem.$O \
	src/base/ftglyph.$O \
	src/base/ftmm.$O \
	src/base/ftbdf.$O \
	src/base/fttype1.$O \
	src/base/ftxf86.$O \
	src/base/ftpfr.$O \
	src/base/ftstroker.$O \
	src/base/ftwinfnt.$O \
	src/base/ftbbox.$O \
	src/base/ftsystem.$O \
	src/base/ftinit.$O \
	src/base/ftdebug.$O \
	src/autohint/ahangles.$O \
	src/autohint/ahglobal.$O \
	src/autohint/ahglyph.$O \
	src/autohint/ahhint.$O \
	src/autohint/ahmodule.$O \



%.$O: %.c
	$CC -o $stem.$O -c $CFLAGS $stem.c

all:V: $LIB $TARG

$LIB:	$OFILES
#	ar vu $LIB $newprereq
#	cp $LIB /$objtype/lib

$TARG:	$LIB main.$O
	$LD -o $TARG main.$O -lfreetype

install:	$TARG
	cp $TARG $BIN/$TARG
	cp ttf2subf.1 /sys/man/1/ttf2subf

clean:
	rm -f $LIB $OFILES $TARG main.$O
