#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>
#include "SConn.h"

static long
ls(char *p, Dir **dirbuf)
{
	int fd;
	long n;
	Dir *db;

	if((db = dirstat(p)) == nil ||
		!(db->qid.type & QTDIR) ||
		(fd = open(p, OREAD)) < 0 )
		return -1;
	free(db);
	n = dirreadall(fd, dirbuf);
	close(fd);
	return n;
}

static uchar*
sha1file(char *pfx, char *nm)
{
	int n, fd, len;
	char *tmp;
	uchar buf[8192];
	static uchar digest[SHA1dlen];
	DigestState *s;

	len = strlen(pfx)+1+strlen(nm)+1;
	tmp = emalloc(len);
	snprint(tmp, len, "%s/%s", pfx, nm);
	if((fd = open(tmp, OREAD)) < 0){
		free(tmp);
		return nil;
	}
	free(tmp);
	s = nil;
	while((n = read(fd, buf, sizeof buf)) > 0)
		s = sha1(buf, n, nil, s);
	close(fd);
	sha1(nil, 0, digest, s);
	return digest;
}

static int
compare(Dir *a, Dir *b)
{
	return strcmp(a->name, b->name);
}

/* list the (name mtime size sum) of regular, readable files in path */
char *
dirls(char *path)
{
	char *list, *date, dig[30], buf[128];
	int m, nmwid, lenwid;
	long i, n, ndir, len;
	Dir *dirbuf;

	if(path==nil || (ndir = ls(path, &dirbuf)) < 0)
		return nil;

	qsort(dirbuf, ndir, sizeof dirbuf[0], (int (*)(const void *, const void *))compare);
	for(nmwid=lenwid=i=0; i<ndir; i++){
		if((m = strlen(dirbuf[i].name)) > nmwid)
			nmwid = m;
		snprint(buf, sizeof(buf), "%ulld", dirbuf[i].length);
		if((m = strlen(buf)) > lenwid)
			lenwid = m;
	}
	for(list=nil, len=0, i=0; i<ndir; i++){
		date = ctime(dirbuf[i].mtime);
		date[28] = 0;  /* trim newline */
		n = snprint(buf, sizeof buf, "%*ulld %s", lenwid, dirbuf[i].length, date+4);
		n += enc64(dig, sizeof dig, sha1file(path, dirbuf[i].name), SHA1dlen);
		n += nmwid+3+strlen(dirbuf[i].name);
		list = erealloc(list, len+n+1);
		len += snprint(list+len, n+1, "%-*s\t%s %s\n", nmwid, dirbuf[i].name, buf, dig);
	}
	free(dirbuf);
	return list;
}
