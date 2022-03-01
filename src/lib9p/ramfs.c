#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

static char Ebad[] = "something bad happened";
static char Enomem[] = "no memory";

typedef struct Ramfile	Ramfile;
struct Ramfile {
	char *data;
	int ndata;
};

void
fsread(Req *r)
{
	Ramfile *rf;
	vlong offset;
	long count;

	rf = r->fid->file->aux;
	offset = r->ifcall.offset;
	count = r->ifcall.count;

/*print("read %ld %lld\n", *count, offset); */
	if(offset >= rf->ndata){
		r->ofcall.count = 0;
		respond(r, nil);
		return;
	}

	if(offset+count >= rf->ndata)
		count = rf->ndata - offset;

	memmove(r->ofcall.data, rf->data+offset, count);
	r->ofcall.count = count;
	respond(r, nil);
}

void
fswrite(Req *r)
{
	void *v;
	Ramfile *rf;
	vlong offset;
	long count;

	rf = r->fid->file->aux;
	offset = r->ifcall.offset;
	count = r->ifcall.count;

	if(offset+count >= rf->ndata){
		v = realloc(rf->data, offset+count);
		if(v == nil){
			respond(r, Enomem);
			return;
		}
		rf->data = v;
		rf->ndata = offset+count;
		r->fid->file->dir.length = rf->ndata;
	}
	memmove(rf->data+offset, r->ifcall.data, count);
	r->ofcall.count = count;
	respond(r, nil);
}

void
fscreate(Req *r)
{
	Ramfile *rf;
	File *f;

	if(f = createfile(r->fid->file, r->ifcall.name, r->fid->uid, r->ifcall.perm, nil)){
		rf = emalloc9p(sizeof *rf);
		f->aux = rf;
		r->fid->file = f;
		r->ofcall.qid = f->dir.qid;
		respond(r, nil);
		return;
	}
	respond(r, Ebad);
}

void
fsopen(Req *r)
{
	Ramfile *rf;

	rf = r->fid->file->aux;

	if(rf && (r->ifcall.mode&OTRUNC)){
		rf->ndata = 0;
		r->fid->file->dir.length = 0;
	}

	respond(r, nil);
}

void
fsdestroyfile(File *f)
{
	Ramfile *rf;

/*fprint(2, "clunk\n"); */
	rf = f->aux;
	if(rf){
		free(rf->data);
		free(rf);
	}
}

Srv fs = {
	.open=	fsopen,
	.read=	fsread,
	.write=	fswrite,
	.create=	fscreate,
};

void
usage(void)
{
	fprint(2, "usage: ramfs [-D] [-s srvname] [-m mtpt]\n");
	threadexitsall("usage");
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char **argv)
{
	char *srvname = nil;
	char *mtpt = nil;
	Qid q;

	fs.tree = alloctree(nil, nil, DMDIR|0777, fsdestroyfile);
	q = fs.tree->root->dir.qid;

	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 's':
		srvname = EARGF(usage());
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	if(argc)
		usage();

	if(chatty9p)
		fprint(2, "ramsrv.nopipe %d srvname %s mtpt %s\n", fs.nopipe, srvname, mtpt);
	if(srvname == nil && mtpt == nil)
		sysfatal("you should at least specify a -s or -m option");

	threadpostmountsrv(&fs, srvname, mtpt, MREPL|MCREATE);
	threadexits(0);
}
