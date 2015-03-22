#include <u.h>
#include <kvm.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sched.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_var.h>
#include <machine/apmvar.h>
#include <sys/ioctl.h>
#include <uvm/uvm_param.h>
#include <uvm/uvm_extern.h>
#include <limits.h>
#include <libc.h>
#include <bio.h>
#include "dat.h"

void xapm(int);
void xloadavg(int);
void xcpu(int);
void xswap(int);
void xsysctl(int);
void xnet(int);
void xkvm(int);

void (*statfn[])(int) =
{
	xkvm,
	xapm,
	xloadavg,
	xcpu,
	xsysctl,
	xnet,
	0
};

static kvm_t *kvm;

static struct nlist nl[] = {
	{ "_ifnet" },
	{ "_cp_time" },
	{ "" }
};

void
xloadavg(int first)
{
	double l[3];

	if(first)
		return;

	if(getloadavg(l, 3) < 0)
		return;
	Bprint(&bout, "load =%d 1000\n", (int)(l[0]*1000.0));
}

void
xapm(int first)
{
	static int fd;
	struct apm_power_info ai;

	if(first){
		fd = open("/dev/apm", OREAD);
		return;
	}

	if(ioctl(fd, APM_IOC_GETPOWER, &ai) < 0)
		return;

	if(ai.battery_life <= 100)
		Bprint(&bout, "battery =%d 100\n", ai.battery_life);
}


void
kvminit(void)
{
	char buf[_POSIX2_LINE_MAX];

	if(kvm)
		return;
	kvm = kvm_openfiles(nil, nil, nil, O_RDONLY, buf);
	if(kvm == nil) {
		fprint(2, "kvm open error\n%s", buf);
		return;
	}
	if(kvm_nlist(kvm, nl) < 0 || nl[0].n_type == 0){
		kvm = nil;
		return;
	}
}

void
xkvm(int first)
{
	if(first)
		kvminit();
}

int
kread(ulong addr, char *buf, int size)
{
	if(kvm_read(kvm, addr, buf, size) != size){
		memset(buf, 0, size);
		return -1;
	}
	return size;
}

void
xnet(int first)
{
	ulong out, in, outb, inb, err;
	static ulong ifnetaddr;
	ulong addr;
	struct ifnet ifnet;
	struct ifnet_head ifnethead;
	char name[16];

	if(first)
		return;

	if(ifnetaddr == 0){
		ifnetaddr = nl[0].n_value;
		if(ifnetaddr == 0)
			return;
	}

	if(kread(ifnetaddr, (char*)&ifnethead, sizeof ifnethead) < 0)
		return;

	out = in = outb = inb = err = 0;
	addr = (ulong)TAILQ_FIRST(&ifnethead);
	while(addr){
		if(kread(addr, (char*)&ifnet, sizeof ifnet) < 0
		|| kread((ulong)ifnet.if_xname, name, 16) < 0)
			return;
		name[15] = 0;
		addr = (ulong)TAILQ_NEXT(&ifnet, if_list);
		out += ifnet.if_opackets;
		in += ifnet.if_ipackets;
		outb += ifnet.if_obytes;
		inb += ifnet.if_ibytes;
		err += ifnet.if_oerrors+ifnet.if_ierrors;
	}
	Bprint(&bout, "etherin %lud 1000\n", in);
	Bprint(&bout, "etherout %lud 1000\n", out);
	Bprint(&bout, "etherinb %lud 1000000\n", inb);
	Bprint(&bout, "etheroutb %lud 1000000\n", outb);
	Bprint(&bout, "ethererr %lud 1000\n", err);
	Bprint(&bout, "ether %lud 1000\n", in+out);
	Bprint(&bout, "etherb %lud 1000000\n", inb+outb);
}

void
xcpu(int first)
{
	static int stathz;
	ulong x[20];
	struct clockinfo *ci;
	int mib[2];
	size_t l;

	if(first){
		mib[0] = CTL_KERN;
		mib[1] = KERN_CLOCKRATE;
		l = sizeof(x);
		sysctl(mib, 2, (char *)&x, &l, nil, 0);
		x[l] = 0;
		if (l < sizeof(ci))
			stathz = 128;
		else{
			ci = (struct clockinfo*)x;
			stathz = ci->stathz;
		}
		return;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_CPTIME;
	l = sizeof(x);
	sysctl(mib, 2, (char *)&x, &l, nil, 0);
	if (l < 5*sizeof(ulong))
		return;
	x[l] = 0;

	Bprint(&bout, "user %lud %d\n", x[CP_USER]+x[CP_NICE], stathz);
	Bprint(&bout, "sys %lud %d\n", x[CP_SYS], stathz);
	Bprint(&bout, "cpu %lud %d\n", x[CP_USER]+x[CP_NICE]+x[CP_SYS], stathz);
	Bprint(&bout, "idle %lud %d\n", x[CP_IDLE], stathz);
}

void
xsysctl(int first)
{
	struct uvmexp vm;
	static int pgsize;
	int mib[2];
	size_t l;

	l = sizeof(vm);
	mib[0] = CTL_VM;
	mib[1] = VM_UVMEXP;
	sysctl(mib, 2, &vm, &l, nil, 0);
	if (l < sizeof(vm))
		return;

	if (first)
		pgsize = vm.pagesize;

	Bprint(&bout, "mem =%lud %lud\n", vm.active*pgsize, vm.npages*pgsize);
	Bprint(&bout, "context %lud 1000\n", vm.swtch);
	Bprint(&bout, "syscall %lud 1000\n", vm.syscalls);
	Bprint(&bout, "intr %lud 1000\n", vm.intrs+vm.traps);
	Bprint(&bout, "fault %lud 1000\n", vm.faults);

	Bprint(&bout, "fork %ud 1000\n", vm.forks);
	Bprint(&bout, "swap =%lud %lud\n", vm.swpginuse*pgsize, vm.swpages*pgsize);
}
