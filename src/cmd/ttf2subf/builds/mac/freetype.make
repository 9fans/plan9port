# Makefile for Apple MPW build environment (currently PPC only)

MAKEFILE     = Makefile
�MondoBuild� = #{MAKEFILE}  # Make blank to avoid rebuilds when makefile is modified
Sym�PPC      = #-sym on
ObjDir�PPC   = :obj:

CFLAGS  = -i :include -i :src -includes unix {Sym�PPC}

OBJS  = �
		"{ObjDir�PPC}ftsystem.c.x" �
		"{ObjDir�PPC}ftdebug.c.x" �
		"{ObjDir�PPC}ftinit.c.x" �
		"{ObjDir�PPC}ftbase.c.x" �
		"{ObjDir�PPC}ftglyph.c.x" �
		"{ObjDir�PPC}ftmm.c.x" �
		"{ObjDir�PPC}ftbbox.c.x" �
		"{ObjDir�PPC}autohint.c.x" �
		"{ObjDir�PPC}ftcache.c.x" �
		"{ObjDir�PPC}cff.c.x" �
		"{ObjDir�PPC}type1cid.c.x" �
		"{ObjDir�PPC}pcf.c.x" �
		"{ObjDir�PPC}psaux.c.x" �
		"{ObjDir�PPC}psmodule.c.x" �
		"{ObjDir�PPC}raster.c.x" �
		"{ObjDir�PPC}sfnt.c.x" �
		"{ObjDir�PPC}smooth.c.x" �
		"{ObjDir�PPC}truetype.c.x" �
		"{ObjDir�PPC}type1.c.x" �
		"{ObjDir�PPC}winfnt.c.x" �
		"{ObjDir�PPC}ftmac.c.x" �

# Main target - build a library
freetype �� {�MondoBuild�} directories freetype.o

# This is used to build the library
freetype.o �� {�MondoBuild�} {OBJS}
	PPCLink �
		-o :lib:{Targ} {Sym�PPC} �
		{OBJS} -c '????' -xm l

# This is used to create the directories needed for build
directories �
	if !`Exists obj` ; NewFolder obj ; end
	if !`Exists lib` ; NewFolder lib ; end


"{ObjDir�PPC}ftsystem.c.x" � {�MondoBuild�} ":src:base:ftsystem.c"
	{PPCC} ":src:base:ftsystem.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}ftdebug.c.x" � {�MondoBuild�} ":src:base:ftdebug.c"
	{PPCC} ":src:base:ftdebug.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}ftinit.c.x" � {�MondoBuild�} ":src:base:ftinit.c"
	{PPCC} ":src:base:ftinit.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}ftbase.c.x" � {�MondoBuild�} ":src:base:ftbase.c"
	{PPCC} ":src:base:ftbase.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}ftglyph.c.x" � {�MondoBuild�} ":src:base:ftglyph.c"
	{PPCC} ":src:base:ftglyph.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}ftmm.c.x" � {�MondoBuild�} ":src:base:ftmm.c"
	{PPCC} ":src:base:ftmm.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}ftbbox.c.x" � {�MondoBuild�} ":src:base:ftbbox.c"
	{PPCC} ":src:base:ftbbox.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}autohint.c.x" � {�MondoBuild�} ":src:autohint:autohint.c"
	{PPCC} ":src:autohint:autohint.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}ftcache.c.x" � {�MondoBuild�} ":src:cache:ftcache.c"
	{PPCC} ":src:cache:ftcache.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}cff.c.x" � {�MondoBuild�} ":src:cff:cff.c"
	{PPCC} ":src:cff:cff.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}type1cid.c.x" � {�MondoBuild�} ":src:cid:type1cid.c"
	{PPCC} ":src:cid:type1cid.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}pcf.c.x" � {�MondoBuild�} ":src:pcf:pcf.c"
	{PPCC} ":src:pcf:pcf.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}psaux.c.x" � {�MondoBuild�} ":src:psaux:psaux.c"
	{PPCC} ":src:psaux:psaux.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}psmodule.c.x" � {�MondoBuild�} ":src:psnames:psmodule.c"
	{PPCC} ":src:psnames:psmodule.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}raster.c.x" � {�MondoBuild�} ":src:raster:raster.c"
	{PPCC} ":src:raster:raster.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}sfnt.c.x" � {�MondoBuild�} ":src:sfnt:sfnt.c"
	{PPCC} ":src:sfnt:sfnt.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}smooth.c.x" � {�MondoBuild�} ":src:smooth:smooth.c"
	{PPCC} ":src:smooth:smooth.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}truetype.c.x" � {�MondoBuild�} ":src:truetype:truetype.c"
	{PPCC} ":src:truetype:truetype.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}type1.c.x" � {�MondoBuild�} ":src:type1:type1.c"
	{PPCC} ":src:type1:type1.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}winfnt.c.x" � {�MondoBuild�} ":src:winfonts:winfnt.c"
	{PPCC} ":src:winfonts:winfnt.c" -o {Targ} {CFLAGS}

"{ObjDir�PPC}ftmac.c.x" � {�MondoBuild�} ":src:base:ftmac.c"
	{PPCC} ":src:base:ftmac.c" -o {Targ} {CFLAGS}