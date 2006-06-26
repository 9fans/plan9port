#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

/*
 * simplistic permission checking.  assume that
 * each user is the leader of her own group.
 */
int
hasperm(File *f, char *uid, int p)
{
	int m;

	m = f->dir.mode & 7;	/* other */
	if((p & m) == p)
		return 1;

	if(strcmp(f->dir.uid, uid) == 0) {
		m |= (f->dir.mode>>6) & 7;
		if((p & m) == p)
			return 1;
	}

	if(strcmp(f->dir.gid, uid) == 0) {
		m |= (f->dir.mode>>3) & 7;
		if((p & m) == p)
			return 1;
	}

	return 0;
}
