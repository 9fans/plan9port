#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <ndbhf.h>

/*
 *  search for a tuple that has the given 'attr=val' and also 'rattr=x'.
 *  copy 'x' into 'buf' and return the whole tuple.
 *
 *  return 0 if not found.
 */
char*
csgetvalue(char *netroot, char *attr, char *val, char *rattr, Ndbtuple **pp)
{
	Ndbtuple *t, *first, *last;
	int n, linefound;
	char line[1024];
	int fd;
	int oops = 0;
	char *rv;

	if(pp)
		*pp = nil;
	rv = nil;

	if(netroot)
		snprint(line, sizeof(line), "%s/cs", netroot);
	else
		strcpy(line, "/net/cs");
	fd = open(line, ORDWR);
	if(fd < 0)
		return 0;
	seek(fd, 0, 0);
	snprint(line, sizeof(line), "!%s=%s %s=*", attr, val, rattr);
	if(write(fd, line, strlen(line)) < 0){
		close(fd);
		return 0;
	}
	seek(fd, 0, 0);

	first = last = 0;
	linefound = 0;
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

		for(; t; t = t->entry){
			if(linefound == 0){
				if(strcmp(rattr, t->attr) == 0){
					linefound = 1;
					rv = strdup(t->val);
				}
			}
		}
	}
	close(fd);

	if(oops){
		werrstr("buffer too short");
		ndbfree(first);
		return nil;
	}

	if(pp){
		setmalloctag(first, getcallerpc(&netroot));
		*pp = first;
	} else
		ndbfree(first);

	return rv;
}

Ndbtuple*
csgetval(char *netroot, char *attr, char *val, char *rattr, char *buf)
{
	Ndbtuple *t;
	char *p;

	p = csgetvalue(netroot, attr, val, rattr, &t);
	if(p == nil){
		if(buf != nil)
			*buf = 0;
	} else {
		if(buf != nil){
			strncpy(buf, p, Ndbvlen-1);
			buf[Ndbvlen-1] = 0;
		}
		free(p);
	}
	return t;
}
