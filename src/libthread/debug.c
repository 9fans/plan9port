#include "threadimpl.h"

int _threaddebuglevel;

void
__threaddebug(ulong flag, char *fmt, ...)
{
	char buf[128];
	va_list arg;
	Fmt f;
	Proc *p;

	if((_threaddebuglevel&flag) == 0)
		return;

	fmtfdinit(&f, 2, buf, sizeof buf);

	p = _threadgetproc();
	if(p==nil)
		fmtprint(&f, "noproc ");
	else if(p->thread)
		fmtprint(&f, "%d.%d ", p->id, p->thread->id);
	else
		fmtprint(&f, "%d._ ", p->id);

	va_start(arg, fmt);
	fmtvprint(&f, fmt, arg);
	va_end(arg);
	fmtprint(&f, "\n");
	fmtfdflush(&f);
}

void
_threadassert(char *s)
{
	char buf[256];
	int n;
	Proc *p;

	p = _threadgetproc();
	if(p && p->thread)
		n = sprint(buf, "%d.%d ", p->pid, p->thread->id);
	else
		n = 0;
	snprint(buf+n, sizeof(buf)-n, "%s: assertion failed\n", s);
	write(2, buf, strlen(buf));
	abort();
}
