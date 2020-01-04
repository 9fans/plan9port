#! /bin/sh
set -eu
: "$OUT $PLAN9 $WORK $CC $CXX $CFLAGS $CXXFLAGS"

export LIB_FUZZING_ENGINE
LIB_FUZZING_ENGINE="$PLAN9/testing/StandaloneFuzzTargetMain.cc"

"$PLAN9/testing/buildFuzz.sh"
cd "$OUT"
test -d corpus || mkdir corpus
for corpusZip in *_seed_corpus.zip; do
	rm -r corpus/* 2>/dev/null || true
	printf 'Extracting zipped corpus %s\n' "$corpusZip"
	unzip "$corpusZip" -d corpus
	"./`printf '%s' "$corpusZip" | sed -E -e 's,(.*)_seed_corpus.zip,\1,'`" corpus/*
done
