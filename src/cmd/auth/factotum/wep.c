/*
 * Copy WEP key to ethernet device.
 */

#include "std.h"
#include "dat.h"

static int
wepclient(Conv *c)
{
	char *dev, buf[128], *p, *kp;
	Key *k;
	int ret, fd, cfd;

	fd = cfd = -1;
	ret = -1;
	dev = nil;

	if((k = keylookup("%A !key1?", c->attr)) == nil
	&& (k = keylookup("%A !key2?", c->attr)) == nil
	&& (k = keylookup("%A !key3?", c->attr)) == nil){
		werrstr("cannot find wep keys");
		goto out;
	}
	if(convreadm(c, &dev) < 0)
		return -1;
	if(dev[0] != '#' || dev[1] != 'l'){
		werrstr("not an ethernet device: %s", dev);
		goto out;
	}
	snprint(buf, sizeof buf, "%s!0", dev);
	if((fd = dial(buf, 0, 0, &cfd)) < 0)
		goto out;
	if(!(p = strfindattr(k->privattr, kp="!key1"))
	&& !(p = strfindattr(k->privattr, kp="key2"))
	&& !(p = strfindattr(k->privattr, kp="key3"))){
		werrstr("lost key");
		goto out;
	}
	if(fprint(cfd, "%s %q", kp+1, p) < 0)
		goto out;
	if((p = strfindattr(k->attr, "essid")) != nil
	&& fprint(cfd, "essid %q", p) < 0)
		goto out;
	if(fprint(cfd, "crypt on") < 0)
		goto out;
	ret = 0;

out:
	free(dev);
	if(cfd >= 0)
		close(cfd);
	if(fd >= 0)
		close(fd);
	keyclose(k);
	return ret;
}

static int
wepcheck(Key *k)
{
	if(strfindattr(k->privattr, "!key1") == nil
	&& strfindattr(k->privattr, "!key2") == nil
	&& strfindattr(k->privattr, "!key3") == nil){
		werrstr("need !key1, !key2, or !key3 attribute");
		return -1;
	}
	return 0;
}

static Role weproles[] = {
	"client",	wepclient,
	0
};

Proto wep =
{
	"wep",
	weproles,
	nil,
	wepcheck,
	nil
};
