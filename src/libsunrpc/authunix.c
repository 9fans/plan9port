#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>

uint
sunauthunixsize(SunAuthUnix *x)
{
	return 4 + sunstringsize(x->sysname) + 4 + 4 + 4 + 4*x->ng;
}
int
sunauthunixunpack(uchar *a, uchar *ea, uchar **pa, SunAuthUnix *x)
{
	int i;

	if(sunuint32unpack(a, ea, &a, &x->stamp) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->sysname, 256) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->uid) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->gid) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->ng) < 0 || x->ng > nelem(x->g)) goto Err;
	for(i=0; i<x->ng; i++)
		if(sunuint32unpack(a, ea, &a, &x->g[i]) < 0) goto Err;

	*pa = a;
	return 0;

Err:
	*pa = ea;
	return -1;
}
int
sunauthunixpack(uchar *a, uchar *ea, uchar **pa, SunAuthUnix *x)
{
	int i;

	if(sunuint32pack(a, ea, &a, &x->stamp) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->sysname, 256) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->uid) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->gid) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->ng) < 0 || x->ng > nelem(x->g)) goto Err;
	for(i=0; i<x->ng; i++)
		if(sunuint32pack(a, ea, &a, &x->g[i]) < 0) goto Err;

	*pa = a;
	return 0;

Err:
	*pa = ea;
	return -1;
}
void
sunauthunixprint(Fmt *fmt, SunAuthUnix *x)
{
	int i;
	fmtprint(fmt, "unix %.8lux %s %lud %lud (", (ulong)x->stamp,
		x->sysname, (ulong)x->uid, (ulong)x->gid);
	for(i=0; i<x->ng; i++)
		fmtprint(fmt, "%s%lud", i ? " ":"", (ulong)x->g[i]);
	fmtprint(fmt, ")");
}
