#include <u.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>

static uchar noether[6];
	Ndb *db;

static void
recursesubnet(Ndb *db, uchar *addr, uchar *mask, char *attr, char *name, char *name1)
{
	Ndbs s;
	Ndbtuple *t, *nt;
	uchar submask[IPaddrlen], net[IPaddrlen];
	char ip[Ndbvlen];
	int found;

	maskip(addr, mask, net);
	sprint(ip, "%I", net);
	t = ndbsearch(db, &s, "ip", ip);
	if(t == 0)
		return;

	for(nt = t; nt; nt = nt->entry){
		if(strcmp(nt->attr, "ipmask") == 0){
			parseip(submask, nt->val);
			if(memcmp(submask, mask, IPaddrlen) != 0)
				recursesubnet(db, addr, submask, attr, name, name1);
			break;
		}
	}

	if(name[0] == 0){
		found = 0;
		for(nt = t; nt; nt = nt->entry){
			if(strcmp(nt->attr, attr) == 0){
				if(found){
					strcpy(name, nt->val);
					name1[0] = 0;
					found = 1;
				} else {
					strcpy(name1, nt->val);
					break;
				}
			}
		}
	}

	ndbfree(t);
}

/*
 *  lookup an ip address
 */
static int
getipaddr(Ndb *db, char *name, uchar *to, Ipinfo *iip)
{
	Ndbtuple *t, *nt;
	char buf[Ndbvlen];
	uchar subnet[IPaddrlen];
	Ndbs s;
	char *attr;

	attr = ipattr(name);
	if(strcmp(attr, "ip") == 0){
		parseip(to, name);
		return 1;
	}

	t = ndbgetval(db, &s, attr, name, "ip", buf);
	if(t){
		/* first look for match on same subnet */
		for(nt = t; nt; nt = nt->entry){
			if(strcmp(nt->attr, "ip") != 0)
				continue;
			parseip(to, nt->val);
			maskip(to, iip->ipmask, subnet);
			if(memcmp(subnet, iip->ipnet, sizeof(subnet)) == 0)
				return 1;
		}

		/* otherwise, just take what we have */
		ndbfree(t);
		parseip(to, buf);
		return 1;
	}
	return 0;
}

/*
 *  return the ip addresses for a type of server for system ip
 */
int
lookupserver(char *attr, uchar ipaddrs[2][IPaddrlen], Ipinfo *iip)
{
	Ndbtuple *t, *nt;
	Ndbs s;
	char ip[32];
	char name[Ndbvlen];
	char name1[Ndbvlen];
	int i;

	name[0] = name1[0] = 0;

	snprint(ip, sizeof(ip), "%I", iip->ipaddr);
	t = ndbsearch(db, &s, "ip", ip);
	while(t){
		for(nt = t; nt; nt = nt->entry){
			if(strcmp(attr, nt->attr) == 0){
				if(*name == 0)
					strcpy(name, nt->val);
				else {
					strcpy(name1, nt->val);
					break;
				}
			}
		}
		if(name[0])
			break;
		t = ndbsnext(&s, "ip", ip);
	}

	if(name[0] == 0)
		recursesubnet(db, iip->ipaddr, classmask[CLASS(iip->ipaddr)], attr, name, name1);

	i = 0;
	if(name[0] && getipaddr(db, name, *ipaddrs, iip) == 1){
		ipaddrs++;
		i++;
	}
	if(name1[0] && getipaddr(db, name1, *ipaddrs, iip) == 1)
		i++;
	return i;
}

void
main(int argc, char **argv)
{
	Ipinfo ii;
	uchar addrs[2][IPaddrlen];
	int i, j;

	db = ndbopen(0);

	fmtinstall('E', eipconv);
	fmtinstall('I', eipconv);
	if(argc < 2)
		exits(0);
	if(strchr(argv[1], '.')){
		if(ipinfo(db, 0, argv[1], 0, &ii) < 0)
			exits(0);
	} else {
		if(ipinfo(db, argv[1], 0, 0, &ii) < 0)
			exits(0);
	}
	print("a %I m %I n %I f %s e %E a %I\n", ii.ipaddr,
		ii.ipmask, ii.ipnet, ii.bootf, ii.etheraddr, ii.auip);

	i = lookupserver("auth", addrs, &ii);
	print("lookupserver returns %d\n", i);
	for(j = 0; j < i; j++)
		print("%I\n", addrs[j]);
	i = lookupserver("dns", addrs, &ii);
	print("lookupserver returns %d\n", i);
	for(j = 0; j < i; j++)
		print("%I\n", addrs[j]);
}
