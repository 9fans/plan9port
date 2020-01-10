#include <u.h>
#include <thread_db.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <sys/procfs.h>	/* psaddr_t */
#include <libc.h>
#include <mach.h>
#include "ureg386.h"

int td_get_allthreads(td_thragent_t *ta, td_thrhandle_t **pall);

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

void
usage(void)
{
	fprint(2, "usage: t pid\n");
	exits("usage");
}

#define	STRINGSZ	128

/*
 *	print the value of dot as file:line
 */
void
printsource(long dot)
{
	char str[STRINGSZ];

	if (fileline(dot, str, STRINGSZ) >= 0)
		print("%s", str);
}

void
printlocals(Symbol *fn, Regs *regs)
{
	int i;
	u32int v;
	Symbol s;

	for (i = 0; indexlsym(fn, i, &s)>=0; i++) {
		if (s.class != CAUTO)
			continue;
		if(lget4(cormap, regs, s.loc, &v) >= 0)
			print("\t%s.%s/\t%#lux\n", fn->name, s.name, v);
		else
			print("\t%s.%s/\t?\n", fn->name, s.name);
	}
}

void
printparams(Symbol *fn, Regs *regs)
{
	int i;
	Symbol s;
	u32int v;
	int first = 0;
	ulong pc, sp, bp;

	if(0) print("pc=%lux sp=%lux bp=%lux ",
		(rget(regs, "PC", &pc), pc),
		(rget(regs, "SP", &sp), sp),
		(rget(regs, "BP", &bp), bp));
	for (i = 0; indexlsym(fn, i, &s)>=0; i++) {
		if (s.class != CPARAM)
			continue;
		if (first++)
			print(", ");
		if(0) print("(%d.%s.%ux.%x)", s.loc.type, s.loc.reg, s.loc.addr, s.loc.offset);
		if(lget4(cormap, regs, s.loc, &v) >= 0)
			print("%s=%#lux", s.name, v);
		else
			print("%s=?", s.name);
	}
}

/*
 *	callback on stack trace
 */
static int
xtrace(Map *map, Regs *regs, ulong pc, ulong nextpc, Symbol *sym, int depth)
{
	char buf[512];

	USED(map);
	print("\t");
	if(sym){
		print("%s(", sym->name);
		printparams(sym, regs);
		print(")+0x%ux ", pc-sym->loc.addr);
	}else
		print("%#lux ", pc);
	printsource(pc);

	print(" called from ");
	symoff(buf, 512, nextpc, CTEXT);
	print("%s ", buf);
/*	printsource(nextpc); */
	print("\n");
	if(sym)
		printlocals(sym, regs);
	return depth<40;
}

void
main(int argc, char **argv)
{
	struct ps_prochandle p;
	prgregset_t regs;
	int e;
	td_thragent_t *ta;
	td_thrhandle_t *ts;
	td_thrinfo_t info;
	int i, n;
	Ureg *u;
	UregRegs r;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	attachargs(argc, argv, OREAD);
	attachdynamic();

/*	if(!corpid && !corhdr) */
/*		sysfatal("could not attach to process"); */
/* */
	p.pid = corpid;
	if((e = td_ta_new(&p, &ta)) != TD_OK)
		sysfatal("td_ta_new: %s", terr(e));
	if((e = td_ta_get_nthreads(ta, &n)) != TD_OK)
		sysfatal("td_ta_get_nthreads: %s", terr(e));
	print("%d threads\n", n);

	if((n = td_get_allthreads(ta, &ts)) < 0)
		sysfatal("td_get_allthreads: %r");
	print("%d threads - regs = %p\n", n, regs);
	for(i=0; i<n; i++){
		if((e = td_thr_get_info(&ts[i], &info)) != TD_OK)
			sysfatal("td_thr_get_info: %s", terr(e));
		print("%d: startfunc=%lux stkbase=%lux pc=%lux sp=%lux lid=%d\n",
			i, info.ti_startfunc, info.ti_stkbase, info.ti_pc, info.ti_sp, info.ti_lid);
		if((e = td_thr_getgregs(&ts[i], regs)) != TD_OK)
			sysfatal("td_thr_getregs: %s", terr(e));
		print("%d: pc=%lux sp=%lux gs=%lux\n", i, regs[12], regs[15], regs[10]);
		if((u = _linux2ureg386((UregLinux386*)regs)) == nil)
			sysfatal("%r");
		r.r.rw = _uregrw;
		r.ureg = (uchar*)u;
		stacktrace(cormap, &r.r, xtrace);
	}
	exits(0);
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

/*
td_err_e td_ta_map_id2thr(const td_thragent_t *ta_p, thread_t tid,td_thrhandle_t *th_p);
*/

/*
int
threadregs(int tid, Regs **rp)
{
	check pid
	look up tid (td_ta_map_id2thr)
	create Regs with thr handle inside
	rw function calls thr_getregs and then
		pulls out the desired register
}

*/
