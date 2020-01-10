#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dat.h"

void xapm(int);
void xloadavg(int);
void xmeminfo(int);
void xnet(int);
void xstat(int);
void xvmstat(int);
void xwireless(int);

void (*statfn[])(int) =
{
	xapm,
	xloadavg,
	xmeminfo,
	xnet,
	xstat,
	xvmstat,
	xwireless,
	0
};

void
xapm(int first)
{
	static int fd = -1;
	int curr = -1;

	if(first){
		fd = open("/sys/class/power_supply/BAT0/capacity", OREAD);
		return;
	}
	if(fd == -1)
		return;

	readfile(fd);
	tokens(0);
	curr = atoi(tok[0]);

	if(curr != -1)
		Bprint(&bout, "battery =%d 100\n", curr);

}

void
xloadavg(int first)
{
	static int fd = -1;

	if(first){
		fd = open("/proc/loadavg", OREAD);
		return;
	}

	readfile(fd);
	tokens(0);
	if(ntok >= 1)
		Bprint(&bout, "load =%d 1000\n", (int)(atof(tok[0])*1000));
}

void
xmeminfo(int first)
{
	int i;
	vlong tot, used;
	vlong mtot, mfree;
	vlong stot, sfree;
	static int fd = -1;

	if(first){
		fd = open("/proc/meminfo", OREAD);
		return;
	}

	readfile(fd);
	mtot = 0;
	stot = 0;
	mfree = 0;
	sfree = 0;
	for(i=0; i<nline; i++){
		tokens(i);
		if(ntok < 3)
			continue;
		tot = atoll(tok[1]);
		used = atoll(tok[2]);
		if(strcmp(tok[0], "Mem:") == 0)
			Bprint(&bout, "mem =%lld %lld\n", used/1024, tot/1024);
		else if(strcmp(tok[0], "Swap:") == 0)
			Bprint(&bout, "swap =%lld %lld\n", used/1024, tot/1024);
		else if(strcmp(tok[0], "MemTotal:") == 0)
			mtot = atoll(tok[1]);	/* kb */
		else if(strcmp(tok[0], "MemFree:") == 0)
			mfree += atoll(tok[1]);
		else if(strcmp(tok[0], "Buffers:") == 0)
			mfree += atoll(tok[1]);
		else if(strcmp(tok[0], "Cached:") == 0){
			mfree += atoll(tok[1]);
			if(mtot < mfree)
				continue;
			Bprint(&bout, "mem =%lld %lld\n", mtot-mfree, mtot);
		}else if(strcmp(tok[0], "SwapTotal:") == 0)
			stot = atoll(tok[1]);	/* kb */
		else if(strcmp(tok[0], "SwapFree:") == 0){
			sfree = atoll(tok[1]);
			if(stot < sfree)
				continue;
			Bprint(&bout, "swap =%lld %lld\n", stot-sfree, stot);
		}
	}
}

void
xnet(int first)
{
	int i, n;
	vlong totb, totp, tote, totin, totou, totinb, totoub, b, p, e, in, ou, inb, oub;
	char *q;
	static int fd = -1;

	if(first){
		fd = open("/proc/net/dev", OREAD);
		return;
	}

	readfile(fd);
	n = 0;
	totb = 0;
	tote = 0;
	totp = 0;
	totin = 0;
	totou = 0;
	totinb = 0;
	totoub = 0;
	for(i=0; i<nline; i++){
		if((q = strchr(line[i], ':')) != nil)
			*q = ' ';
		tokens(i);
		if(ntok < 8+8)
			continue;
		if(strncmp(tok[0], "eth", 3) != 0 && strncmp(tok[0], "wlan", 4) != 0)
			continue;
		inb = atoll(tok[1]);
		oub = atoll(tok[9]);
		in = atoll(tok[2]);
		ou = atoll(tok[10]);
		b = inb+oub;
		p = in+ou;
		e = atoll(tok[3])+atoll(tok[11]);
		totb += b;
		totp += p;
		tote += e;
		totin += in;
		totou += ou;
		totinb += inb;
		totoub += oub;
		n++;
	}
	Bprint(&bout, "etherb %lld %d\n", totb, n*1000000);
	Bprint(&bout, "ether %lld %d\n", totp, n*1000);
	Bprint(&bout, "ethererr %lld %d\n", tote, n*1000);
	Bprint(&bout, "etherin %lld %d\n", totin, n*1000);
	Bprint(&bout, "etherout %lld %d\n", totou, n*1000);
	Bprint(&bout, "etherinb %lld %d\n", totinb, n*1000);
	Bprint(&bout, "etheroutb %lld %d\n", totoub, n*1000);
}

void
xstat(int first)
{
	static int fd = -1;
	int i;

	if(first){
		fd = open("/proc/stat", OREAD);
		return;
	}

	readfile(fd);
	for(i=0; i<nline; i++){
		tokens(i);
		if(ntok < 2)
			continue;
		if(strcmp(tok[0], "cpu") == 0 && ntok >= 5){
			Bprint(&bout, "user %lld 100\n", atoll(tok[1]));
			Bprint(&bout, "sys %lld 100\n", atoll(tok[3]));
			Bprint(&bout, "cpu %lld 100\n", atoll(tok[1])+atoll(tok[3]));
			Bprint(&bout, "idle %lld 100\n", atoll(tok[4]));
		}
	/*
		if(strcmp(tok[0], "page") == 0 && ntok >= 3){
			Bprint(&bout, "pagein %lld 500\n", atoll(tok[1]));
			Bprint(&bout, "pageout %lld 500\n", atoll(tok[2]));
			Bprint(&bout, "page %lld 1000\n", atoll(tok[1])+atoll(tok[2]));
		}
		if(strcmp(tok[0], "swap") == 0 && ntok >= 3){
			Bprint(&bout, "swapin %lld 500\n", atoll(tok[1]));
			Bprint(&bout, "swapout %lld 500\n", atoll(tok[2]));
			Bprint(&bout, "swap %lld 1000\n", atoll(tok[1])+atoll(tok[2]));
		}
	*/
		if(strcmp(tok[0], "intr") == 0)
			Bprint(&bout, "intr %lld 1000\n", atoll(tok[1]));
		if(strcmp(tok[0], "ctxt") == 0)
			Bprint(&bout, "context %lld 10000\n", atoll(tok[1]));
		if(strcmp(tok[0], "processes") == 0)
			Bprint(&bout, "fork %lld 1000\n", atoll(tok[1]));
	}
}

void
xvmstat(int first)
{
	static int fd = -1;
	int i;

	if(first){
		fd = open("/proc/vmstat", OREAD);
		return;
	}

	readfile(fd);
	for(i=0; i<nline; i++){
		tokens(i);
		if(ntok < 2)
			continue;
		if(strcmp(tok[0], "pgfault") == 0)
			Bprint(&bout, "fault %lld 100000\n", atoll(tok[1]));
	}
}

void
xwireless(int first)
{
	static int fd = -1;
	int i;

	if(first){
		fd = open("/proc/net/wireless", OREAD);
		return;
	}

	readfile(fd);
	for(i=0; i<nline; i++){
		tokens(i);
		if(ntok < 3)
			continue;
		if(strcmp(tok[0], "wlan0:") == 0)
			Bprint(&bout, "802.11 =%lld 100\n", atoll(tok[2]));
	}
}
