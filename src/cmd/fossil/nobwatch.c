#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

void
bwatchReset(uchar score[VtScoreSize])
{
	USED(score);
}

void
bwatchInit(void)
{
}

void
bwatchSetBlockSize(uint i)
{
	USED(i);
}

void
bwatchDependency(Block *b)
{
	USED(b);
}

void
bwatchLock(Block *b)
{
	USED(b);
}

void
bwatchUnlock(Block *b)
{
	USED(b);
}
