#include <u.h>
#include <libc.h>
#include "tapefs.h"

/*
 * File system for tp tapes.  dectape versions have 192
 * entries, magtape have 496.  This treats the same
 * by ignoring entries with bad header checksums
 */

struct tp {
	unsigned char	name[32];
	unsigned char	mode[2];
	unsigned char	uid[1];
	unsigned char	gid[1];
	unsigned char	unused[1];
	unsigned char	size[3];
	unsigned char	tmod[4];
	unsigned char	taddress[2];
	unsigned char	unused2[16];
	unsigned char	checksum[2];
} dir[496+8];

char	buffer[8192];
int	tapefile;

void
populate(char *name)
{
	int i, isabs, badcksum, goodcksum;
	struct tp *tpp;
	Fileinf f;

	replete = 1;
	tapefile = open(name, OREAD);
	if (tapefile<0)
		error("Can't open argument file");
	read(tapefile, dir, sizeof dir);
	badcksum = goodcksum = 0;
	for (i=0, tpp=&dir[8]; i<496; i++, tpp++) {
		unsigned char *sp = (unsigned char *)tpp;
		int j, cksum = 0;
		for (j=0; j<32; j++, sp+=2)
			cksum += sp[0] + (sp[1]<<8);
		cksum &= 0xFFFF;
		if (cksum!=0) {
			badcksum++;
			continue;
		}
		goodcksum++;
		if (tpp->name[0]=='\0')
			continue;
		f.addr = tpp->taddress[0] + (tpp->taddress[1]<<8);
		if (f.addr==0)
			continue;
		f.size = (tpp->size[0]<<16) + (tpp->size[1]<<0) + (tpp->size[2]<<8);
		f.mdate = (tpp->tmod[2]<<0) + (tpp->tmod[3]<<8)
		     +(tpp->tmod[0]<<16) + (tpp->tmod[1]<<24);
		f.mode = tpp->mode[0]&0777;
		f.uid = tpp->uid[0];
		f.gid = tpp->gid[0];
		isabs = tpp->name[0]=='/';
		f.name = (char *)tpp->name+isabs;
		poppath(f, 1);
	}
	fprint(2, "%d bad checksums, %d good\n", badcksum, goodcksum);
}

void
popdir(Ram *r)
{
	USED(r);
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
	if (cnt>sizeof(buffer))
		print("count too big\n");
	seek(tapefile, 512*r->addr+off, 0);
	read(tapefile, buffer, cnt);
	return buffer;
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
