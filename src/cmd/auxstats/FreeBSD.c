#include <u.h>
#include <kvm.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/dkstat.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#if __FreeBSD_version < 600000
#include <machine/apm_bios.h>
#endif
#include <sys/ioctl.h>
#include <limits.h>
#include <libc.h>
#include <bio.h>
#include <ifaddrs.h>
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
	xswap,
	xcpu,
	xsysctl,
	xnet,
	0
};

static kvm_t *kvm;

static struct nlist nl[] = {
	{ "_cp_time" },
	{ "" }
};

void
kvminit(void)
{
	char buf[_POSIX2_LINE_MAX];

	if(kvm)
		return;
	kvm = kvm_openfiles(nil, nil, nil, OREAD, buf);
	if(kvm == nil)
		return;
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
	struct ifaddrs *ifap, *ifa;
	ulong out, in, outb, inb, err;

	if(first)
		return;

	if (getifaddrs(&ifap) != 0)
		return;

	out = in = outb = inb = err = 0;
#define	IFA_STAT(s)	(((struct if_data *)ifa->ifa_data)->ifi_ ## s)
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		out += IFA_STAT(opackets);
		in += IFA_STAT(ipackets);
		outb += IFA_STAT(obytes);
		inb += IFA_STAT(ibytes);
		err += IFA_STAT(oerrors) + IFA_STAT(ierrors);
	}
	freeifaddrs(ifap);

	Bprint(&bout, "etherin %lud 1000\n", in);
	Bprint(&bout, "etherout %lud 1000\n", out);
	Bprint(&bout, "etherinb %lud 1000000\n", inb);
	Bprint(&bout, "etheroutb %lud 1000000\n", outb);
	Bprint(&bout, "ethererr %lud 1000\n", err);
	Bprint(&bout, "ether %lud 1000\n", in+out);
	Bprint(&bout, "etherb %lud 1000000\n", inb+outb);
}

#if __FreeBSD_version >= 500000
int
xacpi(int first)
{
	int rv;
	int val;
	size_t len;

	len = sizeof(val);
	rv = sysctlbyname("hw.acpi.battery.life", &val, &len, nil, 0);
	if(rv != 0)
		return -1;
	Bprint(&bout, "battery =%d 100\n", val);
	return 0;
}
#else
int
xacpi(int first)
{
	return -1;
}
#endif

#if __FreeBSD_version < 600000
void
xapm(int first)
{
	static int fd;
	struct apm_info ai;

	if(first){
		xacpi(first);
		fd = open("/dev/apm", OREAD);
		return;
	}

	if(xacpi(0) >= 0)
		return;

	if(ioctl(fd, APMIO_GETINFO, &ai) < 0)
		return;

	if(ai.ai_batt_life <= 100)
		Bprint(&bout, "battery =%d 100\n", ai.ai_batt_life);
}
#else
void
xapm(int first)
{
	xacpi(first);
}
#endif

int
rsys(char *name, char *buf, int len)
{
	size_t l;

	l = len;
	if(sysctlbyname(name, buf, &l, nil, 0) < 0)
		return -1;
	buf[l] = 0;
	return l;
}

vlong
isys(char *name)
{
	ulong u;
	size_t l;

	l = sizeof u;
	if(sysctlbyname(name, &u, &l, nil, 0) < 0)
		return -1;
	return u;
}

void
xsysctl(int first)
{
	static int pgsize;

	if(first){
		pgsize = isys("vm.stats.vm.v_page_size");
		if(pgsize == 0)
			pgsize = 4096;
	}

	Bprint(&bout, "mem =%lld %lld\n",
		isys("vm.stats.vm.v_active_count")*pgsize,
		isys("vm.stats.vm.v_page_count")*pgsize);
	Bprint(&bout, "context %lld 1000\n", isys("vm.stats.sys.v_swtch"));
	Bprint(&bout, "syscall %lld 1000\n", isys("vm.stats.sys.v_syscall"));
	Bprint(&bout, "intr %lld 1000\n", isys("vm.stats.sys.v_intr")+isys("vm.stats.sys.v_trap"));
	Bprint(&bout, "fault %lld 1000\n", isys("vm.stats.vm.v_vm_faults"));
	Bprint(&bout, "fork %lld 1000\n", isys("vm.stats.vm.v_forks")
		+isys("vm.stats.vm.v_rforks")
		+isys("vm.stats.vm.v_vforks"));
}

void
xcpu(int first)
{
	static int stathz;
	union {
		ulong x[20];
		struct clockinfo ci;
	} u;
	int n;

	if(first){
		if(rsys("kern.clockrate", (char*)u.x, sizeof u.x) < sizeof u.ci)
			stathz = 128;
		else
			stathz = u.ci.stathz;
		return;
	}

	if((n=rsys("kern.cp_time", (char*)u.x, sizeof u.x)) < 5*sizeof(ulong))
		return;

	Bprint(&bout, "user %lud %d\n", u.x[CP_USER]+u.x[CP_NICE], stathz);
	Bprint(&bout, "sys %lud %d\n", u.x[CP_SYS], stathz);
	Bprint(&bout, "cpu %lud %d\n", u.x[CP_USER]+u.x[CP_NICE]+u.x[CP_SYS], stathz);
	Bprint(&bout, "idle %lud %d\n", u.x[CP_IDLE], stathz);
}

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
xswap(int first)
{
	static struct kvm_swap s;
	static ulong pgin, pgout;
	int i, o;
	static int pgsize;

	if(first){
		pgsize = getpagesize();
		if(pgsize == 0)
			pgsize = 4096;
		return;
	}

	if(kvm == nil)
		return;

	i = isys("vm.stats.vm.v_swappgsin");
	o = isys("vm.stats.vm.v_swappgsout");
	if(i != pgin || o != pgout){
		pgin = i;
		pgout = o;
		kvm_getswapinfo(kvm, &s, 1, 0);
	}


	Bprint(&bout, "swap =%lld %lld\n", s.ksw_used*(vlong)pgsize, s.ksw_total*(vlong)pgsize);
}
