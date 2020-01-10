#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <ndbhf.h>

/*
 *  look up the ip attributes 'list' for an entry that has the
 *  given 'attr=val' and a 'ip=' tuples.
 *
 *  return nil if not found.
 */
Ndbtuple*
csipinfo(char *netroot, char *attr, char *val, char **list, int n)
{
	Ndbtuple *t, *first, *last;
	int i;
	char line[1024];
	int fd;
	char *p, *e;

	if(netroot)
		snprint(line, sizeof(line), "%s/cs", netroot);
	else
		strcpy(line, "/net/cs");
	fd = open(line, ORDWR);
	if(fd < 0)
		return 0;
	seek(fd, 0, 0);
	e = line + sizeof(line);
	p = seprint(line, e, "!ipinfo %s=%s", attr, val);
	for(i = 0; i < n; i++){
		if(*list == nil)
			break;
		p = seprint(p, e, " %s", *list++);
	}

	if(write(fd, line, strlen(line)) < 0){
		close(fd);
		return 0;
	}
	seek(fd, 0, 0);

	first = last = 0;
	for(;;){
		n = read(fd, line, sizeof(line)-2);
		if(n <= 0)
			break;
		line[n] = '\n';
		line[n+1] = 0;

		t = _ndbparseline(line);
		if(t == 0)
			continue;
		if(first)
			last->entry = t;
		else
			first = t;
		last = t;

		while(last->entry)
			last = last->entry;
	}
	close(fd);

	setmalloctag(first, getcallerpc(&netroot));
	return first;
}
