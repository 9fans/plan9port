#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>

SunStatus
suncallpack(SunProg *prog, uchar *a, uchar *ea, uchar **pa, SunCall *c)
{
	uchar *x;
	int (*pack)(uchar*, uchar*, uchar**, SunCall*);

	if(pa == nil)
		pa = &x;
	if((int32)c->type < 0 || c->type >= prog->nproc || (pack=prog->proc[c->type].pack) == nil)
		return SunProcUnavail;
	if((*pack)(a, ea, pa, c) < 0)
		return SunGarbageArgs;
	return SunSuccess;
}

SunStatus
suncallunpack(SunProg *prog, uchar *a, uchar *ea, uchar **pa, SunCall *c)
{
	uchar *x;
	int (*unpack)(uchar*, uchar*, uchar**, SunCall*);

	if(pa == nil)
		pa = &x;
	if((int32)c->type < 0 || c->type >= prog->nproc || (unpack=prog->proc[c->type].unpack) == nil)
		return SunProcUnavail;
	if((*unpack)(a, ea, pa, c) < 0){
		fprint(2, "%ud %d: '%.*H' unpack failed\n", prog->prog, c->type, (int)(ea-a), a);
		return SunGarbageArgs;
	}
	return SunSuccess;
}

SunStatus
suncallunpackalloc(SunProg *prog, SunCallType type, uchar *a, uchar *ea, uchar **pa, SunCall **pc)
{
	uchar *x;
	uint size;
	int (*unpack)(uchar*, uchar*, uchar**, SunCall*);
	SunCall *c;

	if(pa == nil)
		pa = &x;
	if((int32)type < 0 || type >= prog->nproc || (unpack=prog->proc[type].unpack) == nil)
		return SunProcUnavail;
	size = prog->proc[type].sizeoftype;
	if(size == 0)
		return SunProcUnavail;
	c = mallocz(size, 1);
	if(c == nil)
		return SunSystemErr;
	c->type = type;
	if((*unpack)(a, ea, pa, c) < 0){
		fprint(2, "in: %.*H unpack failed\n", (int)(ea-a), a);
		free(c);
		return SunGarbageArgs;
	}
	*pc = c;
	return SunSuccess;
}

uint
suncallsize(SunProg *prog, SunCall *c)
{
	uint (*size)(SunCall*);

	if((int32)c->type < 0 || c->type >= prog->nproc || (size=prog->proc[c->type].size) == nil)
		return ~0;
	return (*size)(c);
}
