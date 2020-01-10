#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <ndbhf.h>

static void nstrcpy(char*, char*, int);
static void mkptrname(char*, char*, int);
static Ndbtuple *doquery(int, char *dn, char *type);

/*
 *  search for a tuple that has the given 'attr=val' and also 'rattr=x'.
 *  copy 'x' into 'buf' and return the whole tuple.
 *
 *  return 0 if not found.
 */
Ndbtuple*
dnsquery(char *net, char *val, char *type)
{
	char rip[128];
	char *p;
	Ndbtuple *t;
	int fd;

	/* if the address is V4 or V6 null address, give up early vwhoi*/
	if(strcmp(val, "::") == 0 || strcmp(val, "0.0.0.0") == 0)
		return nil;

	if(net == nil)
		net = "/net";
	snprint(rip, sizeof(rip), "%s/dns", net);
	fd = open(rip, ORDWR);
	if(fd < 0){
		if(strcmp(net, "/net") == 0)
			snprint(rip, sizeof(rip), "/srv/dns");
		else {
			snprint(rip, sizeof(rip), "/srv/dns%s", net);
			p = strrchr(rip, '/');
			*p = '_';
		}
		fd = open(rip, ORDWR);
		if(fd < 0)
			return nil;
		if(mount(fd, -1, net, MBEFORE, "") < 0){
			close(fd);
			return nil;
		}
		/* fd is now closed */
		snprint(rip, sizeof(rip), "%s/dns", net);
		fd = open(rip, ORDWR);
		if(fd < 0)
			return nil;
	}

	/* zero out the error string */
	werrstr("");

	/* if this is a reverse lookup, first lookup the domain name */
	if(strcmp(type, "ptr") == 0){
		mkptrname(val, rip, sizeof rip);
		t = doquery(fd, rip, "ptr");
	} else
		t = doquery(fd, val, type);

	close(fd);
	return t;
}

/*
 *  convert address into a reverse lookup address
 */
static void
mkptrname(char *ip, char *rip, int rlen)
{
	char buf[128];
	char *p, *np;
	int len;

	if(strstr(ip, "in-addr.arpa") || strstr(ip, "IN-ADDR.ARPA")){
		nstrcpy(rip, ip, rlen);
		return;
	}

	nstrcpy(buf, ip, sizeof buf);
	for(p = buf; *p; p++)
		;
	*p = '.';
	np = rip;
	len = 0;
	while(p >= buf){
		len++;
		p--;
		if(*p == '.'){
			memmove(np, p+1, len);
			np += len;
			len = 0;
		}
	}
	memmove(np, p+1, len);
	np += len;
	strcpy(np, "in-addr.arpa");
}

static void
nstrcpy(char *to, char *from, int len)
{
	strncpy(to, from, len);
	to[len-1] = 0;
}

static Ndbtuple*
doquery(int fd, char *dn, char *type)
{
	char buf[1024];
	int n;
	Ndbtuple *t, *first, *last;

	seek(fd, 0, 0);
	snprint(buf, sizeof(buf), "!%s %s", dn, type);
	if(write(fd, buf, strlen(buf)) < 0)
		return nil;

	seek(fd, 0, 0);

	first = last = nil;

	for(;;){
		n = read(fd, buf, sizeof(buf)-2);
		if(n <= 0)
			break;
		if(buf[n-1] != '\n')
			buf[n++] = '\n';	/* ndbparsline needs a trailing new line */
		buf[n] = 0;

		/* check for the error condition */
		if(buf[0] == '!'){
			werrstr("%s", buf+1);
			return nil;
		}

		t = _ndbparseline(buf);
		if(t != nil){
			if(first)
				last->entry = t;
			else
				first = t;
			last = t;

			while(last->entry)
				last = last->entry;
		}
	}

	setmalloctag(first, getcallerpc(&fd));
	return first;
}
