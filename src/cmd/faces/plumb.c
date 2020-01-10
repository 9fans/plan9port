#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <regexp.h>
#include <bio.h>
#include <9pclient.h>
#include <plumb.h>
#include "faces.h"

static CFid*	showfd;
static CFid*	seefd;

char		**maildirs;
int		nmaildirs;

void
initplumb(void)
{
	showfd = plumbopenfid("send", OWRITE);
	seefd = plumbopenfid("seemail", OREAD);
	if(showfd == nil || seefd == nil)
		sysfatal("plumbopen: %r");
}

void
addmaildir(char *dir)
{
	maildirs = erealloc(maildirs, (nmaildirs+1)*sizeof(char*));
	maildirs[nmaildirs++] = dir;
}

char*
attr(Face *f)
{
	static char buf[128];

	if(f->str[Sdigest]){
		snprint(buf, sizeof buf, "digest=%s", f->str[Sdigest]);
		return buf;
	}
	return nil;
}

void
showmail(Face *f)
{
	char buf[256];
	Plumbmsg pm;
	Plumbattr a;

	if(showfd<0 || f->str[Sshow]==nil || f->str[Sshow][0]=='\0')
		return;
	snprint(buf, sizeof buf, "Mail/%s", f->str[Sshow]);
	pm.src = "faces";
	pm.dst = "showmail";
	pm.wdir = "/";
	pm.type = "text";
	a.name = "digest";
	a.value = f->str[Sdigest];
	a.next = nil;
	pm.attr = &a;
	pm.ndata = strlen(buf);
	pm.data = buf;
	plumbsendtofid(showfd, &pm);
}

char*
value(Plumbattr *attr, char *key, char *def)
{
	char *v;

	v = plumblookup(attr, key);
	if(v)
		return v;
	return def;
}

void
setname(Face *f, char *sender)
{
	char *at, *bang;
	char *p;
	char *fld[3];
	int nf;

	p = estrdup(sender);
	nf = tokenize(p, fld, 3);
	if(nf <= 1)
		sender = estrdup(fld[0]);
	else
		sender = estrdup(fld[1]);
	free(p);

	/* works with UTF-8, although it's written as ASCII */
	for(p=sender; *p!='\0'; p++)
		*p = tolower(*p);
	f->str[Suser] = sender;
	at = strchr(sender, '@');
	if(at){
		*at++ = '\0';
		f->str[Sdomain] = estrdup(at);
		return;
	}
	bang = strchr(sender, '!');
	if(bang){
		*bang++ = '\0';
		f->str[Suser] = estrdup(bang);
		f->str[Sdomain] = sender;
		return;
	}
}

static char* months[] = {
	"jan", "feb", "mar", "apr",
	"may", "jun", "jul", "aug",
	"sep", "oct", "nov", "dec"
};

static int
getmon(char *s)
{
	int i;

	for(i=0; i<nelem(months); i++)
		if(cistrcmp(months[i], s) == 0)
			return i;
	return -1;
}

/* Fri Jul 23 14:05:14 EDT 1999 */
ulong
parsedatev(char **a)
{
	char *p;
	Tm tm;

	memset(&tm, 0, sizeof tm);
	if((tm.mon=getmon(a[1])) == -1)
		goto Err;
	tm.mday = strtol(a[2], &p, 10);
	if(*p != '\0')
		goto Err;
	tm.hour = strtol(a[3], &p, 10);
	if(*p != ':')
		goto Err;
	tm.min = strtol(p+1, &p, 10);
	if(*p != ':')
		goto Err;
	tm.sec = strtol(p+1, &p, 10);
	if(*p != '\0')
		goto Err;
	if(strlen(a[4]) != 3)
		goto Err;
	strcpy(tm.zone, a[4]);
	if(strlen(a[5]) != 4)
		goto Err;
	tm.year = strtol(a[5], &p, 10);
	if(*p != '\0')
		goto Err;
	tm.year -= 1900;
	return tm2sec(&tm);
Err:
	return time(0);
}

ulong
parsedate(char *s)
{
	char *f[10];
	int nf;

	nf = getfields(s, f, nelem(f), 1, " ");
	if(nf < 6)
		return time(0);
	return parsedatev(f);
}


char*
tweakdate(char *d)
{
	char e[8];

	/* d, date = "Mon Aug  2 23:46:55 EDT 1999" */

	if(strlen(d) < strlen("Mon Aug  2 23:46:55 EDT 1999"))
		return estrdup("");
	if(strncmp(date, d, 4+4+3) == 0)
		snprint(e, sizeof e, "%.5s", d+4+4+3);	/* 23:46 */
	else
		snprint(e, sizeof e, "%.6s", d+4);	/* Aug  2 */
	return estrdup(e);
}

Face*
nextface(void)
{
	int i;
	Face *f;
	Plumbmsg *m;
	char *t, *data, *showmailp, *digestp;
	ulong xtime;

	f = emalloc(sizeof(Face));
	for(;;){
		m = plumbrecvfid(seefd);
		if(m == nil)
			killall("error on seemail plumb port");
		if(strncmp(m->data, "Mail/", 5) != 0){
			plumbfree(m);
			continue;
		}
		data = m->data+5;
		t = value(m->attr, "mailtype", "");
		if(strcmp(t, "delete") == 0)
			delete(data, value(m->attr, "digest", nil));
		else if(strcmp(t, "new") != 0)
			fprint(2, "faces: unknown plumb message type %s\n", t);
		else for(i=0; i<nmaildirs; i++)
			if(strncmp(data, maildirs[i], strlen(maildirs[i])) == 0)
				goto Found;
		plumbfree(m);
		continue;

	Found:
		xtime = parsedate(value(m->attr, "date", date));
		digestp = value(m->attr, "digest", nil);
		if(alreadyseen(digestp)){
			/* duplicate upas/fs can send duplicate messages */
			plumbfree(m);
			continue;
		}
		showmailp = estrdup(data);
		if(digestp)
			digestp = estrdup(digestp);
		setname(f, value(m->attr, "sender", "???"));
		plumbfree(m);
		f->time = xtime;
		f->tm = *localtime(xtime);
		f->str[Sshow] = showmailp;
		f->str[Sdigest] = digestp;
		return f;
	}
	return nil;
}

char*
iline(char *data, char **pp)
{
	char *p;

	if(*data == 0)
		return nil;
	for(p=data; *p!='\0' && *p!='\n'; p++)
		;
	if(*p == '\n')
		*p++ = '\0';
	*pp = p;
	return data;
}

Face*
dirface(char *dir, char *num)
{
	Face *f;
	char buf[1024],  *info, *p, *t, *s;
	int n;
	ulong len;
	CFid *fid;

	sprint(buf, "%s/%s/info", dir, num);
	len = fsdirlen(mailfs, buf);
	if(len <= 0)
		return nil;
	fid = fsopen(mailfs, buf, OREAD);
	if(fid == nil)
		return nil;
	info = emalloc(len+1);
	n = fsreadn(fid, info, len);
	fsclose(fid);
	if(n < 0){
		free(info);
		return nil;
	}
	info[n] = '\0';
	f = emalloc(sizeof(Face));
	for(p=info; (s=iline(p, &p)) != nil; ){
		t = strchr(s, ' ');
		if(t == nil)
			continue;
		*t++ = 0;
		if(strcmp(s, "unixdate") == 0){
			f->time = atoi(t);
			f->tm = *localtime(f->time);
		}
		else if(strcmp(s, "from") == 0)
			setname(f, t);
		else if(strcmp(s, "digest") == 0)
			f->str[Sdigest] = estrdup(t);
	}
	sprint(buf, "%s/%s", dir, num);
	f->str[Sshow] = estrdup(buf);
	free(info);
	return f;
}
