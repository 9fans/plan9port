#include <u.h>
#include <thread_db.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <sys/procfs.h>	/* psaddr_t */
#include <libc.h>
#include <mach.h>

typedef struct Ptprog Ptprog;
struct Pprog
{
	Pthread *t;
	uint nt;
};

typedef struct Pthread Pthread;
struct Pthread
{
	td_thrhandle_t handle;
};

void
pthreadattach(int pid)
{
	
}

void pthreadattach()
	set up mapping

Regs *pthreadregs()
int npthread();



static int td_get_allthreads(td_thragent_t*, td_thrhandle_t**);
static int terr(int);


Regs*
threadregs()
{

}



typedef struct AllThread AllThread;
struct AllThread
{
	td_thrhandle_t *a;
	int n;
	int err;
};

static int
thritercb(const td_thrhandle_t *th, void *cb)
{
	td_thrhandle_t **p;
	AllThread *a;
	int n;

	a = cb;
	if((a->n&(a->n-1)) == 0){
		if(a->n == 0)
			n = 1;
		else
			n = a->n<<1;
		if((p = realloc(a->a, n*sizeof a->a[0])) == 0){
			a->err = -1;
			return -1;	/* stop iteration */
		}
		a->a = p;
	}
	a->a[a->n++] = *th;
	return 0;
}

int
td_get_allthreads(td_thragent_t *ta, td_thrhandle_t **pall)
{
	int e;
	AllThread a;

	a.a = nil;
	a.n = 0;
	a.err = 0;
	if((e = td_ta_thr_iter(ta, thritercb, &a, 
		TD_THR_ANY_STATE,
		TD_THR_LOWEST_PRIORITY,
		TD_SIGNO_MASK,
		TD_THR_ANY_USER_FLAGS)) != TD_OK){
		werrstr("%s", terr(e));
		return -1;
	}

	if(a.err){
		free(a.a);
		return -1;
	}

	*pall = a.a;
	return a.n;
}

static char *tderrstr[] =
{
[TD_OK]			"no error",
[TD_ERR]		"some error",
[TD_NOTHR]		"no matching thread found",
[TD_NOSV]		"no matching synchronization handle found",
[TD_NOLWP]		"no matching light-weight process found",
[TD_BADPH]		"invalid process handle",
[TD_BADTH]		"invalid thread handle",
[TD_BADSH]		"invalid synchronization handle",
[TD_BADTA]		"invalid thread agent",
[TD_BADKEY]		"invalid key",
[TD_NOMSG]		"no event available",
[TD_NOFPREGS]	"no floating-point register content available",
[TD_NOLIBTHREAD]	"application not linked with thread library",
[TD_NOEVENT]	"requested event is not supported",
[TD_NOEVENT]	"requested event is not supported",
[TD_NOCAPAB]	"capability not available",
[TD_DBERR]		"internal debug library error",
[TD_NOAPLIC]	"operation is not applicable",
[TD_NOTSD]		"no thread-specific data available",
[TD_MALLOC]		"out of memory",
[TD_PARTIALREG]	"not entire register set was read or written",
[TD_NOXREGS]	"X register set not available for given threads",
[TD_TLSDEFER]	"thread has not yet allocated TLS for given module",
[TD_VERSION]	"version mismatch twixt libpthread and libthread_db",
[TD_NOTLS]		"there is no TLS segment in the given module",
};

static char*
terr(int e)
{
	static char buf[50];

	if(e < 0 || e >= nelem(tderrstr) || tderrstr[e] == nil){
		snprint(buf, sizeof buf, "thread err %d", e);
		return buf;
	}
	return tderrstr[e];
}

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
	if(get1(ph->map, addr, v, sz) < 0)
		return PS_ERR;
	return PS_OK;
}

int
ps_pdwrite(struct ps_prochandle *ph, psaddr_t addr, void *v, size_t sz)
{
	if(put1(ph->map, addr, v, sz) < 0)
		return PS_ERR;
	return PS_OK;
}

int
ps_ptread(struct ps_prochandle *ph, psaddr_t addr, void *v, size_t sz)
{
	return ps_pdread(ph, addr, v, sz);
}

int
ps_ptwrite(struct ps_prochandle *ph, psaddr_t addr, void *v, size_t sz)
{
	return ps_pdwrite(ph, addr, v, sz);
}

int
ps_lgetregs(struct ps_prochandle *ph, lwpid_t lwp, prgregset_t regs)
{
	int i;

	USED(ph);
	if(corhdr == nil)
		return sys_ps_lgetregs(ph, lwp, regs);
	for(i=0; i<corhdr->nthread; i++){
		if(corhdr->thread[i].id == lwp){
			ureg2prgregset(corhdr->thread[i].ureg, regs);
			return PS_OK;
		}
	}
	return PS_ERR;
}

int
ps_lsetregs(struct ps_prochandle *ph, lwpid_t lwp, prgregset_t regs)
{
	if(corhdr == nil)
		return sys_ps_lsetregs(ph, lwp, regs);
	return PS_ERR;
}

int
ps_lgetfpregs(struct ps_prochandle *ph, lwpid_t lwp, prfpregset_t *fpregs)
{
	if(corhdr == nil)
		return sys_ps_lgetfpregs(ph, lwp, fpregs);
	/* BUG - Look in core dump. */
	return PS_ERR:
}

int
ps_lsetfpregs(struct ps_prochandle *ph, lwpid_t lwp, prfpregset_t *fpregs)
{
	if(corhdr == nil)
		return sys_ps_lsetfpregs(ph, lwp, fpregs);
	return PS_ERR;
}

/* Fetch the special per-thread address associated with the given LWP.
   This call is only used on a few platforms (most use a normal register).
   The meaning of the `int' parameter is machine-dependent.  */
int
ps_get_thread_area(struct ps_prochandle *ph, lwpid_t lwp, int xxx, psaddr_t *addr)
{
	return sys_ps_get_thread_area(ph, lwp, xxx, addr);
}

int
ps_pglobal_lookup(struct ps_prochandle *ph, char *object_name, char *sym_name, psaddr_t *sym_addr)
{
	Fhdr *fp;
	ulong addr;

	if((fp = findhdr(object_name)) == nil){
		print("libmach pthread: lookup %d %s %s => no such hdr\n", ph->pid, object_name, sym_name);
		return PS_NOSYM;
	}
	if(elfsymlookup(fp->elf, sym_name, &addr) < 0){
		print("libmach pthread: lookup %d %s %s => name not found\n", ph->pid, object_name, sym_name);
		return PS_NOSYM;
	}
	/* print("libmach pthread: lookup %d %s %s => 0x%lux\n", ph->pid, object_name, sym_name, addr); */
	*sym_addr = (void*)(addr+fp->base);
	return PS_OK;
}

