/*
 * process interface for Linux.
 * 
 * Uses ptrace for registers and data,
 * /proc for some process status.
 * There's not much point to worrying about
 * byte order here -- using ptrace means
 * we're running on the architecture we're debugging,
 * unless truly weird stuff is going on.
 *
 * It is tempting to use /proc/%d/mem along with
 * the sp and pc in the stat file to get a stack trace
 * without attaching to the program, but unfortunately
 * you can't read the mem file unless you've attached.
 */

#include <u.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/procfs.h>
#include <signal.h>
#include <errno.h>
#include <libc.h>
#include <mach.h>
#include <elf.h>
#include "ureg386.h"

Mach *machcpu = &mach386;

typedef struct PtraceRegs PtraceRegs;

struct PtraceRegs
{
	Regs r;
	int pid;
};

static int ptracesegrw(Map*, Seg*, ulong, void*, uint, int);
static int ptraceregrw(Regs*, char*, ulong*, int);

static int attachedpids[1000];
static int nattached;

int
ptraceattach(int pid)
{
	int i;

	/*
	if(nattached==1 && attachedpids[0] == pid)
		goto already;
	if(nattached)
		detachproc(attachedpids[0]);
	*/
	
	for(i=0; i<nattached; i++)
		if(attachedpids[i]==pid)
			return 0;
	if(nattached == nelem(attachedpids)){
		werrstr("attached to too many processes");
		return -1;
	}

	if(ptrace(PTRACE_ATTACH, pid, 0, 0) < 0){
		werrstr("ptrace attach %d: %r", pid);
		return -1;
	}
	
	if(ctlproc(pid, "waitstop") < 0){
		fprint(2, "waitstop: %r");
		ptrace(PTRACE_DETACH, pid, 0, 0);
		return -1;
	}
	attachedpids[nattached++] = pid;
	return 0;
}

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

	if(ptraceattach(pid) < 0)
		return -1;

	memset(&s, 0, sizeof s);
	s.base = 0;
	s.size = 0xFFFFFFFF;
	s.offset = 0;
	s.name = "data";
	s.file = nil;
	s.rw = ptracesegrw;
	s.pid = pid;
	if(addseg(map, s) < 0){
		fprint(2, "addseg: %r\n");
		return -1;
	}

	if((r = mallocz(sizeof(PtraceRegs), 1)) == nil){
		fprint(2, "mallocz: %r\n");
		return -1;
	}
	r->r.rw = ptraceregrw;
	r->pid = pid;
	*rp = (Regs*)r;
	return 0;
}

int
detachproc(int pid)
{
	int i;

	for(i=0; i<nattached; i++){
		if(attachedpids[i] == pid){
			attachedpids[i] = attachedpids[--nattached];
			break;
		}
	}
	return ptrace(PTRACE_DETACH, pid, 0, 0);
}

static int
ptracerw(int type, int xtype, int isr, int pid, ulong addr, void *v, uint n)
{
	int i;
	u32int u;
	uchar buf[4];

	for(i=0; i<n; i+=4){
		if(isr){
			errno = 0;
			u = ptrace(type, pid, addr+i, 0);
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
				u = ptrace(xtype, pid, addr+i, 0);
				if(errno)
					return -1;
				*(u32int*)buf = u;
				memmove(buf, (char*)v+i, n-i);
				u = *(u32int*)buf;
			}
			if(ptrace(type, pid, addr+i, &u) < 0)
				goto ptraceerr;
		}
	}
	return 0;

ptraceerr:
	werrstr("ptrace: %r");
	return -1;
}

static int
ptracesegrw(Map *map, Seg *seg, ulong addr, void *v, uint n, int isr)
{
	addr += seg->base;
	return ptracerw(isr ? PTRACE_PEEKDATA : PTRACE_POKEDATA, PTRACE_PEEKDATA,
		isr, seg->pid, addr, v, n);
}

static char* linuxregs[] = {
	"BX",
	"CX",
	"DX",
	"SI",
	"DI",
	"BP",
	"AX",
	"DS",
	"ES",
	"FS",
	"GS",
	"OAX",
	"PC",
	"CS",
	"EFLAGS",
	"SP",
	"SS",
};

static ulong
reg2linux(char *reg)
{
	int i;

	for(i=0; i<nelem(linuxregs); i++)
		if(strcmp(linuxregs[i], reg) == 0)
			return 4*i;
	return ~(ulong)0;
}

static int
ptraceregrw(Regs *regs, char *name, ulong *val, int isr)
{
	int pid;
	ulong addr;
	u32int u;

	pid = ((PtraceRegs*)regs)->pid;
	addr = reg2linux(name);
	if(~addr == 0){
		if(isr){
			*val = ~(ulong)0;
			return 0;
		}
		werrstr("register not available");
		return -1;
	}
	if(isr){
		errno = 0;
		u = ptrace(PTRACE_PEEKUSER, pid, addr, 0);
		if(errno)
			goto ptraceerr;
		*val = u;
	}else{
		u = *val;
		if(ptrace(PTRACE_POKEUSER, pid, addr, &u) < 0)
			goto ptraceerr;
	}
	return 0;

ptraceerr:
	werrstr("ptrace: %r");
	return -1;
}

static int
isstopped(int pid)
{
	char buf[1024];
	int fd, n;
	char *p;

	snprint(buf, sizeof buf, "/proc/%d/stat", pid);
	if((fd = open(buf, OREAD)) < 0)
		return 0;
	n = read(fd, buf, sizeof buf-1);
	close(fd);
	if(n <= 0)
		return 0;
	buf[n] = 0;

	/* command name is in parens, no parens afterward */
	p = strrchr(buf, ')');
	if(p == nil || *++p != ' ')
		return 0;
	++p;

	/* next is state - T is stopped for tracing */
	return *p == 'T';
}

/* /proc/pid/stat contains 
	pid
	command in parens
	0. state
	1. ppid
	2. pgrp
	3. session
	4. tty_nr
	5. tpgid
	6. flags (math=4, traced=10)
	7. minflt
	8. cminflt
	9. majflt
	10. cmajflt
	11. utime
	12. stime
	13. cutime
	14. cstime
	15. priority
	16. nice
	17. 0
	18. itrealvalue
	19. starttime
	20. vsize
	21. rss
	22. rlim
	23. startcode
	24. endcode
	25. startstack
	26. kstkesp
	27. kstkeip
	28. pending signal bitmap
	29. blocked signal bitmap
	30. ignored signal bitmap
	31. caught signal bitmap
	32. wchan
	33. nswap
	34. cnswap
	35. exit_signal
	36. processor
*/


int
procnotes(int pid, char ***pnotes)
{
	char buf[1024], *f[40];
	int fd, i, n, nf;
	char *p, *s, **notes;
	ulong sigs;
	extern char *_p9sigstr(int, char*);

	*pnotes = nil;
	snprint(buf, sizeof buf, "/proc/%d/stat", pid);
	if((fd = open(buf, OREAD)) < 0){
		fprint(2, "open %s: %r\n", buf);
		return -1;
	}
	n = read(fd, buf, sizeof buf-1);
	close(fd);
	if(n <= 0){
		fprint(2, "read %s: %r\n", buf);
		return -1;
	}
	buf[n] = 0;

	/* command name is in parens, no parens afterward */
	p = strrchr(buf, ')');
	if(p == nil || *++p != ' '){
		fprint(2, "bad format in /proc/%d/stat\n", pid);
		return -1;
	}
	++p;

	nf = tokenize(p, f, nelem(f));
	if(0) print("code 0x%lux-0x%lux stack 0x%lux kstk 0x%lux keip 0x%lux pending 0x%lux\n",
		strtoul(f[23], 0, 0), strtoul(f[24], 0, 0), strtoul(f[25], 0, 0),
		strtoul(f[26], 0, 0), strtoul(f[27], 0, 0), strtoul(f[28], 0, 0));
	if(nf <= 28)
		return -1;

	sigs = strtoul(f[28], 0, 0) & ~(1<<SIGCONT);
	if(sigs == 0){
		*pnotes = nil;
		return 0;
	}

	notes = mallocz(32*sizeof(char*), 0);
	if(notes == nil)
		return -1;
	n = 0;
	for(i=0; i<32; i++){
		if((sigs&(1<<i)) == 0)
			continue;
		if((s = _p9sigstr(i, nil)) == nil)
			continue;
		notes[n++] = s;
	}
	*pnotes = notes;
	return n;
}

#undef waitpid

int
ctlproc(int pid, char *msg)
{
	int p, status;

	if(strcmp(msg, "hang") == 0){
		if(pid == getpid())
			return ptrace(PTRACE_TRACEME, 0, 0, 0);
		werrstr("can only hang self");
		return -1;
	}
	if(strcmp(msg, "kill") == 0)
		return ptrace(PTRACE_KILL, pid, 0, 0);
	if(strcmp(msg, "startstop") == 0){
		if(ptrace(PTRACE_CONT, pid, 0, 0) < 0)
			return -1;
		goto waitstop;
	}
	if(strcmp(msg, "sysstop") == 0){
		if(ptrace(PTRACE_SYSCALL, pid, 0, 0) < 0)
			return -1;
		goto waitstop;
	}
	if(strcmp(msg, "stop") == 0){
		if(kill(pid, SIGSTOP) < 0)
			return -1;
		goto waitstop;
	}
	if(strcmp(msg, "waitstop") == 0){
	waitstop:
		if(isstopped(pid))
			return 0;
		for(;;){
			p = waitpid(pid, &status, WUNTRACED|__WALL);
			if(p <= 0){
				if(errno == ECHILD){
					if(isstopped(pid))
						return 0;
				}
				return -1;
			}
			if(WIFEXITED(status) || WIFSTOPPED(status))
				return 0;
		}
	}
	if(strcmp(msg, "start") == 0)
		return ptrace(PTRACE_CONT, pid, 0, 0);
	werrstr("unknown control message '%s'", msg);
	return -1;
}

char*
proctextfile(int pid)
{
	static char buf[1024], pbuf[128];

	snprint(pbuf, sizeof pbuf, "/proc/%d/exe", pid);
	if(readlink(pbuf, buf, sizeof buf) >= 0)
		return buf;
	if(access(pbuf, AEXIST) >= 0)
		return pbuf;
	return nil;
}


#if 0
	snprint(buf, sizeof buf, "/proc/%d/maps", pid);
	if((b = Bopen(buf, OREAD)) == nil){
		werrstr("open %s: %r", buf);
		return -1;
	}

/*
        08048000-08056000 r-xp 00000000 03:0c 64593      /usr/sbin/gpm
        08056000-08058000 rw-p 0000d000 03:0c 64593      /usr/sbin/gpm
        08058000-0805b000 rwxp 00000000 00:00 0
        40000000-40013000 r-xp 00000000 03:0c 4165       /lib/ld-2.2.4.so
        40013000-40015000 rw-p 00012000 03:0c 4165       /lib/ld-2.2.4.so
        4001f000-40135000 r-xp 00000000 03:0c 45494      /lib/libc-2.2.4.so
        40135000-4013e000 rw-p 00115000 03:0c 45494      /lib/libc-2.2.4.so
        4013e000-40142000 rw-p 00000000 00:00 0
        bffff000-c0000000 rwxp 00000000 00:00 0
*/

	file = nil;
	while((p = Brdline(b, '\n')) != nil){
		p[Blinelen(b)-1] = 0;
		memset(f, 0, sizeof f);
		if((nf = getfields(p, f, 6, 1, " ")) < 5)
			continue;
		base = strtoul(f[0], &p, 16);
		if(*p != '-')
			continue;
		end = strtoul(p+1, &p, 16);
		if(*p != 0)
			continue;
		offset = strtoul(f[2], &p, 16);
		if(*p != 0)
			continue;
		if(nf == 6)
			file = f[5];
		zero = atoi(f[4]) == 0;
		print("%lux-%lux %lux %s %s\n", base, end, offset, zero ? "data" : "text", file ? file : "");
		s.base = base;
		s.size = end - base;
		s.offset = offset;
		s.name = zero ? "data" : "text";
		s.file = strdup(file);
		s.rw = ptracerw;
		s.pid = pid;
		if(addseg(map, s) < 0){
			Bterm(b);
			ptrace(PTRACE_DETACH, pid, 0, 0);
			return -1;
		}
	}
	Bterm(b);
#endif

/*
 * bottom-end functions for libthread_db to call
 */
enum
{
	PS_OK,
	PS_ERR,
	PS_BADPID,
	PS_BADLWPID,
	PS_BADADDR,
	PS_NOSYM,
	PS_NOFPREGS,
};

pid_t
ps_getpid(struct ps_prochandle *ph)
{
	return ph->pid;
}

int
ps_pstop(const struct ps_prochandle *ph)
{
	return PS_ERR;
}

int
ps_pcontinue(const struct ps_prochandle *ph)
{
	return PS_ERR;
}

int
ps_lstop(const struct ps_prochandle *ph)
{
	return PS_ERR;
}

int
ps_lcontinue(const struct ps_prochandle *ph)
{
	return PS_ERR;
}

/* read/write data or text memory */
int
ps_pdread(struct ps_prochandle *ph, psaddr_t addr, void *v, size_t sz)
{
//print("read %d %p %d\n", ph->pid, addr, sz);
	if(ptracerw(PTRACE_PEEKDATA, 0, 1, ph->pid, (ulong)addr, v, sz) < 0)
		return PS_ERR;
//print("	=> 0x%lux\n", *(ulong*)v);
	return PS_OK;
}

int
ps_pdwrite(struct ps_prochandle *ph, psaddr_t addr, void *v, size_t sz)
{
//print("write %d %p\n", ph->pid, addr);
	if(ptracerw(PTRACE_POKEDATA, PTRACE_PEEKDATA, 0, ph->pid, (ulong)addr, v, sz) < 0)
		return PS_ERR;
	return PS_OK;
}

int
ps_ptread(struct ps_prochandle *ph, psaddr_t addr, void *v, size_t sz)
{
//print("read %d %p\n", ph->pid, addr);
	if(ptracerw(PTRACE_PEEKTEXT, 0, 1, ph->pid, (ulong)addr, v, sz) < 0)
		return PS_ERR;
	return PS_OK;
}

int
ps_ptwrite(struct ps_prochandle *ph, psaddr_t addr, void *v, size_t sz)
{
//print("write %d %p\n", ph->pid, addr);
	if(ptracerw(PTRACE_POKETEXT, PTRACE_PEEKTEXT, 0, ph->pid, (ulong)addr, v, sz) < 0)
		return PS_ERR;
	return PS_OK;
}

int
ps_lgetregs(struct ps_prochandle *ph, lwpid_t lwp, prgregset_t regs)
{
	if(lwp == 0){
		memset(regs, 0xfe, sizeof(regs[0])*nelem(linuxregs));
		return PS_OK;
	}
//print("getregs %d %p (%d)\n", lwp, regs, sizeof(regs[0])*nelem(linuxregs));
	
	if(ptraceattach(lwp) < 0){
		fprint(2, "ptrace attach: %r\n");
		return PS_ERR;
	}

	if(ptracerw(PTRACE_PEEKUSER, 0, 1, lwp, 0, regs, sizeof(regs[0])*nelem(linuxregs)) < 0){
		fprint(2, "ptrace: %r\n");
		return PS_ERR;
	}
	return PS_OK;
}

int
ps_lsetregs(struct ps_prochandle *ph, lwpid_t lwp, prgregset_t regs)
{
print("setregs %d\n", lwp);
	if(ptracerw(PTRACE_POKEUSER, PTRACE_PEEKUSER, 1, lwp, 0, regs, sizeof(regs[0])*nelem(linuxregs)) < 0)
		return PS_ERR;
	return PS_OK;
}

int
ps_lgetfpregs(struct ps_prochandle *ph, lwpid_t lwp, prfpregset_t *fpregs)
{
	if(ptracerw(PTRACE_PEEKUSER, 0, 1, lwp, 18*4, fpregs, sizeof *fpregs) < 0)
		return PS_ERR;
	return PS_OK;
}

int
ps_lsetfpregs(struct ps_prochandle *ph, lwpid_t lwp, prfpregset_t *fpregs)
{
	if(ptracerw(PTRACE_POKEUSER, PTRACE_PEEKUSER, 1, lwp, 18*4, fpregs, sizeof *fpregs) < 0)
		return PS_ERR;
	return PS_OK;
}

/* Fetch the special per-thread address associated with the given LWP.
   This call is only used on a few platforms (most use a normal register).
   The meaning of the `int' parameter is machine-dependent.  */
int
ps_get_thread_area(struct ps_prochandle *ph, lwpid_t lwp, int xxx, psaddr_t *addr)
{
	return PS_NOSYM;
}

/* Look up the named symbol in the named DSO in the symbol tables
   associated with the process being debugged, filling in *SYM_ADDR
   with the corresponding run-time address.  */
int
ps_pglobal_lookup(struct ps_prochandle *ph, char *object_name, char *sym_name, psaddr_t *sym_addr)
{
	Fhdr *fp;
	ulong addr;

	if((fp = findhdr(object_name)) == nil){
print("lookup %d %s %s => no such hdr\n", ph->pid, object_name, sym_name);
		return PS_NOSYM;
	}
	if(elfsymlookup(fp->elf, sym_name, &addr) < 0){
print("lookup %d %s %s => name not found\n", ph->pid, object_name, sym_name);
		return PS_NOSYM;
	}
print("lookup %d %s %s => 0x%lux\n", ph->pid, object_name, sym_name, addr);
	*sym_addr = (void*)(addr+fp->base);
	return PS_OK;
}

Ureg*
_linux2ureg386(UregLinux386 *l)
{
	Ureg *u;

	u = malloc(sizeof(Ureg));
	if(u == nil)
		return nil;
	u->di = l->edi;
	u->si = l->esi;
	u->bp = l->ebp;
	u->nsp = l->esp;
	u->bx = l->ebx;
	u->dx = l->edx;
	u->cx = l->ecx;
	u->ax = l->eax;
	u->gs = l->xgs;
	u->fs = l->xfs;
	u->es = l->xes;
	u->ds = l->xds;
	u->trap = ~0; // l->trapno;
	u->ecode = ~0; // l->err;
	u->pc = l->eip;
	u->cs = l->xcs;
	u->flags = l->eflags;
	u->sp = l->esp;
	u->ss = l->xss;
	return u;
}
