#
# FreeType 2 configuration rules templates for
# development under Unix with no configure script (gcc only)
#


# Copyright 1996-2000 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.


ifndef TOP_DIR
  TOP_DIR := .
endif
TOP_DIR := $(shell cd $(TOP_DIR); pwd)

DELETE   := rm -f
SEP      := /
HOSTSEP  := $(SEP)

# we use a special devel ftoption.h
BUILD    := $(TOP_DIR)/builds/devel

# do not set the platform to `unix', or libtool will trick you
PLATFORM := unixdev


# The directory where all object files are placed.
#
ifndef OBJ_DIR
  OBJ_DIR := $(shell cd $(TOP_DIR)/objs; pwd)
endif


# library file name
#
LIBRARY := lib$(PROJECT)


# The directory where all library files are placed.
#
# By default, this is the same as $(OBJ_DIR); however, this can be changed
# to suit particular needs.
#
LIB_DIR := $(OBJ_DIR)


#
NO_OUTPUT := 2> /dev/null

# EOF
