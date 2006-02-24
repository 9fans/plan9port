#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "tapefs.h"

/*
 * File system for cpio tapes (read-only)
 */

#define TBLOCK	512
#define NBLOCK	40	/* maximum blocksize */
#define DBLOCK	20	/* default blocksize */
#define TNAMSIZ	100

union hblock {
	char dummy[TBLOCK];
	char tbuf[Maxbuf];
	struct header {
		char magic[6];
		char dev[6];
		char ino[6];
		char mode[6];
		char uid[6];
		char gid[6];
		char nlink[6];
		char rdev[6];
		char mtime[11];
		char namesize[6];
		char size[11];
	} dbuf;
	struct hname {
		struct	header x;
		char	name[1];
	} nbuf;
} dblock;

int	tapefile;
vlong	getoct(char*, int);

void
populate(char *name)
{
	vlong offset;
	long isabs, magic, namesize, mode;
	Fileinf f;

	tapefile = open(name, OREAD);
	if (tapefile<0)
		error("Can't open argument file");
	replete = 1;
	for (offset = 0;;) {
		seek(tapefile, offset, 0);
		if (read(tapefile, (char *)&dblock.dbuf, TBLOCK)<TBLOCK)
			break;
		magic = getoct(dblock.dbuf.magic, sizeof(dblock.dbuf.magic));
		if (magic != 070707){
			print("%lo\n", magic);
			error("out of phase--get help");
		}
		if (dblock.nbuf.name[0]=='\0' || strcmp(dblock.nbuf.name, "TRAILER!!!")==0)
			break;
		mode = getoct(dblock.dbuf.mode, sizeof(dblock.dbuf.mode));
		f.mode = mode&0777;
		switch(mode & 0170000) {
		case 0040000:
			f.mode |= DMDIR;
			break;
		case 0100000:
			break;
		default:
			f.mode = 0;
			break;
		}
		f.uid = getoct(dblock.dbuf.uid, sizeof(dblock.dbuf.uid));
		f.gid = getoct(dblock.dbuf.gid, sizeof(dblock.dbuf.gid));
		f.size = getoct(dblock.dbuf.size, sizeof(dblock.dbuf.size));
		f.mdate = getoct(dblock.dbuf.mtime, sizeof(dblock.dbuf.mtime));
		namesize = getoct(dblock.dbuf.namesize, sizeof(dblock.dbuf.namesize));
		f.addr = offset+sizeof(struct header)+namesize;
		isabs = dblock.nbuf.name[0]=='/';
		f.name = &dblock.nbuf.name[isabs];
		poppath(f, 1);
		offset += sizeof(struct header)+namesize+f.size;
	}
}

vlong
getoct(char *p, int l)
{
	vlong r;

	for (r=0; l>0; p++, l--){
		r <<= 3;
		r += *p-'0';
	}
	return r;
}

void
dotrunc(Ram *r)
{
	USED(r);
}

void
docreate(Ram *r)
{
	USED(r);
}

char *
doread(Ram *r, vlong off, long cnt)
{
	seek(tapefile, r->addr+off, 0);
	if (cnt>sizeof(dblock.tbuf))
		error("read too big");
	read(tapefile, dblock.tbuf, cnt);
	return dblock.tbuf;
}

void
popdir(Ram *r)
{
	USED(r);
}

void
dowrite(Ram *r, char *buf, long off, long cnt)
{
	USED(r); USED(buf); USED(off); USED(cnt);
}

int
dopermw(Ram *r)
{
	USED(r);
	return 0;
}
