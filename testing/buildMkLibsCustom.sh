#! /bin/sh
set -eu
: "$WORK $CC $CFLAGS_REAL"

# Fail if not given enough arguments.
case "$#" in
[0123])
	printf 'buildMkLibsCustom.sh: not enough arguments\n' 1>&2
	exit 1
	;;
esac

# Set environment variables that are sometimes used by mk for choosing translation units.
. "$1"

# Directory whose path will be prepended to mk's PATH.
inPath="$2"

# Output library.
outLib="$3"

shift 3

export CC_REAL
CC_REAL="$CC"
objects=
targets=
export TARGET
for target in "$@"; do
	printf '\n ########### Building %s\n\n' "$target"
	TARGET="$WORK/`basename "$target"`"
	test -d "$TARGET" || mkdir "$TARGET"
	cd "$target"
	PATH="$inPath:$PATH" mk -a all
	objects="$objects `echo "$TARGET/"*.o`"
	targets="$targets $TARGET"
done

test -e "$outLib" && rm "$outLib"
ar rcs "$outLib" $objects
rm -r $targets
