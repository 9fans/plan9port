#! /bin/sh
set -eu
: "$OUT $PLAN9 $WORK $LIB_FUZZING_ENGINE $CC $CXX $CFLAGS $CXXFLAGS"

# NOTE: If the build (this script) breaks/becomes outdated, update or expand it using the build log created by mkBuild.sh .
#
# Quick starting guide: Do the following, for example, before running this script by hand:
#
#	# Set environment variables.
#	export OUT PLAN9 WORK LIB_FUZZING_ENGINE CC CXX CFLAGS CXXFLAGS
#	OUT=/tmp/fuzzersOut
#	PLAN9="$HOME/plan9port"
#	WORK=/tmp/work
#	CC=clang
#	CXX=clang++
#	CFLAGS='-O1 -fno-omit-frame-pointer -g -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -fsanitize=address -fsanitize-address-use-after-scope -fsanitize=fuzzer-no-link'
#	CXXFLAGS="$CFLAGS"
#	LIB_FUZZING_ENGINE=-fsanitize=fuzzer
#	
#	# Run this script to build the fuzzers.
#	./buildFuzz.sh
#	
#	# Run a fuzzer called someFuzzer.
#	cd "$OUT"
#	./someFuzzer optionalCorpusDir

# Check if we are running as part of OSS-Fuzz. We need this to disable some fuzzing targets that the OSS-Fuzz people would not appreciate because of their diminutive size. When implementations of targets that are disabled for OSS-Fuzz change, they should definitely be fuzzed again by somebody, because OSS-Fuzz is not going to do it.
oSSFuzz=
case "$#" in
0)
	;;
*)
	case "$1" in
	oss-fuzz)
		oSSFuzz=yes
	esac
	;;
esac

printf 'CC = '
"$CC" -v 2>&1 | grep -v '^Configured with:' | grep version
printf 'CXX = '
"$CXX" -v 2>&1 | grep -v '^Configured with:' | grep version

# Set default IFS, just for ensuring sanity.
IFS=' 	
'

distributiveConcat() {
	distributiveConcat_left="$1"
	shift
	for distributiveConcat_right in "$@"; do
		printf '%s%s ' "$distributiveConcat_left" "$distributiveConcat_right"
	done
}

mkdirCD() {
	printf '%s\n' "$1"
	test -d "$1" || mkdir "$1"
	cd "$1"
}

# Arguments for $CC passed after $CFLAGS. The "-f" arguments are for
# enabling nonstandard features that plan9port C code uses. Ideally
# building with INSTALL would also use those flags. Even more ideally
# those flag arguments would not be needed.
cflags="-I $PLAN9/include -D PLAN9PORT -fsigned-char -fno-strict-aliasing -fno-strict-overflow -Wno-parentheses -Wno-comment"

# Environment variables sometimes used for choosing translation units.
. "$PLAN9/buildEnvironmentVariables.sh"

test -e "$WORK" && rm -r "$WORK" 2>/dev/null || true
test -d "$WORK" || mkdir "$WORK"
test -d "$WORK/zip" || mkdir "$WORK/zip"
test -d "$OUT" || mkdir "$OUT"

# Build mk so we could build the libraries. Afterwards we can build (parts of) mk with user provided CFLAGS if necessary (if we want to test mk).
printf 'Building mk so we could use it to build the plan9port libraries.\n'
backupPath="$PATH"
PATH="$PLAN9/bin:$PATH"
cd "$PLAN9/src"
../dist/buildmk

export CFLAGS_REAL
CFLAGS_REAL="$CFLAGS $cflags"

# Build the selected plan9port libraries.
#
# Expects WORK, CC, and CFLAGS_REAL to be set; and mk to be in the executable search path.
printf '\nBuild the selected plan9port libraries.\n'
"$PLAN9/testing/buildMkLibsCustom.sh" "$PLAN9/buildEnvironmentVariables.sh" "$PLAN9/testing/buildMkLibsCustom_inPath" "$WORK/libplan9port.a" `distributiveConcat "$PLAN9/src/" lib9 libauth libauthsrv libavl libbin libbio libdisk libdiskfs libflate libhtml libhttpd libip libmach libplumb libregexp`

cd "$PLAN9/src/cmd"

"$CC" $CFLAGS_REAL -o "$PLAN9/bin/yacc" yacc.c "$WORK/libplan9port.a"

cd rc
yacc -d syn.y
cp y.tab.h x.tab.h
cd ..

stricterWarns='-Wall -Wextra -Wno-unused-parameter -Wno-comment'
PATH="$backupPath"

printf '\nBuilding fuzz targets.\n\n'
cd "$WORK"
mkdirCD tmpArtifacts
for target in "$PLAN9/testing/fuzzTargets/"*; do
	printf '%s\n' "$target"

	targetBasename="`basename "$target"`"

	test -e "$target/linuxOnly" && case "$SYSNAME" in
	[!L]*)
		printf 'Linux-only, skipping\n'
		continue
		;;
	esac

	test -e "$target/noOSSFuzz" && case "$oSSFuzz" in
	yes)
		printf 'Disabled for OSS-Fuzz, skipping\n'
		continue
		;;
	esac

	# Deliver the possible corpus zip. We assume that any corpus directory is nonempty.
	test -e "$OUT/${targetBasename}_seed_corpus.zip" && rm "$OUT/${targetBasename}_seed_corpus.zip"
	corpora=
	if test -e "$target/corpora.sh"; then
		. "$target/corpora.sh"
	else
		corpora="$target"
	fi
	corpusFiles=
	for c in $corpora; do
		test -e "$c"
		test -d "$c/corpus" && corpusFiles="$corpusFiles `echo "$c/corpus/"*`"
	done
	for f in $corpusFiles; do
		cp "$f" "$WORK/zip"
	done
	if test -n "$corpusFiles"; then
		zip -q -j "$OUT/${targetBasename}_seed_corpus.zip" "$WORK/zip/"*
		rm "$WORK/zip/"*
	fi

	# Deliver the possible dictionary file and possible options file.
	test -e "$target/dictionary" && cp "$target/../`cat "$target/dictionary"`" "$OUT/$targetBasename.dict"
	test -f "$target/$targetBasename.dict" && cp "$target/$targetBasename.dict" "$OUT"
	test -f "$target/$targetBasename.options" && cp "$target/$targetBasename.options" "$OUT"

	# Optionally compile additional files. Usually used for "stealing" stuff from executable programs.
	targetDeps=
	test -f "$target/deps.sh" && . "$target/deps.sh" && "$CC" $CFLAGS_REAL -D PLAN9_NOMAIN -c $targetDeps
	targetDepsObjects="`echo *.o`"
	case "x$targetDepsObjects" in
	'x*.o')
		targetDepsObjects=
		;;
	esac

	printf 'Compile fuzz target\n'
	"$CC" $CFLAGS_REAL $stricterWarns -c -o "$WORK/$targetBasename.o" "$target/$targetBasename.c"

	printf 'Link, deliver fuzzer\n'
	"$CXX" $CXXFLAGS $cflags $stricterWarns -o "$OUT/$targetBasename" "$WORK/$targetBasename.o" $LIB_FUZZING_ENGINE $targetDepsObjects "$WORK/libplan9port.a"
	rm $targetDepsObjects "$WORK/$targetBasename.o"

	printf '\n'
done
