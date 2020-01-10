#include <u.h>
#include <libc.h>
#include <ip.h>
#include <ctype.h>
#include <bio.h>
#include <ndb.h>
#include <thread.h>
#include "dns.h"

/*
 *  Hash table for domain names.  The hash is based only on the
 *  first element of the domain name.
 */
DN	*ht[HTLEN];


static struct
{
	Lock	lk;
	ulong	names;	/* names allocated */
	ulong	oldest;	/* longest we'll leave a name around */
	int	active;
	int	mutex;
	int	id;
} dnvars;

/* names of RR types */
char *rrtname[Tall+2] =
{
	nil,
	"ip",
	"ns",
	"md",
	"mf",
	"cname",
	"soa",
	"mb",
	"mg",
	"mr",
	"null",
	"wks",
	"ptr",
	"hinfo",
	"minfo",
	"mx",
	"txt",
	"rp",
	nil,
	nil,
	nil,
	nil,
	nil,
	nil,
	"sig",
	"key",
	nil,
	nil,
	"aaaa",
	nil,
	nil,
	nil,
	nil,
	nil,
	nil,
	nil,
	nil,
	"cert",
	nil,
	nil,

/* 40 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 48 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 56 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 64 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 72 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 80 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 88 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 96 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 104 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 112 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 120 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 128 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 136 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 144 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 152 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 160 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 168 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 176 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 184 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 192 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 200 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 208 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 216 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 224 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 232 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 240 */	nil, nil, nil, nil, nil, nil, nil, nil,
/* 248 */	nil, nil, nil,

	"ixfr",
	"axfr",
	"mailb",
	nil,
	"all",
	nil
};

/* names of response codes */
char *rname[Rmask+1] =
{
	"ok",
	"format error",
	"server failure",
	"bad name",
	"unimplemented",
	"we don't like you"
};

Lock	dnlock;

static int sencodefmt(Fmt*);

/*
 *  set up a pipe to use as a lock
 */
void
dninit(void)
{
	fmtinstall('E', eipfmt);
	fmtinstall('I', eipfmt);
	fmtinstall('V', eipfmt);
	fmtinstall('R', rrfmt);
	fmtinstall('Q', rravfmt);
	fmtinstall('H', sencodefmt);

	dnvars.oldest = maxage;
	dnvars.names = 0;
}

/*
 *  hash for a domain name
 */
static ulong
dnhash(char *name)
{
	ulong hash;
	uchar *val = (uchar*)name;

	for(hash = 0; *val; val++)
		hash = (hash*13) + tolower(*val)-'a';
	return hash % HTLEN;
}

/*
 *  lookup a symbol.  if enter is not zero and the name is
 *  not found, create it.
 */
DN*
dnlookup(char *name, int class, int enter)
{
	DN **l;
	DN *dp;

	l = &ht[dnhash(name)];
	lock(&dnlock);
	for(dp = *l; dp; dp = dp->next) {
		assert(dp->magic == DNmagic);
		if(dp->class == class && cistrcmp(dp->name, name) == 0){
			dp->referenced = now;
			unlock(&dnlock);
			return dp;
		}
		l = &dp->next;
	}
	if(enter == 0){
		unlock(&dnlock);
		return 0;
	}
	dnvars.names++;
	dp = emalloc(sizeof(*dp));
	dp->magic = DNmagic;
	dp->name = estrdup(name);
	assert(dp->name != 0);
	dp->class = class;
	dp->rr = 0;
	dp->next = 0;
	dp->referenced = now;
	*l = dp;
	unlock(&dnlock);

	return dp;
}

/*
 *  dump the cache
 */
void
dndump(char *file)
{
	DN *dp;
	int i, fd;
	RR *rp;

	fd = open(file, OWRITE|OTRUNC);
	if(fd < 0)
		return;
	lock(&dnlock);
	for(i = 0; i < HTLEN; i++){
		for(dp = ht[i]; dp; dp = dp->next){
			fprint(fd, "%s\n", dp->name);
			for(rp = dp->rr; rp; rp = rp->next)
				fprint(fd, "	%R %c%c %lud/%lud\n", rp, rp->auth?'A':'U',
					rp->db?'D':'N', rp->expire, rp->ttl);
		}
	}
	unlock(&dnlock);
	close(fd);
}

/*
 *  purge all records
 */
void
dnpurge(void)
{
	DN *dp;
	RR *rp, *srp;
	int i;

	lock(&dnlock);

	for(i = 0; i < HTLEN; i++)
		for(dp = ht[i]; dp; dp = dp->next){
			srp = rp = dp->rr;
			dp->rr = nil;
			for(; rp != nil; rp = rp->next)
				rp->cached = 0;
			rrfreelist(srp);
		}

	unlock(&dnlock);
}

/*
 *  check the age of resource records, free any that have timed out
 */
void
dnage(DN *dp)
{
	RR **l;
	RR *rp, *next;
	ulong diff;

	diff = now - dp->referenced;
	if(diff < Reserved)
		return;

	l = &dp->rr;
	for(rp = dp->rr; rp; rp = next){
		assert(rp->magic == RRmagic && rp->cached);
		next = rp->next;
		if(!rp->db)
		if(rp->expire < now || diff > dnvars.oldest){
			*l = next;
			rp->cached = 0;
			rrfree(rp);
			continue;
		}
		l = &rp->next;
	}
}

#define REF(x) if(x) x->refs++

/*
 *  our target is 4000 names cached, this should be larger on large servers
 */
#define TARGET 4000

/*
 *  periodicly sweep for old records and remove unreferenced domain names
 *
 *  only called when all other threads are locked out
 */
void
dnageall(int doit)
{
	DN *dp, **l;
	int i;
	RR *rp;
	static ulong nextage;

	if(dnvars.names < TARGET && now < nextage && !doit){
		dnvars.oldest = maxage;
		return;
	}

	if(dnvars.names > TARGET)
		dnvars.oldest /= 2;
	nextage = now + maxage;

	lock(&dnlock);

	/* time out all old entries (and set refs to 0) */
	for(i = 0; i < HTLEN; i++)
		for(dp = ht[i]; dp; dp = dp->next){
			dp->refs = 0;
			dnage(dp);
		}

	/* mark all referenced domain names */
	for(i = 0; i < HTLEN; i++)
		for(dp = ht[i]; dp; dp = dp->next)
			for(rp = dp->rr; rp; rp = rp->next){
				REF(rp->owner);
				if(rp->negative){
					REF(rp->negsoaowner);
					continue;
				}
				switch(rp->type){
				case Thinfo:
					REF(rp->cpu);
					REF(rp->os);
					break;
				case Ttxt:
					break;
				case Tcname:
				case Tmb:
				case Tmd:
				case Tmf:
				case Tns:
					REF(rp->host);
					break;
				case Tmg:
				case Tmr:
					REF(rp->mb);
					break;
				case Tminfo:
					REF(rp->rmb);
					REF(rp->mb);
					break;
				case Trp:
					REF(rp->rmb);
					REF(rp->rp);
					break;
				case Tmx:
					REF(rp->host);
					break;
				case Ta:
				case Taaaa:
					REF(rp->ip);
					break;
				case Tptr:
					REF(rp->ptr);
					break;
				case Tsoa:
					REF(rp->host);
					REF(rp->rmb);
					break;
				}
			}

	/* sweep and remove unreferenced domain names */
	for(i = 0; i < HTLEN; i++){
		l = &ht[i];
		for(dp = *l; dp; dp = *l){
			if(dp->rr == 0 && dp->refs == 0){
				assert(dp->magic == DNmagic);
				*l = dp->next;
				if(dp->name)
					free(dp->name);
				dp->magic = ~dp->magic;
				dnvars.names--;
				free(dp);
				continue;
			}
			l = &dp->next;
		}
	}

	unlock(&dnlock);
}

/*
 *  timeout all database records (used when rereading db)
 */
void
dnagedb(void)
{
	DN *dp;
	int i;
	RR *rp;

	lock(&dnlock);

	/* time out all database entries */
	for(i = 0; i < HTLEN; i++)
		for(dp = ht[i]; dp; dp = dp->next)
			for(rp = dp->rr; rp; rp = rp->next)
				if(rp->db)
					rp->expire = 0;

	unlock(&dnlock);
}

/*
 *  mark all local db records about my area as authoritative, time out any others
 */
void
dnauthdb(void)
{
	DN *dp;
	int i;
	Area *area;
	RR *rp;

	lock(&dnlock);

	/* time out all database entries */
	for(i = 0; i < HTLEN; i++)
		for(dp = ht[i]; dp; dp = dp->next){
			area = inmyarea(dp->name);
			for(rp = dp->rr; rp; rp = rp->next)
				if(rp->db){
					if(area){
						if(rp->ttl < area->soarr->soa->minttl)
							rp->ttl = area->soarr->soa->minttl;
						rp->auth = 1;
					}
					if(rp->expire == 0){
						rp->db = 0;
						dp->referenced = now - Reserved - 1;
					}
				}
		}

	unlock(&dnlock);
}

/*
 *  keep track of other processes to know if we can
 *  garbage collect.  block while garbage collecting.
 */
int
getactivity(Request *req)
{
	int rv;

	if(traceactivity) syslog(0, "dns", "get %d by %d.%d", dnvars.active, getpid(), threadid());
	lock(&dnvars.lk);
	while(dnvars.mutex){
		unlock(&dnvars.lk);
		sleep(200);
		lock(&dnvars.lk);
	}
	rv = ++dnvars.active;
	now = time(0);
	req->id = ++dnvars.id;
	unlock(&dnvars.lk);

	return rv;
}
void
putactivity(void)
{
	if(traceactivity) syslog(0, "dns", "put %d by %d.%d", dnvars.active, getpid(), threadid());
	lock(&dnvars.lk);
	dnvars.active--;
	assert(dnvars.active >= 0); /* "dnvars.active %d", dnvars.active */;

	/*
	 *  clean out old entries and check for new db periodicly
	 */
	if(dnvars.mutex || (needrefresh == 0 && dnvars.active > 0)){
		unlock(&dnvars.lk);
		return;
	}

	/* wait till we're alone */
	dnvars.mutex = 1;
	while(dnvars.active > 0){
		unlock(&dnvars.lk);
		sleep(100);
		lock(&dnvars.lk);
	}
	unlock(&dnvars.lk);

	db2cache(needrefresh);
	dnageall(0);

	/* let others back in */
	needrefresh = 0;
	dnvars.mutex = 0;
}

/*
 *  Attach a single resource record to a domain name.
 *	- Avoid duplicates with already present RR's
 *	- Chain all RR's of the same type adjacent to one another
 *	- chain authoritative RR's ahead of non-authoritative ones
 */
static void
rrattach1(RR *new, int auth)
{
	RR **l;
	RR *rp;
	DN *dp;

	assert(new->magic == RRmagic && !new->cached);

	if(!new->db)
		new->expire = new->ttl;
	else
		new->expire = now + Year;
	dp = new->owner;
	assert(dp->magic == DNmagic);
	new->auth |= auth;
	new->next = 0;

	/*
	 *  find first rr of the right type
	 */
	l = &dp->rr;
	for(rp = *l; rp; rp = *l){
		assert(rp->magic == RRmagic && rp->cached);
		if(rp->type == new->type)
			break;
		l = &rp->next;
	}

	/*
	 *  negative entries replace positive entries
	 *  positive entries replace negative entries
	 *  newer entries replace older entries with the same fields
	 */
	for(rp = *l; rp; rp = *l){
		assert(rp->magic == RRmagic && rp->cached);
		if(rp->type != new->type)
			break;

		if(rp->db == new->db && rp->auth == new->auth){
			/* negative drives out positive and vice versa */
			if(rp->negative != new->negative){
				*l = rp->next;
				rp->cached = 0;
				rrfree(rp);
				continue;
			}

			/* all things equal, pick the newer one */
			if(rp->arg0 == new->arg0 && rp->arg1 == new->arg1){
				/* new drives out old */
				if(new->ttl > rp->ttl || new->expire > rp->expire){
					*l = rp->next;
					rp->cached = 0;
					rrfree(rp);
					continue;
				} else {
					rrfree(new);
					return;
				}
			}

			/*  Hack for pointer records.  This makes sure
			 *  the ordering in the list reflects the ordering
			 *  received or read from the database
			 */
			if(rp->type == Tptr){
				if(!rp->negative && !new->negative
				&& rp->ptr->ordinal > new->ptr->ordinal)
					break;
			}
		}
		l = &rp->next;
	}

	/*
	 *  add to chain
	 */
	new->cached = 1;
	new->next = *l;
	*l = new;
}

/*
 *  Attach a list of resource records to a domain name.
 *	- Avoid duplicates with already present RR's
 *	- Chain all RR's of the same type adjacent to one another
 *	- chain authoritative RR's ahead of non-authoritative ones
 *	- remove any expired RR's
 */
void
rrattach(RR *rp, int auth)
{
	RR *next;

	lock(&dnlock);
	for(; rp; rp = next){
		next = rp->next;
		rp->next = 0;

		/* avoid any outside spoofing */
		if(cachedb && !rp->db && inmyarea(rp->owner->name))
			rrfree(rp);
		else
			rrattach1(rp, auth);
	}
	unlock(&dnlock);
}

/*
 *  allocate a resource record of a given type
 */
RR*
rralloc(int type)
{
	RR *rp;

	rp = emalloc(sizeof(*rp));
	rp->magic = RRmagic;
	rp->pc = getcallerpc(&type);
	rp->type = type;
	switch(type){
	case Tsoa:
		rp->soa = emalloc(sizeof(*rp->soa));
		rp->soa->slaves = nil;
		break;
	case Tkey:
		rp->key = emalloc(sizeof(*rp->key));
		break;
	case Tcert:
		rp->cert = emalloc(sizeof(*rp->cert));
		break;
	case Tsig:
		rp->sig = emalloc(sizeof(*rp->sig));
		break;
	case Tnull:
		rp->null = emalloc(sizeof(*rp->null));
		break;
	}
	rp->ttl = 0;
	rp->expire = 0;
	rp->next = 0;
	return rp;
}

/*
 *  free a resource record and any related structs
 */
void
rrfree(RR *rp)
{
	DN *dp;
	RR *nrp;
	Txt *t;

	assert(rp->magic = RRmagic);
	assert(!rp->cached);

	dp = rp->owner;
	if(dp){
		assert(dp->magic == DNmagic);
		for(nrp = dp->rr; nrp; nrp = nrp->next)
			assert(nrp != rp); /* "rrfree of live rr" */;
	}

	switch(rp->type){
	case Tsoa:
		freeserverlist(rp->soa->slaves);
		free(rp->soa);
		break;
	case Tkey:
		free(rp->key->data);
		free(rp->key);
		break;
	case Tcert:
		free(rp->cert->data);
		free(rp->cert);
		break;
	case Tsig:
		free(rp->sig->data);
		free(rp->sig);
		break;
	case Tnull:
		free(rp->null->data);
		free(rp->null);
		break;
	case Ttxt:
		while(rp->txt != nil){
			t = rp->txt;
			rp->txt = t->next;
			free(t->p);
			free(t);
		}
		break;
	}

	rp->magic = ~rp->magic;
	free(rp);
}

/*
 *  free a list of resource records and any related structs
 */
void
rrfreelist(RR *rp)
{
	RR *next;

	for(; rp; rp = next){
		next = rp->next;
		rrfree(rp);
	}
}

extern RR**
rrcopy(RR *rp, RR **last)
{
	RR *nrp;
	SOA *soa;
	Key *key;
	Cert *cert;
	Sig *sig;
	Null *null;
	Txt *t, *nt, **l;

	nrp = rralloc(rp->type);
	switch(rp->type){
	case Ttxt:
		*nrp = *rp;
		l = &nrp->txt;
		*l = nil;
		for(t = rp->txt; t != nil; t = t->next){
			nt = emalloc(sizeof(*nt));
			nt->p = estrdup(t->p);
			nt->next = nil;
			*l = nt;
			l = &nt->next;
		}
		break;
	case Tsoa:
		soa = nrp->soa;
		*nrp = *rp;
		nrp->soa = soa;
		*nrp->soa = *rp->soa;
		nrp->soa->slaves = copyserverlist(rp->soa->slaves);
		break;
	case Tkey:
		key = nrp->key;
		*nrp = *rp;
		nrp->key = key;
		*key = *rp->key;
		key->data = emalloc(key->dlen);
		memmove(key->data, rp->key->data, rp->key->dlen);
		break;
	case Tsig:
		sig = nrp->sig;
		*nrp = *rp;
		nrp->sig = sig;
		*sig = *rp->sig;
		sig->data = emalloc(sig->dlen);
		memmove(sig->data, rp->sig->data, rp->sig->dlen);
		break;
	case Tcert:
		cert = nrp->cert;
		*nrp = *rp;
		nrp->cert = cert;
		*cert = *rp->cert;
		cert->data = emalloc(cert->dlen);
		memmove(cert->data, rp->cert->data, rp->cert->dlen);
		break;
	case Tnull:
		null = nrp->null;
		*nrp = *rp;
		nrp->null = null;
		*null = *rp->null;
		null->data = emalloc(null->dlen);
		memmove(null->data, rp->null->data, rp->null->dlen);
		break;
	default:
		*nrp = *rp;
		break;
	}
	nrp->cached = 0;
	nrp->next = 0;
	*last = nrp;
	return &nrp->next;
}

/*
 *  lookup a resource record of a particular type and
 *  class attached to a domain name.  Return copies.
 *
 *  Priority ordering is:
 *	db authoritative
 *	not timed out network authoritative
 *	not timed out network unauthoritative
 *	unauthoritative db
 *
 *  if flag NOneg is set, don't return negative cached entries.
 *  return nothing instead.
 */
RR*
rrlookup(DN *dp, int type, int flag)
{
	RR *rp, *first, **last;

	assert(dp->magic == DNmagic);

	first = 0;
	last = &first;
	lock(&dnlock);

	/* try for an authoritative db entry */
	for(rp = dp->rr; rp; rp = rp->next){
		assert(rp->magic == RRmagic && rp->cached);
		if(rp->db)
		if(rp->auth)
		if(tsame(type, rp->type))
			last = rrcopy(rp, last);
	}
	if(first)
		goto out;

	/* try for an living authoritative network entry */
	for(rp = dp->rr; rp; rp = rp->next){
		if(!rp->db)
		if(rp->auth)
		if(rp->ttl + 60 > now)
		if(tsame(type, rp->type)){
			if(flag == NOneg && rp->negative)
				goto out;
			last = rrcopy(rp, last);
		}
	}
	if(first)
		goto out;

	/* try for an living unauthoritative network entry */
	for(rp = dp->rr; rp; rp = rp->next){
		if(!rp->db)
		if(rp->ttl + 60 > now)
		if(tsame(type, rp->type)){
			if(flag == NOneg && rp->negative)
				goto out;
			last = rrcopy(rp, last);
		}
	}
	if(first)
		goto out;

	/* try for an unauthoritative db entry */
	for(rp = dp->rr; rp; rp = rp->next){
		if(rp->db)
		if(tsame(type, rp->type))
			last = rrcopy(rp, last);
	}
	if(first)
		goto out;

	/* otherwise, settle for anything we got (except for negative caches)  */
	for(rp = dp->rr; rp; rp = rp->next){
		if(tsame(type, rp->type)){
			if(rp->negative)
				goto out;
			last = rrcopy(rp, last);
		}
	}

out:
	unlock(&dnlock);
	unique(first);
	return first;
}

/*
 *  convert an ascii RR type name to its integer representation
 */
int
rrtype(char *atype)
{
	int i;

	for(i = 0; i <= Tall; i++)
		if(rrtname[i] && strcmp(rrtname[i], atype) == 0)
			return i;

	/* make any a synonym for all */
	if(strcmp(atype, "any") == 0)
		return Tall;
	return atoi(atype);
}

/*
 *  convert an integer RR type to it's ascii name
 */
char*
rrname(int type, char *buf, int len)
{
	char *t;

	t = 0;
	if(type <= Tall)
		t = rrtname[type];
	if(t==0){
		snprint(buf, len, "%d", type);
		t = buf;
	}
	return t;
}

/*
 *  return 0 if not a supported rr type
 */
int
rrsupported(int type)
{
	if(type < 0 || type >Tall)
		return 0;
	return rrtname[type] != 0;
}

/*
 *  compare 2 types
 */
int
tsame(int t1, int t2)
{
	return t1 == t2 || t1 == Tall;
}

/*
 *  Add resource records to a list, duplicate them if they are cached
 *  RR's since these are shared.
 */
RR*
rrcat(RR **start, RR *rp)
{
	RR **last;

	last = start;
	while(*last != 0)
		last = &(*last)->next;

	*last = rp;
	return *start;
}

/*
 *  remove negative cache rr's from an rr list
 */
RR*
rrremneg(RR **l)
{
	RR **nl, *rp;
	RR *first;

	first = nil;
	nl = &first;
	while(*l != nil){
		rp = *l;
		if(rp->negative){
			*l = rp->next;
			*nl = rp;
			nl = &rp->next;
			*nl = nil;
		} else
			l = &rp->next;
	}

	return first;
}

/*
 *  remove rr's of a particular type from an rr list
 */
RR*
rrremtype(RR **l, int type)
{
	RR **nl, *rp;
	RR *first;

	first = nil;
	nl = &first;
	while(*l != nil){
		rp = *l;
		if(rp->type == type){
			*l = rp->next;
			*nl = rp;
			nl = &rp->next;
			*nl = nil;
		} else
			l = &(*l)->next;
	}

	return first;
}

/*
 *  print conversion for rr records
 */
int
rrfmt(Fmt *f)
{
	RR *rp;
	char *strp;
	Fmt fstr;
	int rv;
	char buf[Domlen];
	Server *s;
	Txt *t;

	fmtstrinit(&fstr);

	rp = va_arg(f->args, RR*);
	if(rp == 0){
		fmtprint(&fstr, "<null>");
		goto out;
	}

	fmtprint(&fstr, "%s %s", rp->owner->name,
		rrname(rp->type, buf, sizeof buf));

	if(rp->negative){
		fmtprint(&fstr, "\tnegative - rcode %d", rp->negrcode);
		goto out;
	}

	switch(rp->type){
	case Thinfo:
		fmtprint(&fstr, "\t%s %s", rp->cpu->name, rp->os->name);
		break;
	case Tcname:
	case Tmb:
	case Tmd:
	case Tmf:
	case Tns:
		fmtprint(&fstr, "\t%s", rp->host->name);
		break;
	case Tmg:
	case Tmr:
		fmtprint(&fstr, "\t%s", rp->mb->name);
		break;
	case Tminfo:
		fmtprint(&fstr, "\t%s %s", rp->mb->name, rp->rmb->name);
		break;
	case Tmx:
		fmtprint(&fstr, "\t%lud %s", rp->pref, rp->host->name);
		break;
	case Ta:
	case Taaaa:
		fmtprint(&fstr, "\t%s", rp->ip->name);
		break;
	case Tptr:
/*		fmtprint(&fstr, "\t%s(%lud)", rp->ptr->name, rp->ptr->ordinal); */
		fmtprint(&fstr, "\t%s", rp->ptr->name);
		break;
	case Tsoa:
		fmtprint(&fstr, "\t%s %s %lud %lud %lud %lud %lud", rp->host->name,
			rp->rmb->name, rp->soa->serial, rp->soa->refresh, rp->soa->retry,
			rp->soa->expire, rp->soa->minttl);
		for(s = rp->soa->slaves; s != nil; s = s->next)
			fmtprint(&fstr, " %s", s->name);
		break;
	case Tnull:
		fmtprint(&fstr, "\t%.*H", rp->null->dlen, rp->null->data);
		break;
	case Ttxt:
		fmtprint(&fstr, "\t");
		for(t = rp->txt; t != nil; t = t->next)
			fmtprint(&fstr, "%s", t->p);
		break;
	case Trp:
		fmtprint(&fstr, "\t%s %s", rp->rmb->name, rp->rp->name);
		break;
	case Tkey:
		fmtprint(&fstr, "\t%d %d %d", rp->key->flags, rp->key->proto,
			rp->key->alg);
		break;
	case Tsig:
		fmtprint(&fstr, "\t%d %d %d %lud %lud %lud %d %s",
			rp->sig->type, rp->sig->alg, rp->sig->labels, rp->sig->ttl,
			rp->sig->exp, rp->sig->incep, rp->sig->tag, rp->sig->signer->name);
		break;
	case Tcert:
		fmtprint(&fstr, "\t%d %d %d",
			rp->sig->type, rp->sig->tag, rp->sig->alg);
		break;
	default:
		break;
	}
out:
	strp = fmtstrflush(&fstr);
	rv = fmtstrcpy(f, strp);
	free(strp);
	return rv;
}

/*
 *  print conversion for rr records in attribute value form
 */
int
rravfmt(Fmt *f)
{
	RR *rp;
	char *strp;
	Fmt fstr;
	int rv;
	Server *s;
	Txt *t;
	int quote;

	fmtstrinit(&fstr);

	rp = va_arg(f->args, RR*);
	if(rp == 0){
		fmtprint(&fstr, "<null>");
		goto out;
	}

	if(rp->type == Tptr)
		fmtprint(&fstr, "ptr=%s", rp->owner->name);
	else
		fmtprint(&fstr, "dom=%s", rp->owner->name);

	switch(rp->type){
	case Thinfo:
		fmtprint(&fstr, " cpu=%s os=%s", rp->cpu->name, rp->os->name);
		break;
	case Tcname:
		fmtprint(&fstr, " cname=%s", rp->host->name);
		break;
	case Tmb:
	case Tmd:
	case Tmf:
		fmtprint(&fstr, " mbox=%s", rp->host->name);
		break;
	case Tns:
		fmtprint(&fstr,  " ns=%s", rp->host->name);
		break;
	case Tmg:
	case Tmr:
		fmtprint(&fstr, " mbox=%s", rp->mb->name);
		break;
	case Tminfo:
		fmtprint(&fstr, " mbox=%s mbox=%s", rp->mb->name, rp->rmb->name);
		break;
	case Tmx:
		fmtprint(&fstr, " pref=%lud mx=%s", rp->pref, rp->host->name);
		break;
	case Ta:
	case Taaaa:
		fmtprint(&fstr, " ip=%s", rp->ip->name);
		break;
	case Tptr:
		fmtprint(&fstr, " dom=%s", rp->ptr->name);
		break;
	case Tsoa:
		fmtprint(&fstr, " ns=%s mbox=%s serial=%lud refresh=%lud retry=%lud expire=%lud ttl=%lud",
			rp->host->name, rp->rmb->name, rp->soa->serial,
			rp->soa->refresh, rp->soa->retry,
			rp->soa->expire, rp->soa->minttl);
		for(s = rp->soa->slaves; s != nil; s = s->next)
			fmtprint(&fstr, " dnsslave=%s", s->name);
		break;
	case Tnull:
		fmtprint(&fstr, " null=%.*H", rp->null->dlen, rp->null->data);
		break;
	case Ttxt:
		fmtprint(&fstr, " txt=");
		quote = 0;
		for(t = rp->txt; t != nil; t = t->next)
			if(strchr(t->p, ' '))
				quote = 1;
		if(quote)
			fmtprint(&fstr, "\"");
		for(t = rp->txt; t != nil; t = t->next)
			fmtprint(&fstr, "%s", t->p);
		if(quote)
			fmtprint(&fstr, "\"");
		break;
	case Trp:
		fmtprint(&fstr, " rp=%s txt=%s", rp->rmb->name, rp->rp->name);
		break;
	case Tkey:
		fmtprint(&fstr, " flags=%d proto=%d alg=%d",
			rp->key->flags, rp->key->proto, rp->key->alg);
		break;
	case Tsig:
		fmtprint(&fstr, " type=%d alg=%d labels=%d ttl=%lud exp=%lud incep=%lud tag=%d signer=%s",
			rp->sig->type, rp->sig->alg, rp->sig->labels, rp->sig->ttl,
			rp->sig->exp, rp->sig->incep, rp->sig->tag, rp->sig->signer->name);
		break;
	case Tcert:
		fmtprint(&fstr, " type=%d tag=%d alg=%d",
			rp->sig->type, rp->sig->tag, rp->sig->alg);
		break;
	default:
		break;
	}
out:
	strp = fmtstrflush(&fstr);
	rv = fmtstrcpy(f, strp);
	free(strp);
	return rv;
}

void
warning(char *fmt, ...)
{
	char dnserr[128];
	va_list arg;

	va_start(arg, fmt);
	vseprint(dnserr, dnserr+sizeof(dnserr), fmt, arg);
	va_end(arg);
	syslog(1, "dns", dnserr);
}

/*
 *  chasing down double free's
 */
void
dncheck(void *p, int dolock)
{
	DN *dp;
	int i;
	RR *rp;

	if(p != nil){
		dp = p;
		assert(dp->magic == DNmagic);
	}

	if(!testing)
		return;

	if(dolock)
		lock(&dnlock);
	for(i = 0; i < HTLEN; i++)
		for(dp = ht[i]; dp; dp = dp->next){
			assert(dp != p);
			assert(dp->magic == DNmagic);
			for(rp = dp->rr; rp; rp = rp->next){
				assert(rp->magic == RRmagic);
				assert(rp->cached);
				assert(rp->owner == dp);
			}
		}
	if(dolock)
		unlock(&dnlock);
}

static int
rrequiv(RR *r1, RR *r2)
{
	return r1->owner == r2->owner
		&& r1->type == r2->type
		&& r1->arg0 == r2->arg0
		&& r1->arg1 == r2->arg1;
}

void
unique(RR *rp)
{
	RR **l, *nrp;

	for(; rp; rp = rp->next){
		l = &rp->next;
		for(nrp = *l; nrp; nrp = *l){
			if(rrequiv(rp, nrp)){
				*l = nrp->next;
				rrfree(nrp);
			} else
				l = &nrp->next;
		}
	}
}

/*
 *  true if second domain is subsumed by the first
 */
int
subsume(char *higher, char *lower)
{
	int hn, ln;

	ln = strlen(lower);
	hn = strlen(higher);
	if(ln < hn)
		return 0;

	if(cistrcmp(lower + ln - hn, higher) != 0)
		return 0;

	if(ln > hn && hn != 0 && lower[ln - hn - 1] != '.')
		return 0;

	return 1;
}

/*
 *  randomize the order we return items to provide some
 *  load balancing for servers.
 *
 *  only randomize the first class of entries
 */
RR*
randomize(RR *rp)
{
	RR *first, *last, *x, *base;
	ulong n;

	if(rp == nil || rp->next == nil)
		return rp;

	/* just randomize addresses and mx's */
	for(x = rp; x; x = x->next)
		if(x->type != Ta && x->type != Tmx && x->type != Tns)
			return rp;

	base = rp;

	n = rand();
	last = first = nil;
	while(rp != nil){
		/* stop randomizing if we've moved past our class */
		if(base->auth != rp->auth || base->db != rp->db){
			last->next = rp;
			break;
		}

		/* unchain */
		x = rp;
		rp = x->next;
		x->next = nil;

		if(n&1){
			/* add to tail */
			if(last == nil)
				first = x;
			else
				last->next = x;
			last = x;
		} else {
			/* add to head */
			if(last == nil)
				last = x;
			x->next = first;
			first = x;
		}

		/* reroll the dice */
		n >>= 1;
	}
	return first;
}

static int
sencodefmt(Fmt *f)
{
	char *out;
	char *buf;
	int i, len;
	int ilen;
	int rv;
	uchar *b;
	char obuf[64];	/* rsc optimization */

	if(!(f->flags&FmtPrec) || f->prec < 1)
		goto error;

	b = va_arg(f->args, uchar*);
	if(b == nil)
		goto error;

	/* if it's a printable, go for it */
	len = f->prec;
	for(i = 0; i < len; i++)
		if(!isprint(b[i]))
			break;
	if(i == len){
		if(len >= sizeof obuf)
			len = sizeof(obuf)-1;
		memmove(obuf, b, len);
		obuf[len] = 0;
		fmtstrcpy(f, obuf);
		return 0;
	}

	ilen = f->prec;
	f->prec = 0;
	f->flags &= ~FmtPrec;
	switch(f->r){
	case '<':
		len = (8*ilen+4)/5 + 3;
		break;
	case '[':
		len = (8*ilen+5)/6 + 4;
		break;
	case 'H':
		len = 2*ilen + 1;
		break;
	default:
		goto error;
	}

	if(len > sizeof(obuf)){
		buf = malloc(len);
		if(buf == nil)
			goto error;
	} else
		buf = obuf;

	/* convert */
	out = buf;
	switch(f->r){
	case '<':
		rv = enc32(out, len, b, ilen);
		break;
	case '[':
		rv = enc64(out, len, b, ilen);
		break;
	case 'H':
		rv = enc16(out, len, b, ilen);
		break;
	default:
		rv = -1;
		break;
	}
	if(rv < 0)
		goto error;

	fmtstrcpy(f, buf);
	if(buf != obuf)
		free(buf);
	return 0;

error:
	return fmtstrcpy(f, "<encodefmt>");

}

void*
emalloc(int size)
{
	char *x;

	x = mallocz(size, 1);
	if(x == nil)
		abort();
	setmalloctag(x, getcallerpc(&size));
	return x;
}

char*
estrdup(char *s)
{
	int size;
	char *p;

	size = strlen(s)+1;
	p = mallocz(size, 0);
	if(p == nil)
		abort();
	memmove(p, s, size);
	setmalloctag(p, getcallerpc(&s));
	return p;
}

/*
 *  create a pointer record
 */
static RR*
mkptr(DN *dp, char *ptr, ulong ttl)
{
	DN *ipdp;
	RR *rp;

	ipdp = dnlookup(ptr, Cin, 1);

	rp = rralloc(Tptr);
	rp->ptr = dp;
	rp->owner = ipdp;
	rp->db = 1;
	if(ttl)
		rp->ttl = ttl;
	return rp;
}

/*
 *  look for all ip addresses in this network and make
 *  pointer records for them.
 */
void
dnptr(uchar *net, uchar *mask, char *dom, int bytes, int ttl)
{
	int i, j;
	DN *dp;
	RR *rp, *nrp, *first, **l;
	uchar ip[IPaddrlen];
	uchar nnet[IPaddrlen];
	char ptr[Domlen];
	char *p, *e;

	l = &first;
	first = nil;
	for(i = 0; i < HTLEN; i++){
		for(dp = ht[i]; dp; dp = dp->next){
			for(rp = dp->rr; rp; rp = rp->next){
				if(rp->type != Ta || rp->negative)
					continue;
				parseip(ip, rp->ip->name);
				maskip(ip, mask, nnet);
				if(ipcmp(net, nnet) != 0)
					continue;
				p = ptr;
				e = ptr+sizeof(ptr);
				for(j = IPaddrlen-1; j >= IPaddrlen-bytes; j--)
					p = seprint(p, e, "%d.", ip[j]);
				seprint(p, e, "%s", dom);
				nrp = mkptr(dp, ptr, ttl);
				*l = nrp;
				l = &nrp->next;
			}
		}
	}

	for(rp = first; rp != nil; rp = nrp){
		nrp = rp->next;
		rp->next = nil;
		rrattach(rp, 1);
	}
}

void
freeserverlist(Server *s)
{
	Server *next;

	for(; s != nil; s = next){
		next = s->next;
		free(s);
	}
}

void
addserver(Server **l, char *name)
{
	Server *s;

	while(*l)
		l = &(*l)->next;
	s = malloc(sizeof(Server)+strlen(name)+1);
	if(s == nil)
		return;
	s->name = (char*)(s+1);
	strcpy(s->name, name);
	s->next = nil;
	*l = s;
}

Server*
copyserverlist(Server *s)
{
	Server *ns;


	for(ns = nil; s != nil; s = s->next)
		addserver(&ns, s->name);
	return ns;
}
