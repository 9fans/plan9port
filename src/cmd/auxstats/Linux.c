#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dat.h"

void xapm(int);
void xloadavg(int);
void xmeminfo(int);
void xnet(int);
void xstat(int);

void (*statfn[])(int) =
{
	xapm,
	xloadavg,
	xmeminfo,
	xnet,
	xstat,
	0
};

void
xapm(int first)
{
	static int fd = -1;

	if(first){
		fd = open("/proc/apm", OREAD);
		return;
	}
	readfile(fd);
	tokens(0);
	if(ntok >= 7 && atoi(tok[6]) != -1)
		Bprint(&bout, "battery =%d 100\n", atoi(tok[6]));
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
	static int fd = -1;

	if(first){
		fd = open("/proc/meminfo", OREAD);
		return;
	}

	readfile(fd);
	for(i=0; i<nline; i++){
		tokens(i);
		if(ntok < 4)
			continue;
		tot = atoll(tok[1]);
		used = atoll(tok[2]);
		if(strcmp(tok[0], "Mem:") == 0)
			Bprint(&bout, "mem =%lld %lld\n", used/1024, tot/1024);
		else if(strcmp(tok[0], "Swap:") == 0)
			Bprint(&bout, "swap =%lld %lld\n", used/1024, tot/1024);
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
		tokens(i);
		if(ntok < 8+8)
			continue;
		if(strncmp(tok[0], "eth", 3) != 0)
			continue;
		q = strchr(tok[0], ':');
		*q++ = 0;
		tok[0] = q;
		inb = atoll(tok[0]);
		oub = atoll(tok[8]);
		in = atoll(tok[1]);
		ou = atoll(tok[9]);
		b = inb+oub;
		p = in+ou;
		e = atoll(tok[2])+atoll(tok[10]);
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
			Bprint(&bout, "idle %lld\n", atoll(tok[4]));
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
			Bprint(&bout, "interrupt %lld 1000\n", atoll(tok[1]));
		if(strcmp(tok[0], "ctxt") == 0)
			Bprint(&bout, "context %lld 1000\n", atoll(tok[1]));
		if(strcmp(tok[0], "processes") == 0)
			Bprint(&bout, "fork %lld 1000\n", atoll(tok[1]));
	}
}

