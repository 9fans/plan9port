#include <u.h>
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
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <libc.h>
#include <bio.h>

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
AUTOFRAMEWORK(CoreFoundation)
AUTOFRAMEWORK(IOKit)

#include "dat.h"

typedef struct Sample Sample;

struct Sample
{
	uint seq;
	host_cpu_load_info_data_t cpu, p_cpu;
	vm_size_t pgsize;
	double divisor;
	uint64_t time, p_time;
	vm_statistics_data_t vm_stat, p_vm_stat;
	boolean_t purgeable_is_valid;
#ifdef VM_SWAPUSAGE	/* 10.4+ */
	struct xsw_usage xsu;
#endif
	boolean_t xsu_valid;
	integer_t syscalls_mach, p_syscalls_mach;
	integer_t syscalls_unix, p_syscalls_unix;
	ulong csw, p_csw;
	uint net_ifaces;
	uvlong net_ipackets, p_net_ipackets;
	uvlong net_opackets, p_net_opackets;
	uvlong net_ibytes, p_net_ibytes;
	uvlong net_obytes, p_net_obytes;
	uvlong net_errors, p_net_errors;
	ulong usecs;
};

static Sample sample;

void xsample(int);
void xapm(int);
void xloadavg(int);
void xcpu(int);
void xswap(int);
void xvm(int);
void xnet(int);

void (*statfn[])(int) =
{
	xsample,
	xapm,
	xloadavg,
	xswap,
	xcpu,
	xvm,
	xnet,
	0
};

static mach_port_t stat_port;

void
sampleinit(void)
{
	mach_timebase_info_data_t info;

	if(stat_port)
		return;

	stat_port = mach_host_self();
	memset(&sample, 0, sizeof sample);
	if(host_page_size(stat_port, &sample.pgsize) != KERN_SUCCESS)
		sample.pgsize = 4096;

	/* populate clock tick info for timestamps */
	mach_timebase_info(&info);
        sample.divisor = 1000.0 * (double)info.denom/info.numer;
	sample.time = mach_absolute_time();
}

void
samplenet(void)
{
	struct ifaddrs *ifa_list, *ifa;
	struct if_data *if_data;

	ifa_list = nil;
	sample.net_ifaces = 0;
	if(getifaddrs(&ifa_list) == 0){
		sample.p_net_ipackets = sample.net_ipackets;
		sample.p_net_opackets = sample.net_opackets;
		sample.p_net_ibytes = sample.net_ibytes;
		sample.p_net_obytes = sample.net_obytes;
		sample.p_net_errors = sample.net_errors;

		sample.net_ipackets = 0;
		sample.net_opackets = 0;
		sample.net_ibytes = 0;
		sample.net_obytes = 0;
		sample.net_errors = 0;
		sample.net_ifaces = 0;

		for(ifa=ifa_list; ifa; ifa=ifa->ifa_next){
			if(ifa->ifa_addr->sa_family != AF_LINK)
				continue;
			if((ifa->ifa_flags&(IFF_UP|IFF_RUNNING)) == 0)
				continue;
			if(ifa->ifa_data == nil)
				continue;
			if(strncmp(ifa->ifa_name, "lo", 2) == 0)	/* loopback */
				continue;

			if_data = (struct if_data*)ifa->ifa_data;
			sample.net_ipackets += if_data->ifi_ipackets;
			sample.net_opackets += if_data->ifi_opackets;
			sample.net_ibytes += if_data->ifi_ibytes;
			sample.net_obytes += if_data->ifi_obytes;
			sample.net_errors += if_data->ifi_ierrors + if_data->ifi_oerrors;
			sample.net_ifaces++;
		}
		freeifaddrs(ifa_list);
	}
}


/*
 * The following forces the program to be run with the suid root as
 * all the other stat monitoring apps get set:
 *
 * -rwsr-xr-x   1 root  wheel  83088 Mar 20  2005 /usr/bin/top
 * -rwsrwxr-x   1 root  admin  54048 Mar 20  2005
 *	/Applications/Utilities/Activity Monitor.app/Contents/Resources/pmTool
 *
 * If Darwin eventually encompases more into sysctl then this
 * won't be required.
 */
void
sampleevents(void)
{
	uint i, j, pcnt, tcnt;
	mach_msg_type_number_t count;
	kern_return_t error;
	processor_set_t *psets, pset;
	task_t *tasks;
	task_events_info_data_t events;

	if((error = host_processor_sets(stat_port, &psets, &pcnt)) != KERN_SUCCESS){
		Bprint(&bout, "host_processor_sets: %s (make sure auxstats is setuid root)\n",
			mach_error_string(error));
		return;
	}

	sample.p_syscalls_mach = sample.syscalls_mach;
	sample.p_syscalls_unix = sample.syscalls_unix;
	sample.p_csw = sample.csw;

	sample.syscalls_mach = 0;
	sample.syscalls_unix = 0;
	sample.csw = 0;

	for(i=0; i<pcnt; i++){
		if((error=host_processor_set_priv(stat_port, psets[i], &pset)) != KERN_SUCCESS){
			Bprint(&bout, "host_processor_set_priv: %s\n", mach_error_string(error));
			return;
		}
		if((error=processor_set_tasks(pset, &tasks, &tcnt)) != KERN_SUCCESS){
			Bprint(&bout, "processor_set_tasks: %s\n", mach_error_string(error));
			return;
		}
		for(j=0; j<tcnt; j++){
			count = TASK_EVENTS_INFO_COUNT;
			if(task_info(tasks[j], TASK_EVENTS_INFO, (task_info_t)&events, &count) == KERN_SUCCESS){
				sample.syscalls_mach += events.syscalls_mach;
				sample.syscalls_unix += events.syscalls_unix;
				sample.csw += events.csw;
			}

			if(tasks[j] != mach_task_self())
				mach_port_deallocate(mach_task_self(), tasks[j]);
		}

		if((error = vm_deallocate((vm_map_t)mach_task_self(),
				(vm_address_t)tasks, tcnt*sizeof(task_t))) != KERN_SUCCESS){
			Bprint(&bout, "vm_deallocate: %s\n", mach_error_string(error));
			return;
		}

		if((error = mach_port_deallocate(mach_task_self(), pset)) != KERN_SUCCESS
		|| (error = mach_port_deallocate(mach_task_self(), psets[i])) != KERN_SUCCESS){
			Bprint(&bout, "mach_port_deallocate: %s\n", mach_error_string(error));
			return;
		}
	}

	if((error = vm_deallocate((vm_map_t)mach_task_self(), (vm_address_t)psets,
			pcnt*sizeof(processor_set_t))) != KERN_SUCCESS){
		Bprint(&bout, "vm_deallocate: %s\n", mach_error_string(error));
		return;
	}
}

void
xsample(int first)
{
	int mib[2];
	mach_msg_type_number_t count;
	size_t len;

	if(first){
		sampleinit();
		return;
	}

	sample.seq++;
	sample.p_time = sample.time;
	sample.time = mach_absolute_time();

	sample.p_vm_stat = sample.vm_stat;
	count = sizeof(sample.vm_stat) / sizeof(natural_t);
	host_statistics(stat_port, HOST_VM_INFO, (host_info_t)&sample.vm_stat, &count);

	if(sample.seq == 1)
		sample.p_vm_stat = sample.vm_stat;

#ifdef VM_SWAPUSAGE
	mib[0] = CTL_VM;
	mib[1] = VM_SWAPUSAGE;
	len = sizeof sample.xsu;
	sample.xsu_valid = TRUE;
	if(sysctl(mib, 2, &sample.xsu, &len, NULL, 0) < 0 && errno == ENOENT)
		sample.xsu_valid = FALSE;
#endif

	samplenet();
	sampleevents();

	sample.p_cpu = sample.cpu;
	count = HOST_CPU_LOAD_INFO_COUNT;
	host_statistics(stat_port, HOST_CPU_LOAD_INFO, (host_info_t)&sample.cpu, &count);
	sample.usecs = (double)(sample.time - sample.p_time)/sample.divisor;
	Bprint(&bout, "usecs %lud\n", sample.usecs);
}

void
xapm(int first)
{
	int i, battery;
	CFArrayRef array;
	CFDictionaryRef dict;
	CFTypeRef cf, src, value;

	if(first)
		return;

	src = IOPSCopyPowerSourcesInfo();
	array = IOPSCopyPowerSourcesList(src);

	for(i=0; i<CFArrayGetCount(array); i++){
		cf = CFArrayGetValueAtIndex(array, i);
		dict = IOPSGetPowerSourceDescription(src, cf);
		if(dict != nil){
			value = CFDictionaryGetValue(dict, CFSTR("Current Capacity"));
			if(value != nil){
				if(!CFNumberGetValue(value, kCFNumberIntType, &battery))
					battery = 100;
				Bprint(&bout, "battery =%d 100\n", battery);
				break;
			}
		}
	}

	CFRelease(array);
	CFRelease(src);
}

void
xnet(int first)
{
	uint n;
	ulong err, in, inb, out, outb;

	n = sample.net_ifaces;
	in = sample.net_ipackets - sample.p_net_ipackets;
	out = sample.net_opackets - sample.p_net_opackets;
	inb = sample.net_ibytes - sample.p_net_ibytes;
	outb = sample.net_obytes - sample.p_net_obytes;
	err = sample.net_errors - sample.p_net_errors;

       Bprint(&bout, "etherb %lud %d\n", inb+outb, n*1000000);
       Bprint(&bout, "ether %lud %d\n", in+out, n*1000);
       Bprint(&bout, "ethererr %lud %d\n", err, n*1000);
       Bprint(&bout, "etherin %lud %d\n", in, n*1000);
       Bprint(&bout, "etherout %lud %d\n", out, n*1000);
       Bprint(&bout, "etherinb %lud %d\n", inb, n*1000);
       Bprint(&bout, "etheroutb %lud %d\n", outb, n*1000);
}

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
		return 0;
	return u;
}

void
xvm(int first)
{
	natural_t total, active;

	if(first)
		return;

	total = sample.vm_stat.free_count
		+ sample.vm_stat.active_count
		+ sample.vm_stat.inactive_count
		+ sample.vm_stat.wire_count;

	active = sample.vm_stat.active_count
		+ sample.vm_stat.inactive_count
		+ sample.vm_stat.wire_count;

	if(total)
		Bprint(&bout, "mem =%lud %lud\n", active, total);

	Bprint(&bout, "context %lld 1000\n", (vlong)sample.csw);
	Bprint(&bout, "syscall %lld 1000\n", (vlong)sample.syscalls_mach+sample.syscalls_unix);
	Bprint(&bout, "intr %lld 1000\n",
		isys("vm.stats.sys.v_intr")
		+isys("vm.stats.sys.v_trap"));

	Bprint(&bout, "fault %lld 1000\n", sample.vm_stat.faults);
	Bprint(&bout, "fork %lld 1000\n",
		isys("vm.stats.vm.v_rforks")
		+isys("vm.stats.vm.v_vforks"));

/*    Bprint(&bout, "hits %lud of %lud lookups (%d%% hit rate)\n", */
/*               (asamp.vm_stat.hits), */
/*               (asamp.vm_stat.lookups), */
/*               (natural_t)(((double)asamp.vm_stat.hits*100)/ (double)asamp.vm_stat.lookups)); */
}

void
xcpu(int first)
{
	ulong user, sys, idle, nice, t;

	if(first)
		return;

	sys = sample.cpu.cpu_ticks[CPU_STATE_SYSTEM] -
		sample.p_cpu.cpu_ticks[CPU_STATE_SYSTEM];
	idle = sample.cpu.cpu_ticks[CPU_STATE_IDLE] -
		sample.p_cpu.cpu_ticks[CPU_STATE_IDLE];
	user = sample.cpu.cpu_ticks[CPU_STATE_USER] -
		sample.p_cpu.cpu_ticks[CPU_STATE_USER];
	nice = sample.cpu.cpu_ticks[CPU_STATE_NICE] -
		sample.p_cpu.cpu_ticks[CPU_STATE_NICE];

	t = sys+idle+user+nice;

	Bprint(&bout, "user =%lud %lud\n", user, t);
	Bprint(&bout, "sys =%lud %lud\n", sys, t);
	Bprint(&bout, "idle =%lud %lud\n", idle, t);
	Bprint(&bout, "nice =%lud %lud\n", nice, t);
}

void
xloadavg(int first)
{
	double l[3];

	if(first)
		return;

	if(getloadavg(l, 3) < 0)
		return;
	Bprint(&bout, "load %d 1000\n", (int)(l[0]*1000.0));
}

void
xswap(int first)
{
	if(first)
		return;

#ifdef VM_SWAPUSAGE
	if(sample.xsu_valid)
		Bprint(&bout, "swap %lld %lld\n",
			(vlong)sample.xsu.xsu_used,
			(vlong)sample.xsu.xsu_total);
#endif
}
