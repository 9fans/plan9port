#include "std.h"
#include "dat.h"

Ring ring;

Key*
keyiterate(int skip, char *fmt, ...)
{
	int i;
	Attr *a;
	Key *k;
	va_list arg;

	va_start(arg, fmt);
	a = parseattrfmtv(fmt, arg);
	va_end(arg);

	for(i=0; i<ring.nkey; i++){
		k = ring.key[i];
		if(matchattr(a, k->attr, k->privattr)){
			if(skip-- > 0)
				continue;
			k->ref++;
			freeattr(a);
			return k;
		}
	}
	freeattr(a);
	werrstr("no key found");
	return nil;
}

Key*
keylookup(char *fmt, ...)
{
	int i;
	Attr *a;
	Key *k;
	va_list arg;

	va_start(arg, fmt);
	a = parseattrfmtv(fmt, arg);
	va_end(arg);

	for(i=0; i<ring.nkey; i++){
		k = ring.key[i];
		if(matchattr(a, k->attr, k->privattr)){
			k->ref++;
			freeattr(a);
			return k;
		}
	}
	freeattr(a);
	werrstr("no key found");
	return nil;
}

Key*
keyfetch(Conv *c, char *fmt, ...)
{
	int i, tag;
	Attr *a;
	Key *k;
	va_list arg;

	va_start(arg, fmt);
	a = parseattrfmtv(fmt, arg);
	va_end(arg);

	flog("keyfetch %A", a);
	tag = 0;

	for(i=0; i<ring.nkey; i++){
		k = ring.key[i];
		if(tag < k->tag)
			tag = k->tag;
		if(matchattr(a, k->attr, k->privattr)){
			k->ref++;
			if(strfindattr(k->attr, "confirm") && confirmkey(c, k) != 1){
				k->ref--;
				continue;
			}
			freeattr(a);
			flog("using key %A %N", k->attr, k->privattr);
			return k;
		}
	}

	if(needkey(c, a) < 0)
		convneedkey(c, a);

	for(i=0; i<ring.nkey; i++){
		k = ring.key[i];
		if(k->tag <= tag)
			continue;
		if(matchattr(a, k->attr, k->privattr)){
			k->ref++;
			if(strfindattr(k->attr, "confirm") && confirmkey(c, k) != 1){
				k->ref--;
				continue;
			}
			freeattr(a);
			return k;
		}
	}
	freeattr(a);
	werrstr("no key found");
	return nil;
}

static int taggen;

void
keyadd(Key *k)
{
	int i;

	k->ref++;
	k->tag = ++taggen;
	for(i=0; i<ring.nkey; i++){
		if(matchattr(k->attr, ring.key[i]->attr, nil)
		&& matchattr(ring.key[i]->attr, k->attr, nil)){
			keyclose(ring.key[i]);
			ring.key[i] = k;
			return;
		}
	}

	ring.key = erealloc(ring.key, (ring.nkey+1)*sizeof(ring.key[0]));
	ring.key[ring.nkey++] = k;
}

void
keyclose(Key *k)
{
	if(k == nil)
		return;

	if(--k->ref > 0)
		return;

	if(k->proto->closekey)
		(*k->proto->closekey)(k);

	freeattr(k->attr);
	freeattr(k->privattr);
	free(k);
}

Key*
keyreplace(Conv *c, Key *k, char *fmt, ...)
{
	Key *kk;
	char *msg;
	Attr *a, *b, *bp;
	va_list arg;

	va_start(arg, fmt);
	msg = vsmprint(fmt, arg);
	if(msg == nil)
		sysfatal("out of memory");
	va_end(arg);

	/* replace prompted values with prompts */
	a = copyattr(k->attr);
	bp = parseattr(k->proto->keyprompt);
	for(b=bp; b; b=b->next){
		a = delattr(a, b->name);
		a = addattr(a, "%q?", b->name);
	}
	freeattr(bp);

	if(badkey(c, k, msg, a) < 0)
		convbadkey(c, k, msg, a);
	kk = keylookup("%A", a);
	freeattr(a);
	keyclose(k);
	if(kk == k){
		keyclose(kk);
		werrstr("%s", msg);
		return nil;
	}

	if(strfindattr(kk->attr, "confirm")){
		if(confirmkey(c, kk) != 1){
			werrstr("key use not confirmed");
			keyclose(kk);
			return nil;
		}
	}
	return kk;
}

void
keyevict(Conv *c, Key *k, char *fmt, ...)
{
	char *msg;
	Attr *a, *b, *bp;
	va_list arg;

	va_start(arg, fmt);
	msg = vsmprint(fmt, arg);
	if(msg == nil)
		sysfatal("out of memory");
	va_end(arg);

	/* replace prompted values with prompts */
	a = copyattr(k->attr);
	bp = parseattr(k->proto->keyprompt);
	for(b=bp; b; b=b->next){
		a = delattr(a, b->name);
		a = addattr(a, "%q?", b->name);
	}
	freeattr(bp);

	if(badkey(c, k, msg, nil) < 0)
		convbadkey(c, k, msg, nil);
	keyclose(k);
}
