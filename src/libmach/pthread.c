#include <u.h>
#include <thread_db.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <sys/procfs.h>	/* psaddr_t */
#include <libc.h>
#include <mach.h>
#include "ureg386.h"
#include "elf.h"

static char *terr(int);

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

static td_thragent_t *ta;

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

int
pthreaddbinit(void)
{
	int e;
	struct ps_prochandle p;
	
	p.pid = 0;
	if((e = td_ta_new(&p, &ta)) != TD_OK){
		werrstr("%s", terr(e));
		return -1;
	}
	return 0;
}

Regs*
threadregs(uint tid)
{
	int e;
	static UregRegs r;
	static Ureg u;
	td_thrhandle_t th;
	prgregset_t regs;

	if(tid == 0)
		return correg;
	if(!ta)
		pthreaddbinit();
	if((e = td_ta_map_id2thr(ta, tid, &th)) != TD_OK
	|| (e = td_thr_getgregs(&th, regs)) != TD_OK){
		werrstr("reading thread registers: %s", terr(e));
		return nil;
	}
	linux2ureg386((UregLinux386*)regs, &u);
	r.r.rw = _uregrw;
	r.ureg = (uchar*)&u;
	return &r.r;
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
	if(get1(cormap, (ulong)addr, v, sz) < 0)
		return PS_ERR;
	return PS_OK;
}

int
ps_pdwrite(struct ps_prochandle *ph, psaddr_t addr, void *v, size_t sz)
{
	if(put1(cormap, (ulong)addr, v, sz) < 0)
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
			ureg2linux386(corhdr->thread[i].ureg, (UregLinux386*)regs);
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
	return PS_ERR;
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
	return PS_ERR;
//	return sys_ps_get_thread_area(ph, lwp, xxx, addr);
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

