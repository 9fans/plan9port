/*
 * process interface for FreeBSD 
 */

#include <u.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <libc.h>
#include <mach.h>
#include "ureg386.h"

void
unmapproc(Map*)
{
}

int
mapproc(int, Map*, Regs**)
{
}

int
detachproc(int)
{
}

int
procnotes(int, char***)
{
}

int
ctlproc(int, char*)
{
}

char*
proctextfile(int)
{
}
