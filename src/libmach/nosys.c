/*
 * process non-interface
 */

#include <u.h>
#include <libc.h>
#include <mach.h>

Mach *machcpu = &mach386;

void
unmapproc(Map *m)
{
	USED(m);
}

int
mapproc(int pid, Map *m, Regs **r)
{
	USED(pid);
	USED(m);
	USED(r);
	if(r)
		*r = nil;
	werrstr("mapproc not implemented");
	return -1;
}

int
detachproc(int pid)
{
	USED(pid);
	werrstr("detachproc not implemented");
	return -1;
}

int
procnotes(int pid, char ***notes)
{
	USED(pid);
	USED(notes);
	werrstr("procnotes not implemented");
	return -1;
}

int
ctlproc(int pid, char *msg)
{
	USED(pid);
	USED(msg);
	werrstr("ctlproc not implemented");
	return -1;
}

char*
proctextfile(int pid)
{
	USED(pid);
	werrstr("proctextfile not implemented");
	return nil;
}
