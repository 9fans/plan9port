#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>

char*
nfs3statusstr(Nfs3Status x)
{
	switch(x){
	case Nfs3Ok:
		return "Nfs3Ok";
	case Nfs3ErrNotOwner:
		return "Nfs3ErrNotOwner";
	case Nfs3ErrNoEnt:
		return "Nfs3ErrNoEnt";
	case Nfs3ErrNoMem:
		return "Nfs3ErrNoMem";
	case Nfs3ErrIo:
		return "Nfs3ErrIo";
	case Nfs3ErrNxio:
		return "Nfs3ErrNxio";
	case Nfs3ErrAcces:
		return "Nfs3ErrAcces";
	case Nfs3ErrExist:
		return "Nfs3ErrExist";
	case Nfs3ErrXDev:
		return "Nfs3ErrXDev";
	case Nfs3ErrNoDev:
		return "Nfs3ErrNoDev";
	case Nfs3ErrNotDir:
		return "Nfs3ErrNotDir";
	case Nfs3ErrIsDir:
		return "Nfs3ErrIsDir";
	case Nfs3ErrInval:
		return "Nfs3ErrInval";
	case Nfs3ErrFbig:
		return "Nfs3ErrFbig";
	case Nfs3ErrNoSpc:
		return "Nfs3ErrNoSpc";
	case Nfs3ErrRoFs:
		return "Nfs3ErrRoFs";
	case Nfs3ErrMLink:
		return "Nfs3ErrMLink";
	case Nfs3ErrNameTooLong:
		return "Nfs3ErrNameTooLong";
	case Nfs3ErrNotEmpty:
		return "Nfs3ErrNotEmpty";
	case Nfs3ErrDQuot:
		return "Nfs3ErrDQuot";
	case Nfs3ErrStale:
		return "Nfs3ErrStale";
	case Nfs3ErrRemote:
		return "Nfs3ErrRemote";
	case Nfs3ErrBadHandle:
		return "Nfs3ErrBadHandle";
	case Nfs3ErrNotSync:
		return "Nfs3ErrNotSync";
	case Nfs3ErrBadCookie:
		return "Nfs3ErrBadCookie";
	case Nfs3ErrNotSupp:
		return "Nfs3ErrNotSupp";
	case Nfs3ErrTooSmall:
		return "Nfs3ErrTooSmall";
	case Nfs3ErrServerFault:
		return "Nfs3ErrServerFault";
	case Nfs3ErrBadType:
		return "Nfs3ErrBadType";
	case Nfs3ErrJukebox:
		return "Nfs3ErrJukebox";
	case Nfs3ErrFprintNotFound:
		return "Nfs3ErrFprintNotFound";
	case Nfs3ErrAborted:
		return "Nfs3ErrAborted";
	default:
		return "unknown";
	}
}

static struct {
	Nfs3Status status;
	char *msg;
} etab[] = {
	Nfs3ErrNotOwner,	"not owner",
	Nfs3ErrNoEnt,		"directory entry not found",
	Nfs3ErrIo,			"i/o error",
	Nfs3ErrNxio,		"no such device",
	Nfs3ErrNoMem,	"out of memory",
	Nfs3ErrAcces,		"access denied",
	Nfs3ErrExist,		"file or directory exists",
	Nfs3ErrXDev,		"cross-device operation",
	Nfs3ErrNoDev,		"no such device",
	Nfs3ErrNotDir,		"not a directory",
	Nfs3ErrIsDir,		"is a directory",
	Nfs3ErrInval,		"invalid arguments",
	Nfs3ErrFbig,		"file too big",
	Nfs3ErrNoSpc,		"no space left on device",
	Nfs3ErrRoFs,		"read-only file system",
	Nfs3ErrMLink,		"too many links",
	Nfs3ErrNameTooLong,	"name too long",
	Nfs3ErrNotEmpty,	"directory not empty",
	Nfs3ErrDQuot,		"dquot",
	Nfs3ErrStale,		"stale handle",
	Nfs3ErrRemote,	"remote error",
	Nfs3ErrBadHandle,	"bad handle",
	Nfs3ErrNotSync,	"out of sync with server",
	Nfs3ErrBadCookie,	"bad cookie",
	Nfs3ErrNotSupp,	"not supported",
	Nfs3ErrTooSmall,	"too small",
	Nfs3ErrServerFault,	"server fault",
	Nfs3ErrBadType,	"bad type",
	Nfs3ErrJukebox,	"jukebox -- try again later",
	Nfs3ErrFprintNotFound,	"fprint not found",
	Nfs3ErrAborted,	"aborted",
};

void
nfs3errstr(Nfs3Status status)
{
	int i;

	for(i=0; i<nelem(etab); i++){
		if((int)etab[i].status == (int)status){
			werrstr(etab[i].msg);
			return;
		}
	}
	werrstr("unknown nfs3 error %d", (int)status);
}

char*
nfs3filetypestr(Nfs3FileType x)
{
	switch(x){
	case Nfs3FileReg:
		return "Nfs3FileReg";
	case Nfs3FileDir:
		return "Nfs3FileDir";
	case Nfs3FileBlock:
		return "Nfs3FileBlock";
	case Nfs3FileChar:
		return "Nfs3FileChar";
	case Nfs3FileSymlink:
		return "Nfs3FileSymlink";
	case Nfs3FileSocket:
		return "Nfs3FileSocket";
	case Nfs3FileFifo:
		return "Nfs3FileFifo";
	default:
		return "unknown";
	}
}

void
nfs3handleprint(Fmt *fmt, Nfs3Handle *x)
{
	fmtprint(fmt, "%s\n", "Nfs3Handle");
	fmtprint(fmt, "\t%s=", "handle");
	if(x->len > 64)
		fmtprint(fmt, "%.*H... (%d)", 64, x->h, x->len);
	else
		fmtprint(fmt, "%.*H", x->len, x->h);
	fmtprint(fmt, "\n");
}
uint
nfs3handlesize(Nfs3Handle *x)
{
	uint a;
	USED(x);
	a = 0 + sunvaropaquesize(x->len);
	return a;
}
int
nfs3handlepack(uchar *a, uchar *ea, uchar **pa, Nfs3Handle *x)
{
	if(x->len > Nfs3MaxHandleSize || sunuint32pack(a, ea, &a, &x->len) < 0
	|| sunfixedopaquepack(a, ea, &a, x->h, x->len) < 0)
		goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3handleunpack(uchar *a, uchar *ea, uchar **pa, Nfs3Handle *x)
{
	uchar *ha;
	u32int n;

	if(sunuint32unpack(a, ea, &a, &n) < 0 || n > Nfs3MaxHandleSize)
		goto Err;
	ha = a;
	a += (n+3)&~3;
	if(a > ea)
		goto Err;
	memmove(x->h, ha, n);
	x->len = n;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3timeprint(Fmt *fmt, Nfs3Time *x)
{
	fmtprint(fmt, "%s\n", "Nfs3Time");
	fmtprint(fmt, "\t%s=", "sec");
	fmtprint(fmt, "%ud", x->sec);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "nsec");
	fmtprint(fmt, "%ud", x->nsec);
	fmtprint(fmt, "\n");
}
uint
nfs3timesize(Nfs3Time *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	return a;
}
int
nfs3timepack(uchar *a, uchar *ea, uchar **pa, Nfs3Time *x)
{
	if(sunuint32pack(a, ea, &a, &x->sec) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->nsec) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3timeunpack(uchar *a, uchar *ea, uchar **pa, Nfs3Time *x)
{
	if(sunuint32unpack(a, ea, &a, &x->sec) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->nsec) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3attrprint(Fmt *fmt, Nfs3Attr *x)
{
	fmtprint(fmt, "%s\n", "Nfs3Attr");
	fmtprint(fmt, "\t%s=", "type");
	fmtprint(fmt, "%s", nfs3filetypestr(x->type));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "mode");
	fmtprint(fmt, "%ud", x->mode);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "nlink");
	fmtprint(fmt, "%ud", x->nlink);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "uid");
	fmtprint(fmt, "%ud", x->uid);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "gid");
	fmtprint(fmt, "%ud", x->gid);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "size");
	fmtprint(fmt, "%llud", x->size);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "used");
	fmtprint(fmt, "%llud", x->used);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "major");
	fmtprint(fmt, "%ud", x->major);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "minor");
	fmtprint(fmt, "%ud", x->minor);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "fsid");
	fmtprint(fmt, "%llud", x->fsid);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "fileid");
	fmtprint(fmt, "%llud", x->fileid);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "atime");
	nfs3timeprint(fmt, &x->atime);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "mtime");
	nfs3timeprint(fmt, &x->mtime);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "ctime");
	nfs3timeprint(fmt, &x->ctime);
	fmtprint(fmt, "\n");
}
uint
nfs3attrsize(Nfs3Attr *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4 + 4 + 4 + 4 + 8 + 8 + 4 + 4 + 8 + 8 + nfs3timesize(&x->atime) + nfs3timesize(&x->mtime) + nfs3timesize(&x->ctime);
	return a;
}
int
nfs3attrpack(uchar *a, uchar *ea, uchar **pa, Nfs3Attr *x)
{
	int i;

	if(i=x->type, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->mode) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->nlink) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->uid) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->gid) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->size) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->used) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->major) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->minor) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->fsid) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->fileid) < 0) goto Err;
	if(nfs3timepack(a, ea, &a, &x->atime) < 0) goto Err;
	if(nfs3timepack(a, ea, &a, &x->mtime) < 0) goto Err;
	if(nfs3timepack(a, ea, &a, &x->ctime) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3attrunpack(uchar *a, uchar *ea, uchar **pa, Nfs3Attr *x)
{
	int i;
	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->type = i;
	if(sunuint32unpack(a, ea, &a, &x->mode) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->nlink) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->uid) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->gid) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->size) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->used) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->major) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->minor) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->fsid) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->fileid) < 0) goto Err;
	if(nfs3timeunpack(a, ea, &a, &x->atime) < 0) goto Err;
	if(nfs3timeunpack(a, ea, &a, &x->mtime) < 0) goto Err;
	if(nfs3timeunpack(a, ea, &a, &x->ctime) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3wccattrprint(Fmt *fmt, Nfs3WccAttr *x)
{
	fmtprint(fmt, "%s\n", "Nfs3WccAttr");
	fmtprint(fmt, "\t%s=", "size");
	fmtprint(fmt, "%llud", x->size);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "mtime");
	nfs3timeprint(fmt, &x->mtime);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "ctime");
	nfs3timeprint(fmt, &x->ctime);
	fmtprint(fmt, "\n");
}
uint
nfs3wccattrsize(Nfs3WccAttr *x)
{
	uint a;
	USED(x);
	a = 0 + 8 + nfs3timesize(&x->mtime) + nfs3timesize(&x->ctime);
	return a;
}
int
nfs3wccattrpack(uchar *a, uchar *ea, uchar **pa, Nfs3WccAttr *x)
{
	if(sunuint64pack(a, ea, &a, &x->size) < 0) goto Err;
	if(nfs3timepack(a, ea, &a, &x->mtime) < 0) goto Err;
	if(nfs3timepack(a, ea, &a, &x->ctime) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3wccattrunpack(uchar *a, uchar *ea, uchar **pa, Nfs3WccAttr *x)
{
	if(sunuint64unpack(a, ea, &a, &x->size) < 0) goto Err;
	if(nfs3timeunpack(a, ea, &a, &x->mtime) < 0) goto Err;
	if(nfs3timeunpack(a, ea, &a, &x->ctime) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3wccprint(Fmt *fmt, Nfs3Wcc *x)
{
	fmtprint(fmt, "%s\n", "Nfs3Wcc");
	fmtprint(fmt, "\t%s=", "haveWccAttr");
	fmtprint(fmt, "%d", x->haveWccAttr);
	fmtprint(fmt, "\n");
	switch(x->haveWccAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "wccAttr");
		nfs3wccattrprint(fmt, &x->wccAttr);
		fmtprint(fmt, "\n");
		break;
	}
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3wccsize(Nfs3Wcc *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->haveWccAttr){
	case 1:
		a = a + nfs3wccattrsize(&x->wccAttr);
		break;
	}
	a = a + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	return a;
}
int
nfs3wccpack(uchar *a, uchar *ea, uchar **pa, Nfs3Wcc *x)
{
	if(sunuint1pack(a, ea, &a, &x->haveWccAttr) < 0) goto Err;
	switch(x->haveWccAttr){
	case 1:
		if(nfs3wccattrpack(a, ea, &a, &x->wccAttr) < 0) goto Err;
		break;
	}
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3wccunpack(uchar *a, uchar *ea, uchar **pa, Nfs3Wcc *x)
{
	if(sunuint1unpack(a, ea, &a, &x->haveWccAttr) < 0) goto Err;
	switch(x->haveWccAttr){
	case 1:
		if(nfs3wccattrunpack(a, ea, &a, &x->wccAttr) < 0) goto Err;
		break;
	}
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
char*
nfs3settimestr(Nfs3SetTime x)
{
	switch(x){
	case Nfs3SetTimeDont:
		return "Nfs3SetTimeDont";
	case Nfs3SetTimeServer:
		return "Nfs3SetTimeServer";
	case Nfs3SetTimeClient:
		return "Nfs3SetTimeClient";
	default:
		return "unknown";
	}
}

void
nfs3setattrprint(Fmt *fmt, Nfs3SetAttr *x)
{
	fmtprint(fmt, "%s\n", "Nfs3SetAttr");
	fmtprint(fmt, "\t%s=", "setMode");
	fmtprint(fmt, "%d", x->setMode);
	fmtprint(fmt, "\n");
	switch(x->setMode){
	case 1:
		fmtprint(fmt, "\t%s=", "mode");
		fmtprint(fmt, "%ud", x->mode);
		fmtprint(fmt, "\n");
		break;
	}
	fmtprint(fmt, "\t%s=", "setUid");
	fmtprint(fmt, "%d", x->setUid);
	fmtprint(fmt, "\n");
	switch(x->setUid){
	case 1:
		fmtprint(fmt, "\t%s=", "uid");
		fmtprint(fmt, "%ud", x->uid);
		fmtprint(fmt, "\n");
		break;
	}
	fmtprint(fmt, "\t%s=", "setGid");
	fmtprint(fmt, "%d", x->setGid);
	fmtprint(fmt, "\n");
	switch(x->setGid){
	case 1:
		fmtprint(fmt, "\t%s=", "gid");
		fmtprint(fmt, "%ud", x->gid);
		fmtprint(fmt, "\n");
		break;
	}
	fmtprint(fmt, "\t%s=", "setSize");
	fmtprint(fmt, "%d", x->setSize);
	fmtprint(fmt, "\n");
	switch(x->setSize){
	case 1:
		fmtprint(fmt, "\t%s=", "size");
		fmtprint(fmt, "%llud", x->size);
		fmtprint(fmt, "\n");
		break;
	}
	fmtprint(fmt, "\t%s=", "setAtime");
	fmtprint(fmt, "%s", nfs3settimestr(x->setAtime));
	fmtprint(fmt, "\n");
	switch(x->setAtime){
	case Nfs3SetTimeClient:
		fmtprint(fmt, "\t%s=", "atime");
		nfs3timeprint(fmt, &x->atime);
		fmtprint(fmt, "\n");
		break;
	}
	fmtprint(fmt, "\t%s=", "setMtime");
	fmtprint(fmt, "%s", nfs3settimestr(x->setMtime));
	fmtprint(fmt, "\n");
	switch(x->setMtime){
	case Nfs3SetTimeClient:
		fmtprint(fmt, "\t%s=", "mtime");
		nfs3timeprint(fmt, &x->mtime);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3setattrsize(Nfs3SetAttr *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->setMode){
	case 1:
		a = a + 4;
		break;
	}
	a = a + 4;
	switch(x->setUid){
	case 1:
		a = a + 4;
		break;
	}
	a = a + 4;
	switch(x->setGid){
	case 1:
		a = a + 4;
		break;
	}
	a = a + 4;
	switch(x->setSize){
	case 1:
		a = a + 8;
		break;
	}
	a = a + 4;
	switch(x->setAtime){
	case Nfs3SetTimeClient:
		a = a + nfs3timesize(&x->atime);
		break;
	}
	a = a + 4;
	switch(x->setMtime){
	case Nfs3SetTimeClient:
		a = a + nfs3timesize(&x->mtime);
		break;
	}
	return a;
}
int
nfs3setattrpack(uchar *a, uchar *ea, uchar **pa, Nfs3SetAttr *x)
{
	int i;

	if(sunuint1pack(a, ea, &a, &x->setMode) < 0) goto Err;
	switch(x->setMode){
	case 1:
		if(sunuint32pack(a, ea, &a, &x->mode) < 0) goto Err;
		break;
	}
	if(sunuint1pack(a, ea, &a, &x->setUid) < 0) goto Err;
	switch(x->setUid){
	case 1:
		if(sunuint32pack(a, ea, &a, &x->uid) < 0) goto Err;
		break;
	}
	if(sunuint1pack(a, ea, &a, &x->setGid) < 0) goto Err;
	switch(x->setGid){
	case 1:
		if(sunuint32pack(a, ea, &a, &x->gid) < 0) goto Err;
		break;
	}
	if(sunuint1pack(a, ea, &a, &x->setSize) < 0) goto Err;
	switch(x->setSize){
	case 1:
		if(sunuint64pack(a, ea, &a, &x->size) < 0) goto Err;
		break;
	}
	if(i=x->setAtime, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->setAtime){
	case Nfs3SetTimeClient:
		if(nfs3timepack(a, ea, &a, &x->atime) < 0) goto Err;
		break;
	}
	if(i=x->setMtime, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->setMtime){
	case Nfs3SetTimeClient:
		if(nfs3timepack(a, ea, &a, &x->mtime) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3setattrunpack(uchar *a, uchar *ea, uchar **pa, Nfs3SetAttr *x)
{
	int i;

	if(sunuint1unpack(a, ea, &a, &x->setMode) < 0) goto Err;
	switch(x->setMode){
	case 1:
		if(sunuint32unpack(a, ea, &a, &x->mode) < 0) goto Err;
		break;
	}
	if(sunuint1unpack(a, ea, &a, &x->setUid) < 0) goto Err;
	switch(x->setUid){
	case 1:
		if(sunuint32unpack(a, ea, &a, &x->uid) < 0) goto Err;
		break;
	}
	if(sunuint1unpack(a, ea, &a, &x->setGid) < 0) goto Err;
	switch(x->setGid){
	case 1:
		if(sunuint32unpack(a, ea, &a, &x->gid) < 0) goto Err;
		break;
	}
	if(sunuint1unpack(a, ea, &a, &x->setSize) < 0) goto Err;
	switch(x->setSize){
	case 1:
		if(sunuint64unpack(a, ea, &a, &x->size) < 0) goto Err;
		break;
	}
	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->setAtime = i;
	switch(x->setAtime){
	case Nfs3SetTimeClient:
		if(nfs3timeunpack(a, ea, &a, &x->atime) < 0) goto Err;
		break;
	}
	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->setMtime = i;
	switch(x->setMtime){
	case Nfs3SetTimeClient:
		if(nfs3timeunpack(a, ea, &a, &x->mtime) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tnullprint(Fmt *fmt, Nfs3TNull *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "Nfs3TNull");
}
uint
nfs3tnullsize(Nfs3TNull *x)
{
	uint a;
	USED(x);
	a = 0;
	return a;
}
int
nfs3tnullpack(uchar *a, uchar *ea, uchar **pa, Nfs3TNull *x)
{
	USED(x);
	USED(ea);
	*pa = a;
	return 0;
}
int
nfs3tnullunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TNull *x)
{
	USED(x);
	USED(ea);
	*pa = a;
	return 0;
}
void
nfs3rnullprint(Fmt *fmt, Nfs3RNull *x)
{
	USED(x);
	fmtprint(fmt, "%s\n", "Nfs3RNull");
}
uint
nfs3rnullsize(Nfs3RNull *x)
{
	uint a;
	USED(x);
	a = 0;
	return a;
}
int
nfs3rnullpack(uchar *a, uchar *ea, uchar **pa, Nfs3RNull *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
int
nfs3rnullunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RNull *x)
{
	USED(ea);
	USED(x);
	*pa = a;
	return 0;
}
void
nfs3tgetattrprint(Fmt *fmt, Nfs3TGetattr *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TGetattr");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
}
uint
nfs3tgetattrsize(Nfs3TGetattr *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle);
	return a;
}
int
nfs3tgetattrpack(uchar *a, uchar *ea, uchar **pa, Nfs3TGetattr *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tgetattrunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TGetattr *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rgetattrprint(Fmt *fmt, Nfs3RGetattr *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RGetattr");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rgetattrsize(Nfs3RGetattr *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->status){
	case Nfs3Ok:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	return a;
}
int
nfs3rgetattrpack(uchar *a, uchar *ea, uchar **pa, Nfs3RGetattr *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rgetattrunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RGetattr *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	switch(x->status){
	case Nfs3Ok:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tsetattrprint(Fmt *fmt, Nfs3TSetattr *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TSetattr");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "attr");
	nfs3setattrprint(fmt, &x->attr);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "checkCtime");
	fmtprint(fmt, "%d", x->checkCtime);
	fmtprint(fmt, "\n");
	switch(x->checkCtime){
	case 1:
		fmtprint(fmt, "\t%s=", "ctime");
		nfs3timeprint(fmt, &x->ctime);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3tsetattrsize(Nfs3TSetattr *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + nfs3setattrsize(&x->attr) + 4;
	switch(x->checkCtime){
	case 1:
		a = a + nfs3timesize(&x->ctime);
		break;
	}
	return a;
}
int
nfs3tsetattrpack(uchar *a, uchar *ea, uchar **pa, Nfs3TSetattr *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(nfs3setattrpack(a, ea, &a, &x->attr) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->checkCtime) < 0) goto Err;
	switch(x->checkCtime){
	case 1:
		if(nfs3timepack(a, ea, &a, &x->ctime) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tsetattrunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TSetattr *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(nfs3setattrunpack(a, ea, &a, &x->attr) < 0) goto Err;
	if(sunuint1unpack(a, ea, &a, &x->checkCtime) < 0) goto Err;
	switch(x->checkCtime){
	case 1:
		if(nfs3timeunpack(a, ea, &a, &x->ctime) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rsetattrprint(Fmt *fmt, Nfs3RSetattr *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RSetattr");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "wcc");
	nfs3wccprint(fmt, &x->wcc);
	fmtprint(fmt, "\n");
}
uint
nfs3rsetattrsize(Nfs3RSetattr *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + nfs3wccsize(&x->wcc);
	return a;
}
int
nfs3rsetattrpack(uchar *a, uchar *ea, uchar **pa, Nfs3RSetattr *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(nfs3wccpack(a, ea, &a, &x->wcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rsetattrunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RSetattr *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(nfs3wccunpack(a, ea, &a, &x->wcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tlookupprint(Fmt *fmt, Nfs3TLookup *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TLookup");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->name);
	fmtprint(fmt, "\n");
}
uint
nfs3tlookupsize(Nfs3TLookup *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + sunstringsize(x->name);
	return a;
}
int
nfs3tlookuppack(uchar *a, uchar *ea, uchar **pa, Nfs3TLookup *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tlookupunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TLookup *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rlookupprint(Fmt *fmt, Nfs3RLookup *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RLookup");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "handle");
		nfs3handleprint(fmt, &x->handle);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "haveAttr");
		fmtprint(fmt, "%d", x->haveAttr);
		fmtprint(fmt, "\n");
		switch(x->haveAttr){
		case 1:
			fmtprint(fmt, "\t%s=", "attr");
			nfs3attrprint(fmt, &x->attr);
			fmtprint(fmt, "\n");
			break;
		}
		break;
	}
	fmtprint(fmt, "\t%s=", "haveDirAttr");
	fmtprint(fmt, "%d", x->haveDirAttr);
	fmtprint(fmt, "\n");
	switch(x->haveDirAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "dirAttr");
		nfs3attrprint(fmt, &x->dirAttr);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rlookupsize(Nfs3RLookup *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->status){
	case Nfs3Ok:
		a = a + nfs3handlesize(&x->handle) + 4;
		switch(x->haveAttr){
		case 1:
			a = a + nfs3attrsize(&x->attr);
			break;
		}
			break;
	}
	a = a + 4;
	switch(x->haveDirAttr){
	case 1:
		a = a + nfs3attrsize(&x->dirAttr);
		break;
	}
	return a;
}
int
nfs3rlookuppack(uchar *a, uchar *ea, uchar **pa, Nfs3RLookup *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
		if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(sunuint1pack(a, ea, &a, &x->haveDirAttr) < 0) goto Err;
	switch(x->haveDirAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->dirAttr) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rlookupunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RLookup *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	switch(x->status){
	case Nfs3Ok:
		if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
		if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(sunuint1unpack(a, ea, &a, &x->haveDirAttr) < 0) goto Err;
	switch(x->haveDirAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->dirAttr) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3taccessprint(Fmt *fmt, Nfs3TAccess *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TAccess");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "access");
	fmtprint(fmt, "%ud", x->access);
	fmtprint(fmt, "\n");
}
uint
nfs3taccesssize(Nfs3TAccess *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + 4;
	return a;
}
int
nfs3taccesspack(uchar *a, uchar *ea, uchar **pa, Nfs3TAccess *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->access) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3taccessunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TAccess *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->access) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3raccessprint(Fmt *fmt, Nfs3RAccess *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RAccess");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "access");
		fmtprint(fmt, "%ud", x->access);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3raccesssize(Nfs3RAccess *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		a = a + 4;
		break;
	}
	return a;
}
int
nfs3raccesspack(uchar *a, uchar *ea, uchar **pa, Nfs3RAccess *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32pack(a, ea, &a, &x->access) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3raccessunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RAccess *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32unpack(a, ea, &a, &x->access) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3treadlinkprint(Fmt *fmt, Nfs3TReadlink *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TReadlink");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
}
uint
nfs3treadlinksize(Nfs3TReadlink *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle);
	return a;
}
int
nfs3treadlinkpack(uchar *a, uchar *ea, uchar **pa, Nfs3TReadlink *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3treadlinkunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TReadlink *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rreadlinkprint(Fmt *fmt, Nfs3RReadlink *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RReadlink");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "data");
		fmtprint(fmt, "\"%s\"", x->data);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rreadlinksize(Nfs3RReadlink *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		a = a + sunstringsize(x->data);
		break;
	}
	return a;
}
int
nfs3rreadlinkpack(uchar *a, uchar *ea, uchar **pa, Nfs3RReadlink *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunstringpack(a, ea, &a, &x->data, -1) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rreadlinkunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RReadlink *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunstringunpack(a, ea, &a, &x->data, -1) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3treadprint(Fmt *fmt, Nfs3TRead *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TRead");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "offset");
	fmtprint(fmt, "%llud", x->offset);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "count");
	fmtprint(fmt, "%ud", x->count);
	fmtprint(fmt, "\n");
}
uint
nfs3treadsize(Nfs3TRead *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + 8 + 4;
	return a;
}
int
nfs3treadpack(uchar *a, uchar *ea, uchar **pa, Nfs3TRead *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->offset) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->count) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3treadunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TRead *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->offset) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->count) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rreadprint(Fmt *fmt, Nfs3RRead *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RRead");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "count");
		fmtprint(fmt, "%ud", x->count);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "eof");
		fmtprint(fmt, "%d", x->eof);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "data");
		if(x->ndata <= 32)
			fmtprint(fmt, "%.*H", x->ndata, x->data);
		else
			fmtprint(fmt, "%.32H...", x->data);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rreadsize(Nfs3RRead *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		a = a + 4 + 4 + sunvaropaquesize(x->ndata);
		break;
	}
	return a;
}
int
nfs3rreadpack(uchar *a, uchar *ea, uchar **pa, Nfs3RRead *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32pack(a, ea, &a, &x->count) < 0) goto Err;
		if(sunuint1pack(a, ea, &a, &x->eof) < 0) goto Err;
		if(sunvaropaquepack(a, ea, &a, &x->data, &x->ndata, x->count) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rreadunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RRead *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32unpack(a, ea, &a, &x->count) < 0) goto Err;
		if(sunuint1unpack(a, ea, &a, &x->eof) < 0) goto Err;
		if(sunvaropaqueunpack(a, ea, &a, &x->data, &x->ndata, x->count) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
char*
nfs3syncstr(Nfs3Sync x)
{
	switch(x){
	case Nfs3SyncNone:
		return "Nfs3SyncNone";
	case Nfs3SyncData:
		return "Nfs3SyncData";
	case Nfs3SyncFile:
		return "Nfs3SyncFile";
	default:
		return "unknown";
	}
}

void
nfs3twriteprint(Fmt *fmt, Nfs3TWrite *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TWrite");
	fmtprint(fmt, "\t%s=", "file");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "offset");
	fmtprint(fmt, "%llud", x->offset);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "count");
	fmtprint(fmt, "%ud", x->count);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "stable");
	fmtprint(fmt, "%s", nfs3syncstr(x->stable));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "data");
	if(x->ndata > 32)
		fmtprint(fmt, "%.32H... (%d)", x->data, x->ndata);
	else
		fmtprint(fmt, "%.*H", x->ndata, x->data);
	fmtprint(fmt, "\n");
}
uint
nfs3twritesize(Nfs3TWrite *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + 8 + 4 + 4 + sunvaropaquesize(x->ndata);
	return a;
}
int
nfs3twritepack(uchar *a, uchar *ea, uchar **pa, Nfs3TWrite *x)
{
	int i;

	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->offset) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->count) < 0) goto Err;
	if(i=x->stable, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunvaropaquepack(a, ea, &a, &x->data, &x->ndata, x->count) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3twriteunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TWrite *x)
{
	int i;

	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->offset) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->count) < 0) goto Err;
	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->stable = i;
	if(sunvaropaqueunpack(a, ea, &a, &x->data, &x->ndata, x->count) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rwriteprint(Fmt *fmt, Nfs3RWrite *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RWrite");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "wcc");
	nfs3wccprint(fmt, &x->wcc);
	fmtprint(fmt, "\n");
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "count");
		fmtprint(fmt, "%ud", x->count);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "committed");
		fmtprint(fmt, "%s", nfs3syncstr(x->committed));
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "verf");
		fmtprint(fmt, "%.*H", Nfs3WriteVerfSize, x->verf);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rwritesize(Nfs3RWrite *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + nfs3wccsize(&x->wcc);
	switch(x->status){
	case Nfs3Ok:
		a = a + 4 + 4 + Nfs3WriteVerfSize;
		break;
	}
	return a;
}
int
nfs3rwritepack(uchar *a, uchar *ea, uchar **pa, Nfs3RWrite *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(nfs3wccpack(a, ea, &a, &x->wcc) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32pack(a, ea, &a, &x->count) < 0) goto Err;
		if(i=x->committed, sunenumpack(a, ea, &a, &i) < 0) goto Err;
		if(sunfixedopaquepack(a, ea, &a, x->verf, Nfs3WriteVerfSize) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rwriteunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RWrite *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(nfs3wccunpack(a, ea, &a, &x->wcc) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32unpack(a, ea, &a, &x->count) < 0) goto Err;
		if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->committed = i;
		if(sunfixedopaqueunpack(a, ea, &a, x->verf, Nfs3WriteVerfSize) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
char*
nfs3createstr(Nfs3Create x)
{
	switch(x){
	case Nfs3CreateUnchecked:
		return "Nfs3CreateUnchecked";
	case Nfs3CreateGuarded:
		return "Nfs3CreateGuarded";
	case Nfs3CreateExclusive:
		return "Nfs3CreateExclusive";
	default:
		return "unknown";
	}
}

void
nfs3tcreateprint(Fmt *fmt, Nfs3TCreate *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TCreate");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->name);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "mode");
	fmtprint(fmt, "%s", nfs3createstr(x->mode));
	fmtprint(fmt, "\n");
	switch(x->mode){
	case Nfs3CreateUnchecked:
	case Nfs3CreateGuarded:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3setattrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	case Nfs3CreateExclusive:
		fmtprint(fmt, "\t%s=", "verf");
		fmtprint(fmt, "%.*H", Nfs3CreateVerfSize, x->verf);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3tcreatesize(Nfs3TCreate *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + sunstringsize(x->name) + 4;
	switch(x->mode){
	case Nfs3CreateUnchecked:
	case Nfs3CreateGuarded:
		a = a + nfs3setattrsize(&x->attr);
		break;
	case Nfs3CreateExclusive:
		a = a + Nfs3CreateVerfSize;
		break;
	}
	return a;
}
int
nfs3tcreatepack(uchar *a, uchar *ea, uchar **pa, Nfs3TCreate *x)
{
	int i;

	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(i=x->mode, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->mode){
	case Nfs3CreateUnchecked:
	case Nfs3CreateGuarded:
		if(nfs3setattrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	case Nfs3CreateExclusive:
		if(sunfixedopaquepack(a, ea, &a, x->verf, Nfs3CreateVerfSize) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tcreateunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TCreate *x)
{
	int i;

	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->mode = i;
	switch(x->mode){
	case Nfs3CreateUnchecked:
	case Nfs3CreateGuarded:
		if(nfs3setattrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	case Nfs3CreateExclusive:
		if(sunfixedopaqueunpack(a, ea, &a, x->verf, Nfs3CreateVerfSize) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rcreateprint(Fmt *fmt, Nfs3RCreate *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RCreate");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "haveHandle");
		fmtprint(fmt, "%d", x->haveHandle);
		fmtprint(fmt, "\n");
		switch(x->haveHandle){
		case 1:
			fmtprint(fmt, "\t%s=", "handle");
			nfs3handleprint(fmt, &x->handle);
			fmtprint(fmt, "\n");
			break;
		}
		fmtprint(fmt, "\t%s=", "haveAttr");
		fmtprint(fmt, "%d", x->haveAttr);
		fmtprint(fmt, "\n");
		switch(x->haveAttr){
		case 1:
			fmtprint(fmt, "\t%s=", "attr");
			nfs3attrprint(fmt, &x->attr);
			fmtprint(fmt, "\n");
			break;
		}
		break;
	}
	fmtprint(fmt, "\t%s=", "dirWcc");
	nfs3wccprint(fmt, &x->dirWcc);
	fmtprint(fmt, "\n");
}
uint
nfs3rcreatesize(Nfs3RCreate *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->status){
	case Nfs3Ok:
		a = a + 4;
		switch(x->haveHandle){
		case 1:
			a = a + nfs3handlesize(&x->handle);
			break;
		}
		a = a + 4;
		switch(x->haveAttr){
		case 1:
			a = a + nfs3attrsize(&x->attr);
			break;
		}
			break;
	}
	a = a + nfs3wccsize(&x->dirWcc);
	return a;
}
int
nfs3rcreatepack(uchar *a, uchar *ea, uchar **pa, Nfs3RCreate *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint1pack(a, ea, &a, &x->haveHandle) < 0) goto Err;
		switch(x->haveHandle){
		case 1:
			if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
			break;
		}
		if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(nfs3wccpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rcreateunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RCreate *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint1unpack(a, ea, &a, &x->haveHandle) < 0) goto Err;
		switch(x->haveHandle){
		case 1:
			if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
			break;
		}
		if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(nfs3wccunpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tmkdirprint(Fmt *fmt, Nfs3TMkdir *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TMkdir");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->name);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "attr");
	nfs3setattrprint(fmt, &x->attr);
	fmtprint(fmt, "\n");
}
uint
nfs3tmkdirsize(Nfs3TMkdir *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + sunstringsize(x->name) + nfs3setattrsize(&x->attr);
	return a;
}
int
nfs3tmkdirpack(uchar *a, uchar *ea, uchar **pa, Nfs3TMkdir *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(nfs3setattrpack(a, ea, &a, &x->attr) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tmkdirunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TMkdir *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(nfs3setattrunpack(a, ea, &a, &x->attr) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rmkdirprint(Fmt *fmt, Nfs3RMkdir *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RMkdir");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "haveHandle");
		fmtprint(fmt, "%d", x->haveHandle);
		fmtprint(fmt, "\n");
		switch(x->haveHandle){
		case 1:
			fmtprint(fmt, "\t%s=", "handle");
			nfs3handleprint(fmt, &x->handle);
			fmtprint(fmt, "\n");
			break;
		}
		fmtprint(fmt, "\t%s=", "haveAttr");
		fmtprint(fmt, "%d", x->haveAttr);
		fmtprint(fmt, "\n");
		switch(x->haveAttr){
		case 1:
			fmtprint(fmt, "\t%s=", "attr");
			nfs3attrprint(fmt, &x->attr);
			fmtprint(fmt, "\n");
			break;
		}
		break;
	}
	fmtprint(fmt, "\t%s=", "dirWcc");
	nfs3wccprint(fmt, &x->dirWcc);
	fmtprint(fmt, "\n");
}
uint
nfs3rmkdirsize(Nfs3RMkdir *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->status){
	case Nfs3Ok:
		a = a + 4;
		switch(x->haveHandle){
		case 1:
			a = a + nfs3handlesize(&x->handle);
			break;
		}
		a = a + 4;
		switch(x->haveAttr){
		case 1:
			a = a + nfs3attrsize(&x->attr);
			break;
		}
			break;
	}
	a = a + nfs3wccsize(&x->dirWcc);
	return a;
}
int
nfs3rmkdirpack(uchar *a, uchar *ea, uchar **pa, Nfs3RMkdir *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint1pack(a, ea, &a, &x->haveHandle) < 0) goto Err;
		switch(x->haveHandle){
		case 1:
			if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
			break;
		}
		if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(nfs3wccpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rmkdirunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RMkdir *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint1unpack(a, ea, &a, &x->haveHandle) < 0) goto Err;
		switch(x->haveHandle){
		case 1:
			if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
			break;
		}
		if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(nfs3wccunpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tsymlinkprint(Fmt *fmt, Nfs3TSymlink *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TSymlink");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->name);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "attr");
	nfs3setattrprint(fmt, &x->attr);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "data");
	fmtprint(fmt, "\"%s\"", x->data);
	fmtprint(fmt, "\n");
}
uint
nfs3tsymlinksize(Nfs3TSymlink *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + sunstringsize(x->name) + nfs3setattrsize(&x->attr) + sunstringsize(x->data);
	return a;
}
int
nfs3tsymlinkpack(uchar *a, uchar *ea, uchar **pa, Nfs3TSymlink *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(nfs3setattrpack(a, ea, &a, &x->attr) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->data, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tsymlinkunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TSymlink *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(nfs3setattrunpack(a, ea, &a, &x->attr) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->data, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rsymlinkprint(Fmt *fmt, Nfs3RSymlink *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RSymlink");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "haveHandle");
		fmtprint(fmt, "%d", x->haveHandle);
		fmtprint(fmt, "\n");
		switch(x->haveHandle){
		case 1:
			fmtprint(fmt, "\t%s=", "handle");
			nfs3handleprint(fmt, &x->handle);
			fmtprint(fmt, "\n");
			break;
		}
		fmtprint(fmt, "\t%s=", "haveAttr");
		fmtprint(fmt, "%d", x->haveAttr);
		fmtprint(fmt, "\n");
		switch(x->haveAttr){
		case 1:
			fmtprint(fmt, "\t%s=", "attr");
			nfs3attrprint(fmt, &x->attr);
			fmtprint(fmt, "\n");
			break;
		}
		break;
	}
	fmtprint(fmt, "\t%s=", "dirWcc");
	nfs3wccprint(fmt, &x->dirWcc);
	fmtprint(fmt, "\n");
}
uint
nfs3rsymlinksize(Nfs3RSymlink *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->status){
	case Nfs3Ok:
		a = a + 4;
		switch(x->haveHandle){
		case 1:
			a = a + nfs3handlesize(&x->handle);
			break;
		}
		a = a + 4;
		switch(x->haveAttr){
		case 1:
			a = a + nfs3attrsize(&x->attr);
			break;
		}
			break;
	}
	a = a + nfs3wccsize(&x->dirWcc);
	return a;
}
int
nfs3rsymlinkpack(uchar *a, uchar *ea, uchar **pa, Nfs3RSymlink *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint1pack(a, ea, &a, &x->haveHandle) < 0) goto Err;
		switch(x->haveHandle){
		case 1:
			if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
			break;
		}
		if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(nfs3wccpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rsymlinkunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RSymlink *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint1unpack(a, ea, &a, &x->haveHandle) < 0) goto Err;
		switch(x->haveHandle){
		case 1:
			if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
			break;
		}
		if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(nfs3wccunpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tmknodprint(Fmt *fmt, Nfs3TMknod *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TMknod");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->name);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "type");
	fmtprint(fmt, "%s", nfs3filetypestr(x->type));
	fmtprint(fmt, "\n");
	switch(x->type){
	case Nfs3FileChar:
	case Nfs3FileBlock:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3setattrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "major");
		fmtprint(fmt, "%ud", x->major);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "minor");
		fmtprint(fmt, "%ud", x->minor);
		fmtprint(fmt, "\n");
		break;
	case Nfs3FileSocket:
	case Nfs3FileFifo:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3setattrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3tmknodsize(Nfs3TMknod *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + sunstringsize(x->name) + 4;
	switch(x->type){
	case Nfs3FileChar:
	case Nfs3FileBlock:
		a = a + nfs3setattrsize(&x->attr) + 4 + 4;
		break;
	case Nfs3FileSocket:
	case Nfs3FileFifo:
		a = a + nfs3setattrsize(&x->attr);
		break;
	}
	return a;
}
int
nfs3tmknodpack(uchar *a, uchar *ea, uchar **pa, Nfs3TMknod *x)
{
	int i;

	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(i=x->type, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->type){
	case Nfs3FileChar:
	case Nfs3FileBlock:
		if(nfs3setattrpack(a, ea, &a, &x->attr) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->major) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->minor) < 0) goto Err;
		break;
	case Nfs3FileSocket:
	case Nfs3FileFifo:
		if(nfs3setattrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tmknodunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TMknod *x)
{
	int i;

	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->type = i;
	switch(x->type){
	case Nfs3FileChar:
	case Nfs3FileBlock:
		if(nfs3setattrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->major) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->minor) < 0) goto Err;
		break;
	case Nfs3FileSocket:
	case Nfs3FileFifo:
		if(nfs3setattrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rmknodprint(Fmt *fmt, Nfs3RMknod *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RMknod");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "haveHandle");
		fmtprint(fmt, "%d", x->haveHandle);
		fmtprint(fmt, "\n");
		switch(x->haveHandle){
		case 1:
			fmtprint(fmt, "\t%s=", "handle");
			nfs3handleprint(fmt, &x->handle);
			fmtprint(fmt, "\n");
			break;
		}
		fmtprint(fmt, "\t%s=", "haveAttr");
		fmtprint(fmt, "%d", x->haveAttr);
		fmtprint(fmt, "\n");
		switch(x->haveAttr){
		case 1:
			fmtprint(fmt, "\t%s=", "attr");
			nfs3attrprint(fmt, &x->attr);
			fmtprint(fmt, "\n");
			break;
		}
		break;
	}
	fmtprint(fmt, "\t%s=", "dirWcc");
	nfs3wccprint(fmt, &x->dirWcc);
	fmtprint(fmt, "\n");
}
uint
nfs3rmknodsize(Nfs3RMknod *x)
{
	uint a;
	USED(x);
	a = 0 + 4;
	switch(x->status){
	case Nfs3Ok:
		a = a + 4;
		switch(x->haveHandle){
		case 1:
			a = a + nfs3handlesize(&x->handle);
			break;
		}
		a = a + 4;
		switch(x->haveAttr){
		case 1:
			a = a + nfs3attrsize(&x->attr);
			break;
		}
			break;
	}
	a = a + nfs3wccsize(&x->dirWcc);
	return a;
}
int
nfs3rmknodpack(uchar *a, uchar *ea, uchar **pa, Nfs3RMknod *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint1pack(a, ea, &a, &x->haveHandle) < 0) goto Err;
		switch(x->haveHandle){
		case 1:
			if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
			break;
		}
		if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(nfs3wccpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rmknodunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RMknod *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	switch(x->status){
	case Nfs3Ok:
		if(sunuint1unpack(a, ea, &a, &x->haveHandle) < 0) goto Err;
		switch(x->haveHandle){
		case 1:
			if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
			break;
		}
		if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
		switch(x->haveAttr){
		case 1:
			if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
			break;
		}
		break;
	}
	if(nfs3wccunpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tremoveprint(Fmt *fmt, Nfs3TRemove *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TRemove");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->name);
	fmtprint(fmt, "\n");
}
uint
nfs3tremovesize(Nfs3TRemove *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + sunstringsize(x->name);
	return a;
}
int
nfs3tremovepack(uchar *a, uchar *ea, uchar **pa, Nfs3TRemove *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tremoveunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TRemove *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rremoveprint(Fmt *fmt, Nfs3RRemove *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RRemove");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "wcc");
	nfs3wccprint(fmt, &x->wcc);
	fmtprint(fmt, "\n");
}
uint
nfs3rremovesize(Nfs3RRemove *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + nfs3wccsize(&x->wcc);
	return a;
}
int
nfs3rremovepack(uchar *a, uchar *ea, uchar **pa, Nfs3RRemove *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(nfs3wccpack(a, ea, &a, &x->wcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rremoveunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RRemove *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(nfs3wccunpack(a, ea, &a, &x->wcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3trmdirprint(Fmt *fmt, Nfs3TRmdir *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TRmdir");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->name);
	fmtprint(fmt, "\n");
}
uint
nfs3trmdirsize(Nfs3TRmdir *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + sunstringsize(x->name);
	return a;
}
int
nfs3trmdirpack(uchar *a, uchar *ea, uchar **pa, Nfs3TRmdir *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3trmdirunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TRmdir *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rrmdirprint(Fmt *fmt, Nfs3RRmdir *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RRmdir");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "wcc");
	nfs3wccprint(fmt, &x->wcc);
	fmtprint(fmt, "\n");
}
uint
nfs3rrmdirsize(Nfs3RRmdir *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + nfs3wccsize(&x->wcc);
	return a;
}
int
nfs3rrmdirpack(uchar *a, uchar *ea, uchar **pa, Nfs3RRmdir *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(nfs3wccpack(a, ea, &a, &x->wcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rrmdirunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RRmdir *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(nfs3wccunpack(a, ea, &a, &x->wcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3trenameprint(Fmt *fmt, Nfs3TRename *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TRename");
	fmtprint(fmt, "\t%s=", "from");
	fmtprint(fmt, "{\n");
	fmtprint(fmt, "\t\t%s=", "handle");
	nfs3handleprint(fmt, &x->from.handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->from.name);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t}");
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "to");
	fmtprint(fmt, "{\n");
	fmtprint(fmt, "\t\t%s=", "handle");
	nfs3handleprint(fmt, &x->to.handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->to.name);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t}");
	fmtprint(fmt, "\n");
}
uint
nfs3trenamesize(Nfs3TRename *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->from.handle) + sunstringsize(x->from.name) + nfs3handlesize(&x->to.handle) + sunstringsize(x->to.name);
	return a;
}
int
nfs3trenamepack(uchar *a, uchar *ea, uchar **pa, Nfs3TRename *x)
{
	if(nfs3handlepack(a, ea, &a, &x->from.handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->from.name, -1) < 0) goto Err;
	if(nfs3handlepack(a, ea, &a, &x->to.handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->to.name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3trenameunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TRename *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->from.handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->from.name, -1) < 0) goto Err;
	if(nfs3handleunpack(a, ea, &a, &x->to.handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->to.name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rrenameprint(Fmt *fmt, Nfs3RRename *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RRename");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "fromWcc");
	nfs3wccprint(fmt, &x->fromWcc);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "toWcc");
	nfs3wccprint(fmt, &x->toWcc);
	fmtprint(fmt, "\n");
}
uint
nfs3rrenamesize(Nfs3RRename *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + nfs3wccsize(&x->fromWcc) + nfs3wccsize(&x->toWcc);
	return a;
}
int
nfs3rrenamepack(uchar *a, uchar *ea, uchar **pa, Nfs3RRename *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(nfs3wccpack(a, ea, &a, &x->fromWcc) < 0) goto Err;
	if(nfs3wccpack(a, ea, &a, &x->toWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rrenameunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RRename *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(nfs3wccunpack(a, ea, &a, &x->fromWcc) < 0) goto Err;
	if(nfs3wccunpack(a, ea, &a, &x->toWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tlinkprint(Fmt *fmt, Nfs3TLink *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TLink");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "link");
	fmtprint(fmt, "{\n");
	fmtprint(fmt, "\t\t%s=", "handle");
	nfs3handleprint(fmt, &x->link.handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->link.name);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t}");
	fmtprint(fmt, "\n");
}
uint
nfs3tlinksize(Nfs3TLink *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + nfs3handlesize(&x->link.handle) + sunstringsize(x->link.name);
	return a;
}
int
nfs3tlinkpack(uchar *a, uchar *ea, uchar **pa, Nfs3TLink *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(nfs3handlepack(a, ea, &a, &x->link.handle) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->link.name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tlinkunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TLink *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(nfs3handleunpack(a, ea, &a, &x->link.handle) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->link.name, -1) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rlinkprint(Fmt *fmt, Nfs3RLink *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RLink");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	fmtprint(fmt, "\t%s=", "dirWcc");
	nfs3wccprint(fmt, &x->dirWcc);
	fmtprint(fmt, "\n");
}
uint
nfs3rlinksize(Nfs3RLink *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	a = a + nfs3wccsize(&x->dirWcc);
	return a;
}
int
nfs3rlinkpack(uchar *a, uchar *ea, uchar **pa, Nfs3RLink *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	if(nfs3wccpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rlinkunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RLink *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	if(nfs3wccunpack(a, ea, &a, &x->dirWcc) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3treaddirprint(Fmt *fmt, Nfs3TReadDir *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TReadDir");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "cookie");
	fmtprint(fmt, "%llud", x->cookie);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "verf");
	fmtprint(fmt, "%.*H", Nfs3CookieVerfSize, x->verf);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "count");
	fmtprint(fmt, "%ud", x->count);
	fmtprint(fmt, "\n");
}
uint
nfs3treaddirsize(Nfs3TReadDir *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + 8 + Nfs3CookieVerfSize + 4;
	return a;
}
int
nfs3treaddirpack(uchar *a, uchar *ea, uchar **pa, Nfs3TReadDir *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->cookie) < 0) goto Err;
	if(sunfixedopaquepack(a, ea, &a, x->verf, Nfs3CookieVerfSize) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->count) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3treaddirunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TReadDir *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->cookie) < 0) goto Err;
	if(sunfixedopaqueunpack(a, ea, &a, x->verf, Nfs3CookieVerfSize) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->count) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3entryprint(Fmt *fmt, Nfs3Entry *x)
{
	fmtprint(fmt, "%s\n", "Nfs3Entry");
	fmtprint(fmt, "\t%s=", "fileid");
	fmtprint(fmt, "%llud", x->fileid);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->name);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "cookie");
	fmtprint(fmt, "%llud", x->cookie);
	fmtprint(fmt, "\n");
}
uint
nfs3entrysize(Nfs3Entry *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 8 + sunstringsize(x->name) + 8;
	return a;
}
static int
sunstringvpack(uchar *a, uchar *ea, uchar **pa, char **s, u32int n)
{
	return sunvaropaquepack(a, ea, pa, (uchar**)(void*)s, &n, -1);
}
int
nfs3entrypack(uchar *a, uchar *ea, uchar **pa, Nfs3Entry *x)
{
	u1int one;

	one = 1;
	if(sunuint1pack(a, ea, &a, &one) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->fileid) < 0) goto Err;
	if(sunstringvpack(a, ea, &a, &x->name, x->namelen) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->cookie) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3entryunpack(uchar *a, uchar *ea, uchar **pa, Nfs3Entry *x)
{
	u1int one;

	memset(x, 0, sizeof *x);
	if(sunuint1unpack(a, ea, &a, &one) < 0 || one != 1) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->fileid) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	x->namelen = strlen(x->name);
	if(sunuint64unpack(a, ea, &a, &x->cookie) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rreaddirprint(Fmt *fmt, Nfs3RReadDir *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RReadDir");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "verf");
		fmtprint(fmt, "%.*H", Nfs3CookieVerfSize, x->verf);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=%ud\n", "count", x->count);
		fmtprint(fmt, "\t%s=", "eof");
		fmtprint(fmt, "%d", x->eof);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rreaddirsize(Nfs3RReadDir *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		a = a + Nfs3CookieVerfSize;
		a += x->count;
		a += 4 + 4;
		break;
	}
	return a;
}
int
nfs3rreaddirpack(uchar *a, uchar *ea, uchar **pa, Nfs3RReadDir *x)
{
	int i;
	u1int zero;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunfixedopaquepack(a, ea, &a, x->verf, Nfs3CookieVerfSize) < 0) goto Err;
		if(sunfixedopaquepack(a, ea, &a, x->data, x->count) < 0) goto Err;
		zero = 0;
		if(sunuint1pack(a, ea, &a, &zero) < 0) goto Err;
		if(sunuint1pack(a, ea, &a, &x->eof) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
static int
countEntry(uchar *a, uchar *ea, uchar **pa, u32int *n)
{
	uchar *oa;
	u64int u64;
	u32int u32;
	u1int u1;

	oa = a;
	for(;;){
		if(sunuint1unpack(a, ea, &a, &u1) < 0)
			return -1;
		if(u1 == 0)
			break;
		if(sunuint64unpack(a, ea, &a, &u64) < 0
		|| sunuint32unpack(a, ea, &a, &u32) < 0)
			return -1;
		a += (u32+3)&~3;
		if(a >= ea)
			return -1;
		if(sunuint64unpack(a, ea, &a, &u64) < 0)
			return -1;
	}
	*n = (a-4) - oa;
	*pa = a;
	return 0;
}
int
nfs3rreaddirunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RReadDir *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	if(x->status == Nfs3Ok){
		if(sunfixedopaqueunpack(a, ea, &a, x->verf, Nfs3CookieVerfSize) < 0) goto Err;
		x->data = a;
		if(countEntry(a, ea, &a, &x->count) < 0) goto Err;
		if(sunuint1unpack(a, ea, &a, &x->eof) < 0) goto Err;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3treaddirplusprint(Fmt *fmt, Nfs3TReadDirPlus *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TReadDirPlus");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "cookie");
	fmtprint(fmt, "%llud", x->cookie);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "verf");
	fmtprint(fmt, "%.*H", Nfs3CookieVerfSize, x->verf);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "dirCount");
	fmtprint(fmt, "%ud", x->dirCount);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "maxCount");
	fmtprint(fmt, "%ud", x->maxCount);
	fmtprint(fmt, "\n");
}
uint
nfs3treaddirplussize(Nfs3TReadDirPlus *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + 8 + Nfs3CookieVerfSize + 4 + 4;
	return a;
}
int
nfs3treaddirpluspack(uchar *a, uchar *ea, uchar **pa, Nfs3TReadDirPlus *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->cookie) < 0) goto Err;
	if(sunfixedopaquepack(a, ea, &a, x->verf, Nfs3CookieVerfSize) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->dirCount) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->maxCount) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3treaddirplusunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TReadDirPlus *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->cookie) < 0) goto Err;
	if(sunfixedopaqueunpack(a, ea, &a, x->verf, Nfs3CookieVerfSize) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->dirCount) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->maxCount) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3entryplusprint(Fmt *fmt, Nfs3Entry *x)
{
	fmtprint(fmt, "%s\n", "Nfs3EntryPlus");
	fmtprint(fmt, "\t%s=", "fileid");
	fmtprint(fmt, "%llud", x->fileid);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "name");
	fmtprint(fmt, "\"%s\"", x->name);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "cookie");
	fmtprint(fmt, "%llud", x->cookie);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	fmtprint(fmt, "\t%s=", "haveHandle");
	fmtprint(fmt, "%d", x->haveHandle);
	fmtprint(fmt, "\n");
	switch(x->haveHandle){
	case 1:
		fmtprint(fmt, "\t%s=", "handle");
		nfs3handleprint(fmt, &x->handle);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3entryplussize(Nfs3Entry *x)
{
	uint a;
	USED(x);
	a = 0 + 8 + sunstringsize(x->name) + 8 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	a = a + 4;
	switch(x->haveHandle){
	case 1:
		a = a + nfs3handlesize(&x->handle);
		break;
	}
	return a;
}
int
nfs3entrypluspack(uchar *a, uchar *ea, uchar **pa, Nfs3Entry *x)
{
	u1int u1;

	if(sunuint1pack(a, ea, &a, &u1) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->fileid) < 0) goto Err;
	if(sunstringpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->cookie) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	if(sunuint1pack(a, ea, &a, &x->haveHandle) < 0) goto Err;
	switch(x->haveHandle){
	case 1:
		if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3entryplusunpack(uchar *a, uchar *ea, uchar **pa, Nfs3Entry *x)
{
	u1int u1;

	if(sunuint1unpack(a, ea, &a, &u1) < 0 || u1 != 1) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->fileid) < 0) goto Err;
	if(sunstringunpack(a, ea, &a, &x->name, -1) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->cookie) < 0) goto Err;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	if(sunuint1unpack(a, ea, &a, &x->haveHandle) < 0) goto Err;
	switch(x->haveHandle){
	case 1:
		if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rreaddirplusprint(Fmt *fmt, Nfs3RReadDirPlus *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RReadDirPlus");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "verf");
		fmtprint(fmt, "%.*H", Nfs3CookieVerfSize, x->verf);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\tcount=%ud\n", x->count);
		fmtprint(fmt, "\t%s=", "eof");
		fmtprint(fmt, "%d", x->eof);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rreaddirplussize(Nfs3RReadDirPlus *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		a = a + Nfs3CookieVerfSize;
		a += x->count;
		a += 4 + 4;
		break;
	}
	return a;
}
int
nfs3rreaddirpluspack(uchar *a, uchar *ea, uchar **pa, Nfs3RReadDirPlus *x)
{
	int i;
	u1int zero;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunfixedopaquepack(a, ea, &a, x->verf, Nfs3CookieVerfSize) < 0) goto Err;
		if(sunfixedopaquepack(a, ea, &a, x->data, x->count) < 0) goto Err;
		zero = 0;
		if(sunuint1pack(a, ea, &a, &zero) < 0) goto Err;
		if(sunuint1pack(a, ea, &a, &x->eof) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
static int
countEntryPlus(uchar *a, uchar *ea, uchar **pa, u32int *n)
{
	uchar *oa;
	u64int u64;
	u32int u32;
	u1int u1;
	Nfs3Handle h;
	Nfs3Attr attr;

	oa = a;
	for(;;){
		if(sunuint1unpack(a, ea, &a, &u1) < 0)
			return -1;
		if(u1 == 0)
			break;
		if(sunuint64unpack(a, ea, &a, &u64) < 0
		|| sunuint32unpack(a, ea, &a, &u32) < 0)
			return -1;
		a += (u32+3)&~3;
		if(a >= ea)
			return -1;
		if(sunuint64unpack(a, ea, &a, &u64) < 0
		|| sunuint1unpack(a, ea, &a, &u1) < 0
		|| (u1 && nfs3attrunpack(a, ea, &a, &attr) < 0)
		|| sunuint1unpack(a, ea, &a, &u1) < 0
		|| (u1 && nfs3handleunpack(a, ea, &a, &h) < 0))
			return -1;
	}
	*n = (a-4) - oa;
	*pa = a;
	return 0;
}

int
nfs3rreaddirplusunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RReadDirPlus *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	if(x->status == Nfs3Ok){
		if(sunfixedopaqueunpack(a, ea, &a, x->verf, Nfs3CookieVerfSize) < 0) goto Err;
		x->data = a;
		if(countEntryPlus(a, ea, &a, &x->count) < 0) goto Err;
		if(sunuint1unpack(a, ea, &a, &x->eof) < 0) goto Err;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tfsstatprint(Fmt *fmt, Nfs3TFsStat *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TFsStat");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
}
uint
nfs3tfsstatsize(Nfs3TFsStat *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle);
	return a;
}
int
nfs3tfsstatpack(uchar *a, uchar *ea, uchar **pa, Nfs3TFsStat *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tfsstatunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TFsStat *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rfsstatprint(Fmt *fmt, Nfs3RFsStat *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RFsStat");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "totalBytes");
		fmtprint(fmt, "%llud", x->totalBytes);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "freeBytes");
		fmtprint(fmt, "%llud", x->freeBytes);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "availBytes");
		fmtprint(fmt, "%llud", x->availBytes);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "totalFiles");
		fmtprint(fmt, "%llud", x->totalFiles);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "freeFiles");
		fmtprint(fmt, "%llud", x->freeFiles);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "availFiles");
		fmtprint(fmt, "%llud", x->availFiles);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "invarSec");
		fmtprint(fmt, "%ud", x->invarSec);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rfsstatsize(Nfs3RFsStat *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		a = a + 8 + 8 + 8 + 8 + 8 + 8 + 4;
		break;
	}
	return a;
}
int
nfs3rfsstatpack(uchar *a, uchar *ea, uchar **pa, Nfs3RFsStat *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint64pack(a, ea, &a, &x->totalBytes) < 0) goto Err;
		if(sunuint64pack(a, ea, &a, &x->freeBytes) < 0) goto Err;
		if(sunuint64pack(a, ea, &a, &x->availBytes) < 0) goto Err;
		if(sunuint64pack(a, ea, &a, &x->totalFiles) < 0) goto Err;
		if(sunuint64pack(a, ea, &a, &x->freeFiles) < 0) goto Err;
		if(sunuint64pack(a, ea, &a, &x->availFiles) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->invarSec) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rfsstatunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RFsStat *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint64unpack(a, ea, &a, &x->totalBytes) < 0) goto Err;
		if(sunuint64unpack(a, ea, &a, &x->freeBytes) < 0) goto Err;
		if(sunuint64unpack(a, ea, &a, &x->availBytes) < 0) goto Err;
		if(sunuint64unpack(a, ea, &a, &x->totalFiles) < 0) goto Err;
		if(sunuint64unpack(a, ea, &a, &x->freeFiles) < 0) goto Err;
		if(sunuint64unpack(a, ea, &a, &x->availFiles) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->invarSec) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tfsinfoprint(Fmt *fmt, Nfs3TFsInfo *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TFsInfo");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
}
uint
nfs3tfsinfosize(Nfs3TFsInfo *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle);
	return a;
}
int
nfs3tfsinfopack(uchar *a, uchar *ea, uchar **pa, Nfs3TFsInfo *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tfsinfounpack(uchar *a, uchar *ea, uchar **pa, Nfs3TFsInfo *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rfsinfoprint(Fmt *fmt, Nfs3RFsInfo *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RFsInfo");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "readMax");
		fmtprint(fmt, "%ud", x->readMax);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "readPref");
		fmtprint(fmt, "%ud", x->readPref);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "readMult");
		fmtprint(fmt, "%ud", x->readMult);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "writeMax");
		fmtprint(fmt, "%ud", x->writeMax);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "writePref");
		fmtprint(fmt, "%ud", x->writePref);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "writeMult");
		fmtprint(fmt, "%ud", x->writeMult);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "readDirPref");
		fmtprint(fmt, "%ud", x->readDirPref);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "maxFileSize");
		fmtprint(fmt, "%llud", x->maxFileSize);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "timePrec");
		nfs3timeprint(fmt, &x->timePrec);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "flags");
		fmtprint(fmt, "%ud", x->flags);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rfsinfosize(Nfs3RFsInfo *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		a = a + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 8 + nfs3timesize(&x->timePrec) + 4;
		break;
	}
	return a;
}
int
nfs3rfsinfopack(uchar *a, uchar *ea, uchar **pa, Nfs3RFsInfo *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32pack(a, ea, &a, &x->readMax) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->readPref) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->readMult) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->writeMax) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->writePref) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->writeMult) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->readDirPref) < 0) goto Err;
		if(sunuint64pack(a, ea, &a, &x->maxFileSize) < 0) goto Err;
		if(nfs3timepack(a, ea, &a, &x->timePrec) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->flags) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rfsinfounpack(uchar *a, uchar *ea, uchar **pa, Nfs3RFsInfo *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32unpack(a, ea, &a, &x->readMax) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->readPref) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->readMult) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->writeMax) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->writePref) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->writeMult) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->readDirPref) < 0) goto Err;
		if(sunuint64unpack(a, ea, &a, &x->maxFileSize) < 0) goto Err;
		if(nfs3timeunpack(a, ea, &a, &x->timePrec) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->flags) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tpathconfprint(Fmt *fmt, Nfs3TPathconf *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TPathconf");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
}
uint
nfs3tpathconfsize(Nfs3TPathconf *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle);
	return a;
}
int
nfs3tpathconfpack(uchar *a, uchar *ea, uchar **pa, Nfs3TPathconf *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tpathconfunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TPathconf *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rpathconfprint(Fmt *fmt, Nfs3RPathconf *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RPathconf");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "haveAttr");
	fmtprint(fmt, "%d", x->haveAttr);
	fmtprint(fmt, "\n");
	switch(x->haveAttr){
	case 1:
		fmtprint(fmt, "\t%s=", "attr");
		nfs3attrprint(fmt, &x->attr);
		fmtprint(fmt, "\n");
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "maxLink");
		fmtprint(fmt, "%ud", x->maxLink);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "maxName");
		fmtprint(fmt, "%ud", x->maxName);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "noTrunc");
		fmtprint(fmt, "%d", x->noTrunc);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "chownRestricted");
		fmtprint(fmt, "%d", x->chownRestricted);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "caseInsensitive");
		fmtprint(fmt, "%d", x->caseInsensitive);
		fmtprint(fmt, "\n");
		fmtprint(fmt, "\t%s=", "casePreserving");
		fmtprint(fmt, "%d", x->casePreserving);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rpathconfsize(Nfs3RPathconf *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + 4;
	switch(x->haveAttr){
	case 1:
		a = a + nfs3attrsize(&x->attr);
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		a = a + 4 + 4 + 4 + 4 + 4 + 4;
		break;
	}
	return a;
}
int
nfs3rpathconfpack(uchar *a, uchar *ea, uchar **pa, Nfs3RPathconf *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(sunuint1pack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32pack(a, ea, &a, &x->maxLink) < 0) goto Err;
		if(sunuint32pack(a, ea, &a, &x->maxName) < 0) goto Err;
		if(sunuint1pack(a, ea, &a, &x->noTrunc) < 0) goto Err;
		if(sunuint1pack(a, ea, &a, &x->chownRestricted) < 0) goto Err;
		if(sunuint1pack(a, ea, &a, &x->caseInsensitive) < 0) goto Err;
		if(sunuint1pack(a, ea, &a, &x->casePreserving) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rpathconfunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RPathconf *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(sunuint1unpack(a, ea, &a, &x->haveAttr) < 0) goto Err;
	switch(x->haveAttr){
	case 1:
		if(nfs3attrunpack(a, ea, &a, &x->attr) < 0) goto Err;
		break;
	}
	switch(x->status){
	case Nfs3Ok:
		if(sunuint32unpack(a, ea, &a, &x->maxLink) < 0) goto Err;
		if(sunuint32unpack(a, ea, &a, &x->maxName) < 0) goto Err;
		if(sunuint1unpack(a, ea, &a, &x->noTrunc) < 0) goto Err;
		if(sunuint1unpack(a, ea, &a, &x->chownRestricted) < 0) goto Err;
		if(sunuint1unpack(a, ea, &a, &x->caseInsensitive) < 0) goto Err;
		if(sunuint1unpack(a, ea, &a, &x->casePreserving) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3tcommitprint(Fmt *fmt, Nfs3TCommit *x)
{
	fmtprint(fmt, "%s\n", "Nfs3TCommit");
	fmtprint(fmt, "\t%s=", "handle");
	nfs3handleprint(fmt, &x->handle);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "offset");
	fmtprint(fmt, "%llud", x->offset);
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "count");
	fmtprint(fmt, "%ud", x->count);
	fmtprint(fmt, "\n");
}
uint
nfs3tcommitsize(Nfs3TCommit *x)
{
	uint a;
	USED(x);
	a = 0 + nfs3handlesize(&x->handle) + 8 + 4;
	return a;
}
int
nfs3tcommitpack(uchar *a, uchar *ea, uchar **pa, Nfs3TCommit *x)
{
	if(nfs3handlepack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64pack(a, ea, &a, &x->offset) < 0) goto Err;
	if(sunuint32pack(a, ea, &a, &x->count) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3tcommitunpack(uchar *a, uchar *ea, uchar **pa, Nfs3TCommit *x)
{
	if(nfs3handleunpack(a, ea, &a, &x->handle) < 0) goto Err;
	if(sunuint64unpack(a, ea, &a, &x->offset) < 0) goto Err;
	if(sunuint32unpack(a, ea, &a, &x->count) < 0) goto Err;
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
void
nfs3rcommitprint(Fmt *fmt, Nfs3RCommit *x)
{
	fmtprint(fmt, "%s\n", "Nfs3RCommit");
	fmtprint(fmt, "\t%s=", "status");
	fmtprint(fmt, "%s", nfs3statusstr(x->status));
	fmtprint(fmt, "\n");
	fmtprint(fmt, "\t%s=", "wcc");
	nfs3wccprint(fmt, &x->wcc);
	fmtprint(fmt, "\n");
	switch(x->status){
	case Nfs3Ok:
		fmtprint(fmt, "\t%s=", "verf");
		fmtprint(fmt, "%.*H", Nfs3WriteVerfSize, x->verf);
		fmtprint(fmt, "\n");
		break;
	}
}
uint
nfs3rcommitsize(Nfs3RCommit *x)
{
	uint a;
	USED(x);
	a = 0 + 4 + nfs3wccsize(&x->wcc);
	switch(x->status){
	case Nfs3Ok:
		a = a + Nfs3WriteVerfSize;
		break;
	}
	return a;
}
int
nfs3rcommitpack(uchar *a, uchar *ea, uchar **pa, Nfs3RCommit *x)
{
	int i;

	if(i=x->status, sunenumpack(a, ea, &a, &i) < 0) goto Err;
	if(nfs3wccpack(a, ea, &a, &x->wcc) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(sunfixedopaquepack(a, ea, &a, x->verf, Nfs3WriteVerfSize) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}
int
nfs3rcommitunpack(uchar *a, uchar *ea, uchar **pa, Nfs3RCommit *x)
{
	int i;

	if(sunenumunpack(a, ea, &a, &i) < 0) goto Err; x->status = i;
	if(nfs3wccunpack(a, ea, &a, &x->wcc) < 0) goto Err;
	switch(x->status){
	case Nfs3Ok:
		if(sunfixedopaqueunpack(a, ea, &a, x->verf, Nfs3WriteVerfSize) < 0) goto Err;
		break;
	}
	*pa = a;
	return 0;
Err:
	*pa = ea;
	return -1;
}

typedef int (*P)(uchar*, uchar*, uchar**, SunCall*);
typedef void (*F)(Fmt*, SunCall*);
typedef uint (*S)(SunCall*);

static SunProc proc[] = {
	(P)nfs3tnullpack, (P)nfs3tnullunpack, (S)nfs3tnullsize, (F)nfs3tnullprint, sizeof(Nfs3TNull),
	(P)nfs3rnullpack, (P)nfs3rnullunpack, (S)nfs3rnullsize, (F)nfs3rnullprint, sizeof(Nfs3RNull),
	(P)nfs3tgetattrpack, (P)nfs3tgetattrunpack, (S)nfs3tgetattrsize, (F)nfs3tgetattrprint, sizeof(Nfs3TGetattr),
	(P)nfs3rgetattrpack, (P)nfs3rgetattrunpack, (S)nfs3rgetattrsize, (F)nfs3rgetattrprint, sizeof(Nfs3RGetattr),
	(P)nfs3tsetattrpack, (P)nfs3tsetattrunpack, (S)nfs3tsetattrsize, (F)nfs3tsetattrprint, sizeof(Nfs3TSetattr),
	(P)nfs3rsetattrpack, (P)nfs3rsetattrunpack, (S)nfs3rsetattrsize, (F)nfs3rsetattrprint, sizeof(Nfs3RSetattr),
	(P)nfs3tlookuppack, (P)nfs3tlookupunpack, (S)nfs3tlookupsize, (F)nfs3tlookupprint, sizeof(Nfs3TLookup),
	(P)nfs3rlookuppack, (P)nfs3rlookupunpack, (S)nfs3rlookupsize, (F)nfs3rlookupprint, sizeof(Nfs3RLookup),
	(P)nfs3taccesspack, (P)nfs3taccessunpack, (S)nfs3taccesssize, (F)nfs3taccessprint, sizeof(Nfs3TAccess),
	(P)nfs3raccesspack, (P)nfs3raccessunpack, (S)nfs3raccesssize, (F)nfs3raccessprint, sizeof(Nfs3RAccess),
	(P)nfs3treadlinkpack, (P)nfs3treadlinkunpack, (S)nfs3treadlinksize, (F)nfs3treadlinkprint, sizeof(Nfs3TReadlink),
	(P)nfs3rreadlinkpack, (P)nfs3rreadlinkunpack, (S)nfs3rreadlinksize, (F)nfs3rreadlinkprint, sizeof(Nfs3RReadlink),
	(P)nfs3treadpack, (P)nfs3treadunpack, (S)nfs3treadsize, (F)nfs3treadprint, sizeof(Nfs3TRead),
	(P)nfs3rreadpack, (P)nfs3rreadunpack, (S)nfs3rreadsize, (F)nfs3rreadprint, sizeof(Nfs3RRead),
	(P)nfs3twritepack, (P)nfs3twriteunpack, (S)nfs3twritesize, (F)nfs3twriteprint, sizeof(Nfs3TWrite),
	(P)nfs3rwritepack, (P)nfs3rwriteunpack, (S)nfs3rwritesize, (F)nfs3rwriteprint, sizeof(Nfs3RWrite),
	(P)nfs3tcreatepack, (P)nfs3tcreateunpack, (S)nfs3tcreatesize, (F)nfs3tcreateprint, sizeof(Nfs3TCreate),
	(P)nfs3rcreatepack, (P)nfs3rcreateunpack, (S)nfs3rcreatesize, (F)nfs3rcreateprint, sizeof(Nfs3RCreate),
	(P)nfs3tmkdirpack, (P)nfs3tmkdirunpack, (S)nfs3tmkdirsize, (F)nfs3tmkdirprint, sizeof(Nfs3TMkdir),
	(P)nfs3rmkdirpack, (P)nfs3rmkdirunpack, (S)nfs3rmkdirsize, (F)nfs3rmkdirprint, sizeof(Nfs3RMkdir),
	(P)nfs3tsymlinkpack, (P)nfs3tsymlinkunpack, (S)nfs3tsymlinksize, (F)nfs3tsymlinkprint, sizeof(Nfs3TSymlink),
	(P)nfs3rsymlinkpack, (P)nfs3rsymlinkunpack, (S)nfs3rsymlinksize, (F)nfs3rsymlinkprint, sizeof(Nfs3RSymlink),
	(P)nfs3tmknodpack, (P)nfs3tmknodunpack, (S)nfs3tmknodsize, (F)nfs3tmknodprint, sizeof(Nfs3TMknod),
	(P)nfs3rmknodpack, (P)nfs3rmknodunpack, (S)nfs3rmknodsize, (F)nfs3rmknodprint, sizeof(Nfs3RMknod),
	(P)nfs3tremovepack, (P)nfs3tremoveunpack, (S)nfs3tremovesize, (F)nfs3tremoveprint, sizeof(Nfs3TRemove),
	(P)nfs3rremovepack, (P)nfs3rremoveunpack, (S)nfs3rremovesize, (F)nfs3rremoveprint, sizeof(Nfs3RRemove),
	(P)nfs3trmdirpack, (P)nfs3trmdirunpack, (S)nfs3trmdirsize, (F)nfs3trmdirprint, sizeof(Nfs3TRmdir),
	(P)nfs3rrmdirpack, (P)nfs3rrmdirunpack, (S)nfs3rrmdirsize, (F)nfs3rrmdirprint, sizeof(Nfs3RRmdir),
	(P)nfs3trenamepack, (P)nfs3trenameunpack, (S)nfs3trenamesize, (F)nfs3trenameprint, sizeof(Nfs3TRename),
	(P)nfs3rrenamepack, (P)nfs3rrenameunpack, (S)nfs3rrenamesize, (F)nfs3rrenameprint, sizeof(Nfs3RRename),
	(P)nfs3tlinkpack, (P)nfs3tlinkunpack, (S)nfs3tlinksize, (F)nfs3tlinkprint, sizeof(Nfs3TLink),
	(P)nfs3rlinkpack, (P)nfs3rlinkunpack, (S)nfs3rlinksize, (F)nfs3rlinkprint, sizeof(Nfs3RLink),
	(P)nfs3treaddirpack, (P)nfs3treaddirunpack, (S)nfs3treaddirsize, (F)nfs3treaddirprint, sizeof(Nfs3TReadDir),
	(P)nfs3rreaddirpack, (P)nfs3rreaddirunpack, (S)nfs3rreaddirsize, (F)nfs3rreaddirprint, sizeof(Nfs3RReadDir),
	(P)nfs3treaddirpluspack, (P)nfs3treaddirplusunpack, (S)nfs3treaddirplussize, (F)nfs3treaddirplusprint, sizeof(Nfs3TReadDirPlus),
	(P)nfs3rreaddirpluspack, (P)nfs3rreaddirplusunpack, (S)nfs3rreaddirplussize, (F)nfs3rreaddirplusprint, sizeof(Nfs3RReadDirPlus),
	(P)nfs3tfsstatpack, (P)nfs3tfsstatunpack, (S)nfs3tfsstatsize, (F)nfs3tfsstatprint, sizeof(Nfs3TFsStat),
	(P)nfs3rfsstatpack, (P)nfs3rfsstatunpack, (S)nfs3rfsstatsize, (F)nfs3rfsstatprint, sizeof(Nfs3RFsStat),
	(P)nfs3tfsinfopack, (P)nfs3tfsinfounpack, (S)nfs3tfsinfosize, (F)nfs3tfsinfoprint, sizeof(Nfs3TFsInfo),
	(P)nfs3rfsinfopack, (P)nfs3rfsinfounpack, (S)nfs3rfsinfosize, (F)nfs3rfsinfoprint, sizeof(Nfs3RFsInfo),
	(P)nfs3tpathconfpack, (P)nfs3tpathconfunpack, (S)nfs3tpathconfsize, (F)nfs3tpathconfprint, sizeof(Nfs3TPathconf),
	(P)nfs3rpathconfpack, (P)nfs3rpathconfunpack, (S)nfs3rpathconfsize, (F)nfs3rpathconfprint, sizeof(Nfs3RPathconf),
	(P)nfs3tcommitpack, (P)nfs3tcommitunpack, (S)nfs3tcommitsize, (F)nfs3tcommitprint, sizeof(Nfs3TCommit),
	(P)nfs3rcommitpack, (P)nfs3rcommitunpack, (S)nfs3rcommitsize, (F)nfs3rcommitprint, sizeof(Nfs3RCommit)
};

SunProg nfs3prog =
{
	Nfs3Program,
	Nfs3Version,
	proc,
	nelem(proc),
};
