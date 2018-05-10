#
# FreeType 2 Borland C++-specific rules
#


# Copyright 1996-2000 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.


# Compiler command line name
#
CC := bcc32

# The object file extension (for standard and static libraries).  This can be
# .o, .tco, .obj, etc., depending on the platform.
#
O  := obj
SO := obj

# The library file extension (for standard and static libraries).  This can
# be .a, .lib, etc., depending on the platform.
#
A  := lib
SA := lib


# Path inclusion flag.  Some compilers use a different flag than `-I' to
# specify an additional include path.  Examples are `/i=' or `-J'.
#
I := -I


# C flag used to define a macro before the compilation of a given source
# object.  Usually it is `-D' like in `-DDEBUG'.
#
D := -D


# The link flag used to specify a given library file on link.  Note that
# this is only used to compile the demo programs, not the library itself.
#
L :=


# Target flag -- no trailing space.
#
T := -o


# C flags
#
#   These should concern: debug output, optimization & warnings.
#
#   Use the ANSIFLAGS variable to define the compiler flags used to enfore
#   ANSI compliance.
#
ifndef CFLAGS
  CFLAGS := -c -q -y -d -v -Od -w-par -w-ccc -w-rch -w-pro -w-aus
endif

# ANSIFLAGS: Put there the flags used to make your compiler ANSI-compliant.
#
ANSIFLAGS := -A


# Library linking
#
ifndef CLEAN_LIBRARY
  CLEAN_LIBRARY = $(DELETE) $(subst $(SEP),$(HOSTSEP),$(PROJECT_LIBRARY))
endif
TARGET_OBJECTS = $(subst $(SEP),\\,$(OBJECTS_LIST))
LINK_LIBRARY   = tlib /u $(subst $(SEP),\\,$@) $(TARGET_OBJECTS:%=+%)

# EOF
