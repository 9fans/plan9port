#include "std.h"
#include "dat.h"

/*
 *	key attr=val... - add a key
 *		the attr=val pairs are protocol-specific.
 *		for example, both of these are valid:
 *			key p9sk1 gre cs.bell-labs.com mysecret
 *			key p9sk1 gre cs.bell-labs.com 11223344556677 fmt=des7hex
 *	delkey ... - delete a key
 *		if given, the attr=val pairs are used to narrow the search
 *		[maybe should require a password?]
 *
 *	debug - toggle debugging
 */

static char *msg[] = {
	"key",
	"delkey",
	"debug"
};

static int
classify(char *s)
{
	int i;

	for(i=0; i<nelem(msg); i++)
		if(strcmp(msg[i], s) == 0)
			return i;
	return -1;
}

int
ctlwrite(char *a)
{
	char *p;
	int i, nmatch, ret;
	Attr *attr, *kpa, **l, **lpriv, **lprotos, *pa, *priv, *protos;
	Key *k;
	Proto *proto;

	while(*a == ' ' || *a == '\t' || *a == '\n')
		a++;

	if(a[0] == '#' || a[0] == '\0')
		return 0;

	/*
	 * it would be nice to emit a warning of some sort here.
	 * we ignore all but the first line of the write.  this helps
	 * both with things like "echo delkey >/mnt/factotum/ctl"
	 * and writes that (incorrectly) contain multiple key lines.
	 */
	if(p = strchr(a, '\n')){
		if(p[1] != '\0'){
			werrstr("multiline write not allowed");
			return -1;
		}
		*p = '\0';
	}

	if((p = strchr(a, ' ')) == nil)
		p = "";
	else
		*p++ = '\0';
	switch(classify(a)){
	default:
		werrstr("unknown verb %s", a);
		return -1;
	case 0:	/* key */
		attr = parseattr(p);
		/* separate out proto= attributes */
		lprotos = &protos;
		for(l=&attr; (*l); ){
			if(strcmp((*l)->name, "proto") == 0){
				*lprotos = *l;
				lprotos = &(*l)->next;
				*l = (*l)->next;
			}else
				l = &(*l)->next;
		}
		*lprotos = nil;
		if(protos == nil){
			werrstr("key without protos");
			freeattr(attr);
			return -1;
		}

		/* separate out private attributes */
		lpriv = &priv;
		for(l=&attr; (*l); ){
			if((*l)->name[0] == '!'){
				*lpriv = *l;
				lpriv = &(*l)->next;
				*l = (*l)->next;
			}else
				l = &(*l)->next;
		}
		*lpriv = nil;
		flog("addkey %A %A %N", protos, attr, priv);

		/* add keys */
		ret = 0;
		for(pa=protos; pa; pa=pa->next){
			if((proto = protolookup(pa->val)) == nil){
				werrstr("unknown proto %s", pa->val);
				flog("addkey: %r");
				ret = -1;
				continue;
			}
			if(proto->keyprompt){
				kpa = parseattr(proto->keyprompt);
				if(!matchattr(kpa, attr, priv)){
					freeattr(kpa);
					werrstr("missing attributes -- want %s", proto->keyprompt);
					flog("addkey %s: %r", proto->name);
					ret = -1;
					continue;
				}
				freeattr(kpa);
			}
			k = emalloc(sizeof(Key));
			k->attr = mkattr(AttrNameval, "proto", proto->name, copyattr(attr));
			k->privattr = copyattr(priv);
			k->ref = 1;
			k->proto = proto;
			if(proto->checkkey && (*proto->checkkey)(k) < 0){
				flog("addkey %s: %r", proto->name);
				ret = -1;
				keyclose(k);
				continue;
			}
			flog("adding key: %A %N", k->attr, k->privattr);
			keyadd(k);
			keyclose(k);
		}
		freeattr(attr);
		freeattr(priv);
		freeattr(protos);
		return ret;
	case 1:	/* delkey */
		nmatch = 0;
		attr = parseattr(p);
		flog("delkey %A", attr);
		for(pa=attr; pa; pa=pa->next){
			if(pa->type != AttrQuery && pa->name[0]=='!'){
				werrstr("only !private? patterns are allowed for private fields");
				freeattr(attr);
				return -1;
			}
		}
		for(i=0; i<ring.nkey; ){
			if(matchattr(attr, ring.key[i]->attr, ring.key[i]->privattr)){
				nmatch++;
				flog("deleting %A %N", ring.key[i]->attr, ring.key[i]->privattr);
				keyclose(ring.key[i]);
				ring.nkey--;
				memmove(&ring.key[i], &ring.key[i+1], (ring.nkey-i)*sizeof(ring.key[0]));
			}else
				i++;
		}
		freeattr(attr);
		if(nmatch == 0){
			werrstr("found no keys to delete");
			return -1;
		}
		return 0;
	case 2:	/* debug */
		debug ^= 1;
		flog("debug = %d", debug);
		return 0;
	}
}
