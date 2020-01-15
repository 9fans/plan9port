/*
 * process interface for FreeBSD
 *
 * we could be a little more careful about not using
 * ptrace unless absolutely necessary.  this would let us
 * look at processes without stopping them.
 *
 * I'd like to make this a bit more generic (there's too much
 * duplication with Linux and presumably other systems),
 * but ptrace is too damn system-specific.
 */

#include <u.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <machine/reg.h>
#include <signal.h>
#include <errno.h>
#include <libc.h>
#include <mach.h>
#include "ureg386.h"

Mach *machcpu = &mach386;

typedef struct PtraceRegs PtraceRegs;
struct PtraceRegs
{
	Regs r;
	int pid;
};

static int ptracerw(Map*, Seg*, u64int, void*, uint, int);
static int ptraceregrw(Regs*, char*, u64int*, int);

void
unmapproc(Map *map)
{
	int i;

	if(map == nil)
		return;
	for(i=0; i<map->nseg; i++)
		while(i<map->nseg && map->seg[i].pid){
			map->nseg--;
			memmove(&map->seg[i], &map->seg[i+1],
				(map->nseg-i)*sizeof(map->seg[0]));
		}
}

int
mapproc(int pid, Map *map, Regs **rp)
{
	Seg s;
	PtraceRegs *r;

	if(ptrace(PT_ATTACH, pid, 0, 0) < 0)
	if(ptrace(PT_READ_I, pid, 0, 0)<0 && errno!=EINVAL)
	if(ptrace(PT_ATTACH, pid, 0, 0) < 0){
		werrstr("ptrace attach %d: %r", pid);
		return -1;
	}

	if(ctlproc(pid, "waitanyway") < 0){
		ptrace(PT_DETACH, pid, 0, 0);
		return -1;
	}

	memset(&s, 0, sizeof s);
	s.base = 0;
	s.size = 0xFFFFFFFF;
	s.offset = 0;
	s.name = "data";
	s.file = nil;
	s.rw = ptracerw;
	s.pid = pid;
	if(addseg(map, s) < 0)
		return -1;

	if((r = mallocz(sizeof(PtraceRegs), 1)) == nil)
		return -1;
	r->r.rw = ptraceregrw;
	r->pid = pid;
	*rp = (Regs*)r;
	return 0;
}

int
detachproc(int pid)
{
	return ptrace(PT_DETACH, pid, 0, 0);
}

static int
ptracerw(Map *map, Seg *seg, u64int addr, void *v, uint n, int isr)
{
	int i;
	u32int u;
	uchar buf[4];

	addr += seg->base;
	for(i=0; i<n; i+=4){
		if(isr){
			errno = 0;
			u = ptrace(PT_READ_D, seg->pid, (char*)addr+i, 0);
			if(errno)
				goto ptraceerr;
			if(n-i >= 4)
				*(u32int*)((char*)v+i) = u;
			else{
				*(u32int*)buf = u;
				memmove((char*)v+i, buf, n-i);
			}
		}else{
			if(n-i >= 4)
				u = *(u32int*)((char*)v+i);
			else{
				errno = 0;
				u = ptrace(PT_READ_D, seg->pid, (char*)addr+i, 0);
				if(errno)
					return -1;
				*(u32int*)buf = u;
				memmove(buf, (char*)v+i, n-i);
				u = *(u32int*)buf;
			}
			if(ptrace(PT_WRITE_D, seg->pid, (char*)addr+i, u) < 0)
				goto ptraceerr;
		}
	}
	return 0;

ptraceerr:
	werrstr("ptrace: %r");
	return -1;
}

static char *freebsdregs[] = {
	"FS",
	"ES",
	"DS",
	"DI",
	"SI",
	"BP",
	"SP",
	"BX",
	"DX",
	"CX",
	"AX",
	"TRAP",
	"PC",
	"CS",
	"EFLAGS",
	"SP",
	"SS",
	"GS",
};

static ulong
reg2freebsd(char *reg)
{
	int i;

	for(i=0; i<nelem(freebsdregs); i++)
		if(strcmp(freebsdregs[i], reg) == 0)
			return 4*i;
	return ~(ulong)0;
}

static int
ptraceregrw(Regs *regs, char *name, u64int *val, int isr)
{
	int pid;
	ulong addr;
	struct reg mregs;

	addr = reg2freebsd(name);
	if(~addr == 0){
		if(isr){
			*val = ~(ulong)0;
			return 0;
		}
		werrstr("register not available");
		return -1;
	}

	pid = ((PtraceRegs*)regs)->pid;
	if(ptrace(PT_GETREGS, pid, (char*)&mregs, 0) < 0)
		return -1;
	if(isr)
		*val = *(u32int*)((char*)&mregs+addr);
	else{
		*(u32int*)((char*)&mregs+addr) = *val;
		if(ptrace(PT_SETREGS, pid, (char*)&mregs, 0) < 0)
			return -1;
	}
	return 0;
}

char*
proctextfile(int pid)
{
	static char buf[1024], pbuf[128];

	snprint(pbuf, sizeof pbuf, "/proc/%d/file", pid);
	if(readlink(pbuf, buf, sizeof buf) >= 0)
		return buf;
	if(access(pbuf, AEXIST) >= 0)
		return pbuf;
	return nil;
}

/*

  status  The process status.  This file is read-only and returns a single
	     line containing multiple space-separated fields as follows:

	     o	 command name
	     o	 process id
	     o	 parent process id
	     o	 process group id
	     o	 session id
	     o	 major,minor of the controlling terminal, or -1,-1 if there is
		 no controlling terminal.
	     o	 a list of process flags: ctty if there is a controlling ter-
		 minal, sldr if the process is a session leader, noflags if
		 neither of the other two flags are set.
	     o	 the process start time in seconds and microseconds, comma
		 separated.
	     o	 the user time in seconds and microseconds, comma separated.
	     o	 the system time in seconds and microseconds, comma separated.
	     o	 the wait channel message
	     o	 the process credentials consisting of the effective user id
		 and the list of groups (whose first member is the effective
		 group id) all comma separated.
*/

int
procnotes(int pid, char ***pnotes)
{
	/* figure out the set of pending notes - how? */
	*pnotes = nil;
	return 0;
}

static int
isstopped(int pid)
{
	char buf[1024], *f[12];
	int fd, n, nf;

	snprint(buf, sizeof buf, "/proc/%d/status", pid);
	if((fd = open(buf, OREAD)) < 0)
		return 0;
	n = read(fd, buf, sizeof buf-1);
	close(fd);
	if(n <= 0)
		return 0;
	buf[n] = 0;

	if((nf = tokenize(buf, f, nelem(f))) < 11)
		return 0;
	if(strcmp(f[10], "nochan") == 0)
		return 1;
	return 0;
}

#undef waitpid

int
ctlproc(int pid, char *msg)
{
	int p, status;

	if(strcmp(msg, "hang") == 0){
		if(pid == getpid())
			return ptrace(PT_TRACE_ME, 0, 0, 0);
		werrstr("can only hang self");
		return -1;
	}
	if(strcmp(msg, "kill") == 0)
		return ptrace(PT_KILL, pid, 0, 0);
	if(strcmp(msg, "startstop") == 0){
		if(ptrace(PT_CONTINUE, pid, 0, 0) < 0)
			return -1;
		goto waitstop;
	}
/*
	if(strcmp(msg, "sysstop") == 0){
		if(ptrace(PTRACE_SYSCALL, pid, 0, 0) < 0)
			return -1;
		goto waitstop;
	}
*/
	if(strcmp(msg, "stop") == 0){
		if(kill(pid, SIGSTOP) < 0)
			return -1;
		goto waitstop;
	}
	if(strcmp(msg, "waitanyway") == 0)
		goto waitanyway;
	if(strcmp(msg, "waitstop") == 0){
	waitstop:
		if(isstopped(pid))
			return 0;
	waitanyway:
		for(;;){
			p = waitpid(pid, &status, WUNTRACED);
			if(p <= 0)
				return -1;
			if(WIFEXITED(status) || WIFSTOPPED(status))
				return 0;
		}
	}
	if(strcmp(msg, "start") == 0)
		return ptrace(PT_CONTINUE, pid, 0, 0);
	werrstr("unknown control message '%s'", msg);
	return -1;
}
