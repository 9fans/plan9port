/*
 * SUN NFSv3 Mounter.  See RFC 1813
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>

void
nfsmount3tnullprint(Fmt *fmt, NfsMount3TNull *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "NfsMount3TNull");
}
uint
nfsmount3tnullsize(NfsMount3TNull *x)
{
	uint a;
	USED(x);
	a = 0;
	return a;
}
int
nfsmount3tnullpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TNull *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
int
nfsmount3tnullunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TNull *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
void
nfsmount3rnullprint(Fmt *fmt, NfsMount3RNull *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "NfsMount3RNull");
}
uint
nfsmount3rnullsize(NfsMount3RNull *x)
{
	uint a;
	USED(x);
	a = 0;
	return a;
}
int
nfsmount3rnullpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RNull *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
int
nfsmount3rnullunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RNull *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
void
nfsmount3tmntprint(Fmt *fmt, NfsMount3TMnt *x)
{
	fmtprint(fmt, "%s\n", "NfsMount3TMnt");
	fmtprint(fmt, "\t%s=", "path");
	fmtprint(fmt, "\"%s\"", x->path);
	fmtprint(fmt, "\n");
}
uint
nfsmount3tmntsize(NfsMount3TMnt *x)
{
	uint a;
	USED(x);
	a = 0 + sunstringsize(x->path);
	return a;
}
int
nfsmount3tmntpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TMnt *x)
{
	if(sunstringpack(a, ea, &a, &x->path, 1024) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfsmount3tmntunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TMnt *x)
{
	if(sunstringunpack(a, ea, &a, &x->path, 1024) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfsmount3rmntprint(Fmt *fmt, NfsMount3RMnt *x)
{
	fmtprint(fmt, "%s\n", "NfsMount3RMnt");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%ud", x->status);
	fmtprint(fmt, "\n");
	switch(x->status){
	case 0:
		fmtprint(fmt, "\t%s=", "handle");
		fmtprint(fmt, "%.*H", x->len, x->handle);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfsmount3rmntsize(NfsMount3RMnt *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->status){
	case 0:
		a = a + sunvaropaquesize(x->len);
		a = a + 4 + 4 * x->nauth;
		break;
	}
	return a;
}
uint
nfsmount1rmntsize(NfsMount3RMnt *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->status){
	case 0:
		a = a + NfsMount1HandleSize;
		break;
	}
	return a;
}

int
nfsmount3rmntpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RMnt *x)
{
	int i;
	if(sunuint32pack(a, ea, &a, &x->status) < 0) goto Err;
	switch(x->status){
	case 0:
		if(sunvaropaquepack(a, ea, &a, &x->handle, &x->len, NfsMount3MaxHandleSize) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->nauth) < 0) goto Err;
		for(i=0; i<x->nauth; i++)
			if(sunuint32pack(a, ea, &a, &x->auth[i]) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfsmount1rmntpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RMnt *x)
{
	if(sunuint32pack(a, ea, &a, &x->status) < 0) goto Err;
	switch(x->status){
	case 0:
		if(x->len != NfsMount1HandleSize)
			goto Err;
		if(sunfixedopaquepack(a, ea, &a, x->handle, NfsMount1HandleSize) < 0) goto Err;
		if(x->nauth != 0)
			goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfsmount1rmntunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RMnt *x)
{
	if(sunuint32unpack(a, ea, &a, &x->status) < 0) goto Err;
	switch(x->status){
	case 0:
		x->len = NfsMount1HandleSize;
		x->nauth = 0;
		x->auth = 0;
		if(sunfixedopaqueunpack(a, ea, &a, x->handle, NfsMount1HandleSize) < 0) goto Err;
		if(x->nauth != 0)
			goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}

int
nfsmount3rmntunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RMnt *x)
{
	int i;

	if(sunuint32unpack(a, ea, &a, &x->status) < 0) goto Err;
	switch(x->status){
	case 0:
		if(sunvaropaqueunpack(a, ea, &a, &x->handle, &x->len, NfsMount3MaxHandleSize) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->nauth) < 0) goto Err;
		x->auth = (u32int*)a;
		for(i=0; i<x->nauth; i++)
			if(sunuint32unpack(a, ea, &a, &x->auth[i]) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfsmount3tdumpprint(Fmt *fmt, NfsMount3TDump *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "NfsMount3TDump");
}
uint
nfsmount3tdumpsize(NfsMount3TDump *x)
{
	uint a;
	USED(x);
	a = 0;
	return a;
}
int
nfsmount3tdumppack(uchar *a, uchar *ea, uchar **pa, NfsMount3TDump *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
int
nfsmount3tdumpunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TDump *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
void
nfsmount3entryprint(Fmt *fmt, NfsMount3Entry *x)
{
	fmtprint(fmt, "%s\n", "NfsMount3Entry");
	fmtprint(fmt, "\t%s=", "host");
	fmtprint(fmt, "\"%s\"", x->host);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "path");
	fmtprint(fmt, "\"%s\"", x->path);
	fmtprint(fmt, "\n");
}
uint
nfsmount3entrysize(NfsMount3Entry *x)
{
	uint a;
	USED(x);
	a = 0 + sunstringsize(x->host) + sunstringsize(x->path);
	return a;
}
int
nfsmount3entrypack(uchar *a, uchar *ea, uchar **pa, NfsMount3Entry *x)
{
	u1int one;

	one = 1;
	if(sunuint1pack(a, ea, &a, &one) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->host, 255) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->path, 1024) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfsmount3entryunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3Entry *x)
{
	u1int one;

	if(sunuint1unpack(a, ea, &a, &one) < 0 || one != 1) goto Err;
	if(sunstringunpack(a, ea, &a, &x->host, NfsMount3MaxNameSize) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->path, NfsMount3MaxPathSize) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfsmount3rdumpprint(Fmt *fmt, NfsMount3RDump *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "NfsMount3RDump");
}
uint
nfsmount3rdumpsize(NfsMount3RDump *x)
{
	uint a;
	USED(x);
	a = 0;
	a += x->count;
	a += 4;
	return a;
}
int
nfsmount3rdumppack(uchar *a, uchar *ea, uchar **pa, NfsMount3RDump *x)
{
	u1int zero;

	zero = 0;
	if(a+x->count > ea) goto Err;
	memmove(a, x->data, x->count);
	a += x->count;
	if(sunuint1pack(a, ea, &a, &zero) < 0)
		goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfsmount3rdumpunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RDump *x)
{
	int i;
	uchar *oa;
	u1int u1;
	u32int u32;

	oa = a;
	for(i=0;; i++){
		if(sunuint1unpack(a, ea, &a, &u1) < 0)
			goto Err;
		if(u1 == 0)
			break;
		if(sunuint32unpack(a, ea, &a, &u32) < 0
		|| u32 > NfsMount3MaxNameSize
		|| (a+=u32) >= ea
		|| sunuint32unpack(a, ea, &a, &u32) < 0
		|| u32 > NfsMount3MaxPathSize
		|| (a+=u32) >= ea)
			goto Err;
	}
	x->count = (a-4) - oa;
	x->data = oa;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfsmount3tumntprint(Fmt *fmt, NfsMount3TUmnt *x)
{
	fmtprint(fmt, "%s\n", "NfsMount3TUmnt");
	fmtprint(fmt, "\t%s=", "path");
	fmtprint(fmt, "\"%s\"", x->path);
	fmtprint(fmt, "\n");
}
uint
nfsmount3tumntsize(NfsMount3TUmnt *x)
{
	uint a;
	USED(x);
	a = 0 + sunstringsize(x->path);
	return a;
}
int
nfsmount3tumntpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TUmnt *x)
{
	if(sunstringpack(a, ea, &a, &x->path, 1024) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfsmount3tumntunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TUmnt *x)
{
	if(sunstringunpack(a, ea, &a, &x->path, 1024) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfsmount3rumntprint(Fmt *fmt, NfsMount3RUmnt *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "NfsMount3RUmnt");
}
uint
nfsmount3rumntsize(NfsMount3RUmnt *x)
{
	uint a;
	USED(x);
	a = 0;
	return a;
}
int
nfsmount3rumntpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RUmnt *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
int
nfsmount3rumntunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RUmnt *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
void
nfsmount3tumntallprint(Fmt *fmt, NfsMount3TUmntall *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "NfsMount3TUmntall");
}
uint
nfsmount3tumntallsize(NfsMount3TUmntall *x)
{
	uint a;
	USED(x);
	a = 0;
	return a;
}
int
nfsmount3tumntallpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TUmntall *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
int
nfsmount3tumntallunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TUmntall *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
void
nfsmount3rumntallprint(Fmt *fmt, NfsMount3RUmntall *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "NfsMount3RUmntall");
}
uint
nfsmount3rumntallsize(NfsMount3RUmntall *x)
{
	uint a;
	USED(x);
	a = 0;
	return a;
}
int
nfsmount3rumntallpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RUmntall *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
int
nfsmount3rumntallunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RUmntall *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
void
nfsmount3texportprint(Fmt *fmt, NfsMount3TExport *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "NfsMount3TExport");
}
uint
nfsmount3texportsize(NfsMount3TExport *x)
{
	uint a;
	USED(x);
	a = 0;
	return a;
}
int
nfsmount3texportpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TExport *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
int
nfsmount3texportunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3TExport *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
void
nfsmount3rexportprint(Fmt *fmt, NfsMount3RExport *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "NfsMount3RExport");
	fmtprint(fmt, "\n");
}
uint
nfsmount3rexportsize(NfsMount3RExport *x)
{
	uint a;
	USED(x);
	a = 0;
	a += x->count;
	a += 4;	/* end of export list */
	return a;
}
int
nfsmount3rexportpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RExport *x)
{
	u1int zero;

	zero = 0;
	if(a+x->count > ea) goto Err;
	memmove(a, x->data, x->count);
	a += x->count;
	if(sunuint1pack(a, ea, &a, &zero) < 0)
		goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfsmount3rexportunpack(uchar *a, uchar *ea, uchar **pa, NfsMount3RExport *x)
{
	int ng, ne;
	uchar *oa;
	u1int u1;
	u32int u32;

	oa = a;
	ng = 0;
	for(ne=0;; ne++){
		if(sunuint1unpack(a, ea, &a, &u1) < 0)
			goto Err;
		if(u1 == 0)
			break;
		if(sunuint32unpack(a, ea, &a, &u32) < 0
		|| (a += (u32+3)&~3) >= ea)
			goto Err;
		for(;; ng++){
			if(sunuint1unpack(a, ea, &a, &u1) < 0)
				goto Err;
			if(u1 == 0)
				break;
			if(sunuint32unpack(a, ea, &a, &u32) < 0
			|| (a += (u32+3)&~3) >= ea)
				goto Err;
		}
	}
	x->data = oa;
	x->count = (a-4) - oa;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
uint
nfsmount3exportgroupsize(uchar *a)
{
	int ng;
	u1int have;
	u32int n;

	a += 4;
	sunuint32unpack(a, a+4, &a, &n);
	a += (n+3)&~3;
	ng = 0;
	for(;;){
		sunuint1unpack(a, a+4, &a, &have);
		if(have == 0)
			break;
		ng++;
		sunuint32unpack(a, a+4, &a, &n);
		a += (n+3)&~3;
	}
	return ng;
}
int
nfsmount3exportunpack(uchar *a, uchar *ea, uchar **pa, char **gp, char ***pgp, NfsMount3Export *x)
{
	int ng;
	u1int u1;

	if(sunuint1unpack(a, ea, &a, &u1) < 0 || u1 != 1) goto Err;
	if(sunstringunpack(a, ea, &a, &x->path, NfsMount3MaxPathSize) < 0) goto Err;
	x->g = gp;
	ng = 0;
	for(;;){
		if(sunuint1unpack(a, ea, &a, &u1) < 0) goto Err;
		if(u1 == 0)
			break;
		if(sunstringunpack(a, ea, &a, &gp[ng++], NfsMount3MaxNameSize) < 0) goto Err;
	}
	x->ng = ng;
	*pgp = gp+ng;
	*pa = a;
	return 0;

Err:
	*pa = ea;
	return -1;
}
uint
nfsmount3exportsize(NfsMount3Export *x)
{
	int i;
	uint a;

	a = 4 + sunstringsize(x->path);
	for(i=0; i<x->ng; i++)
		a += 4 + sunstringsize(x->g[i]);
	a += 4;
	return a;
}
int
nfsmount3exportpack(uchar *a, uchar *ea, uchar **pa, NfsMount3Export *x)
{
	int i;
	u1int u1;

	u1 = 1;
	if(sunuint1pack(a, ea, &a, &u1) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->path, NfsMount3MaxPathSize) < 0) goto Err;
	for(i=0; i<x->ng; i++){
		if(sunuint1pack(a, ea, &a, &u1) < 0) goto Err;
		if(sunstringpack(a, ea, &a, &x->g[i], NfsMount3MaxNameSize) < 0) goto Err;
	}
	u1 = 0;
	if(sunuint1pack(a, ea, &a, &u1) < 0) goto Err;
	*pa = a;
	return 0;

Err:
	*pa = ea;
	return -1;
}

typedef int (*P)(uchar*, uchar*, uchar**, SunCall*);
typedef void (*F)(Fmt*, SunCall*);
typedef uint (*S)(SunCall*);

static SunProc proc3[] = {
	(P)nfsmount3tnullpack, (P)nfsmount3tnullunpack, (S)nfsmount3tnullsize, (F)nfsmount3tnullprint, sizeof(NfsMount3TNull),
	(P)nfsmount3rnullpack, (P)nfsmount3rnullunpack, (S)nfsmount3rnullsize, (F)nfsmount3rnullprint, sizeof(NfsMount3RNull),
	(P)nfsmount3tmntpack, (P)nfsmount3tmntunpack, (S)nfsmount3tmntsize, (F)nfsmount3tmntprint, sizeof(NfsMount3TMnt),
	(P)nfsmount3rmntpack, (P)nfsmount3rmntunpack, (S)nfsmount3rmntsize, (F)nfsmount3rmntprint, sizeof(NfsMount3RMnt),
	(P)nfsmount3tdumppack, (P)nfsmount3tdumpunpack, (S)nfsmount3tdumpsize, (F)nfsmount3tdumpprint, sizeof(NfsMount3TDump),
	(P)nfsmount3rdumppack, (P)nfsmount3rdumpunpack, (S)nfsmount3rdumpsize, (F)nfsmount3rdumpprint, sizeof(NfsMount3RDump),
	(P)nfsmount3tumntpack, (P)nfsmount3tumntunpack, (S)nfsmount3tumntsize, (F)nfsmount3tumntprint, sizeof(NfsMount3TUmnt),
	(P)nfsmount3rumntpack, (P)nfsmount3rumntunpack, (S)nfsmount3rumntsize, (F)nfsmount3rumntprint, sizeof(NfsMount3RUmnt),
	(P)nfsmount3tumntallpack, (P)nfsmount3tumntallunpack, (S)nfsmount3tumntallsize, (F)nfsmount3tumntallprint, sizeof(NfsMount3TUmntall),
	(P)nfsmount3rumntallpack, (P)nfsmount3rumntallunpack, (S)nfsmount3rumntallsize, (F)nfsmount3rumntallprint, sizeof(NfsMount3RUmntall),
	(P)nfsmount3texportpack, (P)nfsmount3texportunpack, (S)nfsmount3texportsize, (F)nfsmount3texportprint, sizeof(NfsMount3TExport),
	(P)nfsmount3rexportpack, (P)nfsmount3rexportunpack, (S)nfsmount3rexportsize, (F)nfsmount3rexportprint, sizeof(NfsMount3RExport),
};

static SunProc proc1[] = {
	(P)nfsmount3tnullpack, (P)nfsmount3tnullunpack, (S)nfsmount3tnullsize, (F)nfsmount3tnullprint, sizeof(NfsMount3TNull),
	(P)nfsmount3rnullpack, (P)nfsmount3rnullunpack, (S)nfsmount3rnullsize, (F)nfsmount3rnullprint, sizeof(NfsMount3RNull),
	(P)nfsmount3tmntpack, (P)nfsmount3tmntunpack, (S)nfsmount3tmntsize, (F)nfsmount3tmntprint, sizeof(NfsMount3TMnt),
	(P)nfsmount1rmntpack, (P)nfsmount1rmntunpack, (S)nfsmount1rmntsize, (F)nfsmount3rmntprint, sizeof(NfsMount3RMnt),
	(P)nfsmount3tdumppack, (P)nfsmount3tdumpunpack, (S)nfsmount3tdumpsize, (F)nfsmount3tdumpprint, sizeof(NfsMount3TDump),
	(P)nfsmount3rdumppack, (P)nfsmount3rdumpunpack, (S)nfsmount3rdumpsize, (F)nfsmount3rdumpprint, sizeof(NfsMount3RDump),
	(P)nfsmount3tumntpack, (P)nfsmount3tumntunpack, (S)nfsmount3tumntsize, (F)nfsmount3tumntprint, sizeof(NfsMount3TUmnt),
	(P)nfsmount3rumntpack, (P)nfsmount3rumntunpack, (S)nfsmount3rumntsize, (F)nfsmount3rumntprint, sizeof(NfsMount3RUmnt),
	(P)nfsmount3tumntallpack, (P)nfsmount3tumntallunpack, (S)nfsmount3tumntallsize, (F)nfsmount3tumntallprint, sizeof(NfsMount3TUmntall),
	(P)nfsmount3rumntallpack, (P)nfsmount3rumntallunpack, (S)nfsmount3rumntallsize, (F)nfsmount3rumntallprint, sizeof(NfsMount3RUmntall),
	(P)nfsmount3texportpack, (P)nfsmount3texportunpack, (S)nfsmount3texportsize, (F)nfsmount3texportprint, sizeof(NfsMount3TExport),
	(P)nfsmount3rexportpack, (P)nfsmount3rexportunpack, (S)nfsmount3rexportsize, (F)nfsmount3rexportprint, sizeof(NfsMount3RExport),
};

SunProg nfsmount3prog =
{
	NfsMount3Program,
	NfsMount3Version,
	proc3,
	nelem(proc3),
};

SunProg nfsmount1prog =
{
	NfsMount1Program,
	NfsMount1Version,
	proc1,
	nelem(proc1),
};
