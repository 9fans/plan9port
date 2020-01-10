#include <u.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>
#include <ctype.h>
#include "dat.h"

/*
 *  format of a binding entry:
 *	char ipaddr[32];
 *	char id[32];
 *	char hwa[32];
 *	char otime[10];
 */
Binding *bcache;
uchar bfirst[IPaddrlen];
char *binddir = nil;
char *xbinddir = "#9/ndb/dhcp";

/*
 *  convert a byte array to hex
 */
static char
hex(int x)
{
	if(x < 10)
		return x + '0';
	return x - 10 + 'a';
}
extern char*
tohex(char *hdr, uchar *p, int len)
{
	char *s, *sp;
	int hlen;

	hlen = strlen(hdr);
	s = malloc(hlen + 2*len + 1);
	sp = s;
	strcpy(sp, hdr);
	sp += hlen;
	for(; len > 0; len--){
		*sp++ = hex(*p>>4);
		*sp++ = hex(*p & 0xf);
		p++;
	}
	*sp = 0;
	return s;
}

/*
 *  convert a client id to a string.  If it's already
 *  ascii, leave it be.  Otherwise, convert it to hex.
 */
extern char*
toid(uchar *p, int n)
{
	int i;
	char *s;

	for(i = 0; i < n; i++)
		if(!isprint(p[i]))
			return tohex("id", p, n);
	s = malloc(n + 1);
	memmove(s, p, n);
	s[n] = 0;
	return s;
}

/*
 *  increment an ip address
 */
static void
incip(uchar *ip)
{
	int i, x;

	for(i = IPaddrlen-1; i >= 0; i--){
		x = ip[i];
		x++;
		ip[i] = x;
		if((x & 0x100) == 0)
			break;
	}
}

/*
 *  find a binding for an id or hardware address
 */
static int
lockopen(char *file)
{
	char err[ERRMAX];
	int fd, tries;

	for(tries = 0; tries < 5; tries++){
		fd = open(file, OLOCK|ORDWR);
		if(fd >= 0)
			return fd;
		errstr(err, sizeof err);
		if(strstr(err, "lock")){
			/* wait for other process to let go of lock */
			sleep(250);

			/* try again */
			continue;
		}
		if(strstr(err, "exist") || strstr(err, "No such")){
			/* no file, create an exclusive access file */
			fd = create(file, ORDWR, DMEXCL|0666);
			chmod(file, 0666);
			if(fd >= 0)
				return fd;
		}
	}
	return -1;
}

void
setbinding(Binding *b, char *id, long t)
{
	if(b->boundto)
		free(b->boundto);

	b->boundto = strdup(id);
	b->lease = t;
}

static void
parsebinding(Binding *b, char *buf)
{
	long t;
	char *id, *p;

	/* parse */
	t = atoi(buf);
	id = strchr(buf, '\n');
	if(id){
		*id++ = 0;
		p = strchr(id, '\n');
		if(p)
			*p = 0;
	} else
		id = "";

	/* replace any past info */
	setbinding(b, id, t);
}

static int
writebinding(int fd, Binding *b)
{
	Dir *d;

	seek(fd, 0, 0);
	if(fprint(fd, "%ld\n%s\n", b->lease, b->boundto) < 0)
		return -1;
	d = dirfstat(fd);
	if(d == nil)
		return -1;
	b->q.type = d->qid.type;
	b->q.path = d->qid.path;
	b->q.vers = d->qid.vers;
	free(d);
	return 0;
}

/*
 *  synchronize cached binding with file.  the file always wins.
 */
int
syncbinding(Binding *b, int returnfd)
{
	char buf[512];
	int i, fd;
	Dir *d;

	if(binddir == nil)
		binddir = unsharp(xbinddir);

	snprint(buf, sizeof(buf), "%s/%I", binddir, b->ip);
	fd = lockopen(buf);
	if(fd < 0){
		/* assume someone else is using it */
		b->lease = time(0) + OfferTimeout;
		return -1;
	}

	/* reread if changed */
	d = dirfstat(fd);
	if(d != nil)	/* BUG? */
	if(d->qid.type != b->q.type || d->qid.path != b->q.path || d->qid.vers != b->q.vers){
		i = read(fd, buf, sizeof(buf)-1);
		if(i < 0)
			i = 0;
		buf[i] = 0;
		parsebinding(b, buf);
		b->lasttouched = d->mtime;
		b->q.path = d->qid.path;
		b->q.vers = d->qid.vers;
	}

	free(d);

	if(returnfd)
		return fd;

	close(fd);
	return 0;
}

extern int
samenet(uchar *ip, Info *iip)
{
	uchar x[IPaddrlen];

	maskip(iip->ipmask, ip, x);
	return ipcmp(x, iip->ipnet) == 0;
}

/*
 *  create a record for each binding
 */
extern void
initbinding(uchar *first, int n)
{
	while(n-- > 0){
		iptobinding(first, 1);
		incip(first);
	}
}

/*
 *  find a binding for a specific ip address
 */
extern Binding*
iptobinding(uchar *ip, int mk)
{
	Binding *b;

	for(b = bcache; b; b = b->next){
		if(ipcmp(b->ip, ip) == 0){
			syncbinding(b, 0);
			return b;
		}
	}

	if(mk == 0)
		return 0;
	b = malloc(sizeof(*b));
	memset(b, 0, sizeof(*b));
	ipmove(b->ip, ip);
	b->next = bcache;
	bcache = b;
	syncbinding(b, 0);
	return b;
}

static void
lognolease(Binding *b)
{
	/* renew the old binding, and hope it eventually goes away */
	b->offer = 5*60;
	commitbinding(b);

	/* complain if we haven't in the last 5 minutes */
	if(now - b->lastcomplained < 5*60)
		return;
	syslog(0, blog, "dhcp: lease for %I to %s ended at %ld but still in use\n",
		b->ip, b->boundto != nil ? b->boundto : "?", b->lease);
	b->lastcomplained = now;
}

/*
 *  find a free binding for a hw addr or id on the same network as iip
 */
extern Binding*
idtobinding(char *id, Info *iip, int ping)
{
	Binding *b, *oldest;
	int oldesttime;

	/*
	 *  first look for an old binding that matches.  that way
	 *  clients will tend to keep the same ip addresses.
	 */
	for(b = bcache; b; b = b->next){
		if(b->boundto && strcmp(b->boundto, id) == 0){
			if(!samenet(b->ip, iip))
				continue;

			/* check with the other servers */
			syncbinding(b, 0);
			if(strcmp(b->boundto, id) == 0)
				return b;
		}
	}

	/*
	 *  look for oldest binding that we think is unused
	 */
	for(;;){
		oldest = nil;
		oldesttime = 0;
		for(b = bcache; b; b = b->next){
			if(b->tried != now)
			if(b->lease < now && b->expoffer < now && samenet(b->ip, iip))
			if(oldest == nil || b->lasttouched < oldesttime){
				/* sync and check again */
				syncbinding(b, 0);
				if(b->lease < now && b->expoffer < now && samenet(b->ip, iip))
				if(oldest == nil || b->lasttouched < oldesttime){
					oldest = b;
					oldesttime = b->lasttouched;
				}
			}
		}
		if(oldest == nil)
			break;

		/* make sure noone is still using it */
		oldest->tried = now;
		if(ping == 0 || icmpecho(oldest->ip) == 0)
			return oldest;

		lognolease(oldest);	/* sets lastcomplained */
	}

	/* try all bindings */
	for(b = bcache; b; b = b->next){
		syncbinding(b, 0);
		if(b->tried != now)
		if(b->lease < now && b->expoffer < now && samenet(b->ip, iip)){
			b->tried = now;
			if(ping == 0 || icmpecho(b->ip) == 0)
				return b;

			lognolease(b);
		}
	}

	/* nothing worked, give up */
	return 0;
}

/*
 *  create an offer
 */
extern void
mkoffer(Binding *b, char *id, long leasetime)
{
	if(leasetime <= 0){
		if(b->lease > now + minlease)
			leasetime = b->lease - now;
		else
			leasetime = minlease;
	}
	if(b->offeredto)
		free(b->offeredto);
	b->offeredto = strdup(id);
	b->offer = leasetime;
	b->expoffer = now + OfferTimeout;
}

/*
 *  find an offer for this id
 */
extern Binding*
idtooffer(char *id, Info *iip)
{
	Binding *b;

	/* look for an offer to this id */
	for(b = bcache; b; b = b->next){
		if(b->offeredto && strcmp(b->offeredto, id) == 0 && samenet(b->ip, iip)){
			/* make sure some other system hasn't stolen it */
			syncbinding(b, 0);
			if(b->lease < now
			|| (b->boundto && strcmp(b->boundto, b->offeredto) == 0))
				return b;
		}
	}
	return 0;
}

/*
 *  commit a lease, this could fail
 */
extern int
commitbinding(Binding *b)
{
	int fd;
	long now;

	now = time(0);

	if(b->offeredto == 0)
		return -1;
	fd = syncbinding(b, 1);
	if(fd < 0)
		return -1;
	if(b->lease > now && b->boundto && strcmp(b->boundto, b->offeredto) != 0){
		close(fd);
		return -1;
	}
	setbinding(b, b->offeredto, now + b->offer);
	b->lasttouched = now;

	if(writebinding(fd, b) < 0){
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

/*
 *  commit a lease, this could fail
 */
extern int
releasebinding(Binding *b, char *id)
{
	int fd;
	long now;

	now = time(0);

	fd = syncbinding(b, 1);
	if(fd < 0)
		return -1;
	if(b->lease > now && b->boundto && strcmp(b->boundto, id) != 0){
		close(fd);
		return -1;
	}
	b->lease = 0;
	b->expoffer = 0;

	if(writebinding(fd, b) < 0){
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}
