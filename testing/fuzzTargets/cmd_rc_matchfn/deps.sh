# Sourced from buildFuzz.sh

targetDeps="`distributiveConcat "$PLAN9/src/cmd/rc/" code.c exec.c getflags.c glob.c here.c io.c lex.c pcmd.c pfnc.c simple.c subr.c trap.c tree.c unixcrap.c var.c y.tab.c plan9ish.c havefork.c`"
