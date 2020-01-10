/*
 * old (V6 and before) PDP-11 Unix filesystem
 */
#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "tapefs.h"

/*
 * v6 disk inode
 */
#define	V6NADDR	8
#define	V6FMT	0160000
#define	V6IFREG	0100000
#define	V6IFDIR	0140000
#define	V6IFCHR	0120000
#define	V6IFBLK	0160000
#define	V6MODE	0777
#define	V6LARGE	010000
#define	V6SUPERB	1
#define	V6ROOT		1	/* root inode */
#define	V6NAMELEN	14
#define	BLSIZE	512
#define	LINOPB	(BLSIZE/sizeof(struct v6dinode))
#define	LNINDIR	(BLSIZE/sizeof(unsigned short))

struct v6dinode {
	unsigned char flags[2];
	unsigned char nlinks;
	unsigned char uid;
	unsigned char gid;
	unsigned char hisize;
	unsigned char losize[2];
	unsigned char addr[V6NADDR][2];
	unsigned char atime[4];	/* pdp-11 order */
	unsigned char mtime[4];	/* pdp-11 order */
};

struct	v6dir {
	uchar	ino[2];
	char	name[V6NAMELEN];
};

int	tapefile;
Fileinf	iget(int ino);
long	bmap(Ram *r, long bno);
void	getblk(Ram *r, long bno, char *buf);

void
populate(char *name)
{
	Fileinf f;

	replete = 0;
	tapefile = open(name, OREAD);
	if (tapefile<0)
		error("Can't open argument file");
	f = iget(V6ROOT);
	ram->perm = f.mode;
	ram->mtime = f.mdate;
	ram->addr = f.addr;
	ram->data = f.data;
	ram->ndata = f.size;
}

void
popdir(Ram *r)
{
	int i, ino;
	char *cp;
	struct v6dir *dp;
	Fileinf f;
	char name[V6NAMELEN+1];

	cp = 0;
	for (i=0; i<r->ndata; i+=sizeof(struct v6dir)) {
		if (i%BLSIZE==0)
			cp = doread(r, i, BLSIZE);
		dp = (struct v6dir *)(cp+i%BLSIZE);
		ino = dp->ino[0] + (dp->ino[1]<<8);
		if (strcmp(dp->name, ".")==0 || strcmp(dp->name, "..")==0)
			continue;
		if (ino==0)
			continue;
		f = iget(ino);
		strncpy(name, dp->name, V6NAMELEN);
		name[V6NAMELEN] = '\0';
		f.name = name;
		popfile(r, f);
	}
	r->replete = 1;
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
	static char buf[Maxbuf+BLSIZE];
	int bno, i;

	bno = off/BLSIZE;
	off -= bno*BLSIZE;
	if (cnt>Maxbuf)
		error("count too large");
	if (off)
		cnt += off;
	i = 0;
	while (cnt>0) {
		getblk(r, bno, &buf[i*BLSIZE]);
		cnt -= BLSIZE;
		bno++;
		i++;
	}
	return buf;
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

/*
 * fetch an i-node
 * -- no sanity check for now
 * -- magic inode-to-disk-block stuff here
 */

Fileinf
iget(int ino)
{
	char buf[BLSIZE];
	struct v6dinode *dp;
	long flags, i;
	Fileinf f;

	memset(&f, 0, sizeof f);
	seek(tapefile, BLSIZE*((ino-1)/LINOPB + V6SUPERB + 1), 0);
	if (read(tapefile, buf, BLSIZE) != BLSIZE)
		error("Can't read inode");
	dp = ((struct v6dinode *)buf) + ((ino-1)%LINOPB);
	flags = (dp->flags[1]<<8) + dp->flags[0];
	f.size = (dp->hisize << 16) + (dp->losize[1]<<8) + dp->losize[0];
	if ((flags&V6FMT)==V6IFCHR || (flags&V6FMT)==V6IFBLK)
		f.size = 0;
	f.data = emalloc(V6NADDR*sizeof(ushort));
	for (i = 0; i < V6NADDR; i++)
		((ushort*)f.data)[i] = (dp->addr[i][1]<<8) + dp->addr[i][0];
	f.mode = flags & V6MODE;
	if ((flags&V6FMT)==V6IFDIR)
		f.mode |= DMDIR;
	f.uid = dp->uid;
	f.gid = dp->gid;
	f.mdate = (dp->mtime[2]<<0) + (dp->mtime[3]<<8)
	     +(dp->mtime[0]<<16) + (dp->mtime[1]<<24);
	return f;
}

void
getblk(Ram *r, long bno, char *buf)
{
	long dbno;

	if ((dbno = bmap(r, bno)) == 0) {
		memset(buf, 0, BLSIZE);
		return;
	}
	seek(tapefile, dbno*BLSIZE, 0);
	if (read(tapefile, buf, BLSIZE) != BLSIZE)
		error("bad read");
}

/*
 * logical to physical block
 * only singly-indirect files for now
 */

long
bmap(Ram *r, long bno)
{
	unsigned char indbuf[LNINDIR][2];

	if (r->ndata <= V6NADDR*BLSIZE) {	/* assume size predicts largeness of file */
		if (bno < V6NADDR)
			return ((ushort*)r->data)[bno];
		return 0;
	}
	if (bno < V6NADDR*LNINDIR) {
		seek(tapefile, ((ushort *)r->data)[bno/LNINDIR]*BLSIZE, 0);
		if (read(tapefile, (char *)indbuf, BLSIZE) != BLSIZE)
			return 0;
		return ((indbuf[bno%LNINDIR][1]<<8) + indbuf[bno%LNINDIR][0]);
	}
	return 0;
}
