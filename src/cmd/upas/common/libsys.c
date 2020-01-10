#include <u.h>
#include <sys/types.h>
#include <pwd.h>
#include <netdb.h>
#include "common.h"
#include <auth.h>
#include <ndb.h>

/*
 *  number of predefined fd's
 */
int nsysfile=3;

static char err[Errlen];

/*
 *  return the date
 */
extern char *
thedate(void)
{
	static char now[64];
	char *cp;

	strcpy(now, ctime(time(0)));
	cp = strchr(now, '\n');
	if(cp)
		*cp = 0;
	return now;
}

/*
 *  return the user id of the current user
 */
extern char *
getlog(void)
{
	return getuser();
}

/*
 *  return the lock name (we use one lock per directory)
 */
static String *
lockname(char *path)
{
	String *lp;
	char *cp;

	/*
	 *  get the name of the lock file
	 */
	lp = s_new();
	cp = strrchr(path, '/');
	if(cp)
		s_nappend(lp, path, cp - path + 1);
	s_append(lp, "L.mbox");

	return lp;
}

int
syscreatelocked(char *path, int mode, int perm)
{
	return create(path, mode, DMEXCL|perm);
}

int
sysopenlocked(char *path, int mode)
{
/*	return open(path, OEXCL|mode);/**/
	return open(path, mode);		/* until system call is fixed */
}

int
sysunlockfile(int fd)
{
	return close(fd);
}

/*
 *  try opening a lock file.  If it doesn't exist try creating it.
 */
static int
openlockfile(Mlock *l)
{
	int fd;
	Dir *d;
	Dir nd;
	char *p;

	fd = open(s_to_c(l->name), OREAD);
	if(fd >= 0){
		l->fd = fd;
		return 0;
	}

	d = dirstat(s_to_c(l->name));
	if(d == nil){
		/* file doesn't exist */
		/* try creating it */
		fd = create(s_to_c(l->name), OREAD, DMEXCL|0666);
		if(fd >= 0){
			nulldir(&nd);
			nd.mode = DMEXCL|0666;
			if(dirfwstat(fd, &nd) < 0){
				/* if we can't chmod, don't bother */
				/* live without the lock but log it */
				syslog(0, "mail", "lock error: %s: %r", s_to_c(l->name));
				remove(s_to_c(l->name));
			}
			l->fd = fd;
			return 0;
		}

		/* couldn't create */
		/* do we have write access to the directory? */
		p = strrchr(s_to_c(l->name), '/');
		if(p != 0){
			*p = 0;
			fd = access(s_to_c(l->name), 2);
			*p = '/';
			if(fd < 0){
				/* live without the lock but log it */
				syslog(0, "mail", "lock error: %s: %r", s_to_c(l->name));
				return 0;
			}
		} else {
			fd = access(".", 2);
			if(fd < 0){
				/* live without the lock but log it */
				syslog(0, "mail", "lock error: %s: %r", s_to_c(l->name));
				return 0;
			}
		}
	} else
		free(d);

	return 1; /* try again later */
}

#define LSECS 5*60

/*
 *  Set a lock for a particular file.  The lock is a file in the same directory
 *  and has L. prepended to the name of the last element of the file name.
 */
extern Mlock *
syslock(char *path)
{
	Mlock *l;
	int tries;

	l = mallocz(sizeof(Mlock), 1);
	if(l == 0)
		return nil;

	l->name = lockname(path);

	/*
	 *  wait LSECS seconds for it to unlock
	 */
	for(tries = 0; tries < LSECS*2; tries++){
		switch(openlockfile(l)){
		case 0:
			return l;
		case 1:
			sleep(500);
			break;
		default:
			goto noway;
		}
	}

noway:
	s_free(l->name);
	free(l);
	return nil;
}

/*
 *  like lock except don't wait
 */
extern Mlock *
trylock(char *path)
{
	Mlock *l;
	char buf[1];
	int fd;

	l = malloc(sizeof(Mlock));
	if(l == 0)
		return 0;

	l->name = lockname(path);
	if(openlockfile(l) != 0){
		s_free(l->name);
		free(l);
		return 0;
	}

	/* fork process to keep lock alive */
	switch(l->pid = rfork(RFPROC)){
	default:
		break;
	case 0:
		fd = l->fd;
		for(;;){
			sleep(1000*60);
			if(pread(fd, buf, 1, 0) < 0)
				break;
		}
		_exits(0);
	}
	return l;
}

extern void
syslockrefresh(Mlock *l)
{
	char buf[1];

	pread(l->fd, buf, 1, 0);
}

extern void
sysunlock(Mlock *l)
{
	if(l == 0)
		return;
	if(l->name){
		s_free(l->name);
	}
	if(l->fd >= 0)
		close(l->fd);
	if(l->pid > 0)
		postnote(PNPROC, l->pid, "time to die");
	free(l);
}

/*
 *  Open a file.  The modes are:
 *
 *	l	- locked
 *	a	- set append permissions
 *	r	- readable
 *	w	- writable
 *	A	- append only (doesn't exist in Bio)
 */
extern Biobuf *
sysopen(char *path, char *mode, ulong perm)
{
	int sysperm;
	int sysmode;
	int fd;
	int docreate;
	int append;
	int truncate;
	Dir *d, nd;
	Biobuf *bp;

	/*
	 *  decode the request
	 */
	sysperm = 0;
	sysmode = -1;
	docreate = 0;
	append = 0;
	truncate = 0;
 	for(; mode && *mode; mode++)
		switch(*mode){
		case 'A':
			sysmode = OWRITE;
			append = 1;
			break;
		case 'c':
			docreate = 1;
			break;
		case 'l':
			sysperm |= DMEXCL;
			sysmode |= OLOCK;
			break;
		case 'a':
			sysperm |= DMAPPEND;
			break;
		case 'w':
			if(sysmode == -1)
				sysmode = OWRITE;
			else
				sysmode = ORDWR;
			break;
		case 'r':
			if(sysmode == -1)
				sysmode = OREAD;
			else
				sysmode = ORDWR;
			break;
		case 't':
			truncate = 1;
			break;
		default:
			break;
		}
	switch(sysmode){
	case OREAD:
	case OWRITE:
	case ORDWR:
		break;
	default:
		if(sysperm&DMAPPEND)
			sysmode = OWRITE;
		else
			sysmode = OREAD;
		break;
	}

	/*
	 *  create file if we need to
	 */
	if(truncate)
		sysmode |= OTRUNC;
	fd = open(path, sysmode);
	if(fd < 0){
		d = dirstat(path);
		if(d == nil){
			if(docreate == 0)
				return 0;

			fd = create(path, sysmode, sysperm|perm);
			if(fd < 0)
				return 0;
			nulldir(&nd);
			nd.mode = sysperm|perm;
			dirfwstat(fd, &nd);
		} else {
			free(d);
			return 0;
		}
	}

	bp = (Biobuf*)malloc(sizeof(Biobuf));
	if(bp == 0){
		close(fd);
		return 0;
	}
	memset(bp, 0, sizeof(Biobuf));
	Binit(bp, fd, sysmode&~OTRUNC);

	if(append)
		Bseek(bp, 0, 2);
	return bp;
}

/*
 *  close the file, etc.
 */
int
sysclose(Biobuf *bp)
{
	int rv;

	rv = Bterm(bp);
	close(Bfildes(bp));
	free(bp);
	return rv;
}

/*
 *  create a file
 */
int
syscreate(char *file, int mode, ulong perm)
{
	return create(file, mode, perm);
}

/*
 *  make a directory
 */
int
sysmkdir(char *file, ulong perm)
{
	int fd;

	if((fd = create(file, OREAD, DMDIR|perm)) < 0)
		return -1;
	close(fd);
	return 0;
}

/*
 *  change the group of a file
 */
int
syschgrp(char *file, char *group)
{
	Dir nd;

	if(group == 0)
		return -1;
	nulldir(&nd);
	nd.gid = group;
	return dirwstat(file, &nd);
}

extern int
sysdirreadall(int fd, Dir **d)
{
	return dirreadall(fd, d);
}

/*
 *  read in the system name
 */
static char *unix_hostname_read(void);
extern char *
sysname_read(void)
{
	static char name[128];
	char *cp;

	cp = getenv("site");
	if(cp == 0 || *cp == 0)
		cp = alt_sysname_read();
	if(cp == 0 || *cp == 0)
		cp = "kremvax";
	strecpy(name, name+sizeof name, cp);
	return name;
}
extern char *
alt_sysname_read(void)
{
	char *cp;
	static char name[128];

	cp = getenv("sysname");
	if(cp == 0 || *cp == 0)
		cp = unix_hostname_read();
	if(cp == 0 || *cp == 0)
		return 0;
	strecpy(name, name+sizeof name, cp);
	return name;
}
static char *
unix_hostname_read(void)
{
	static char hostname[256];

	if(gethostname(hostname, sizeof hostname) < 0)
		return nil;
	return hostname;
}

/*
 *  get all names
 */
extern char**
sysnames_read(void)
{
	static char **namev;
	struct hostent *h;
	char **p, **a;

	if(namev)
		return namev;

	h = gethostbyname(alt_sysname_read());
	if(h == nil)
		return 0;

	for(p=h->h_aliases; *p; p++)
		;

	namev = malloc((2+p-h->h_aliases)*sizeof namev[0]);
	if(namev == 0)
		return 0;

	a = namev;
	*a++ = strdup(h->h_name);
	for(p=h->h_aliases; *p; p++)
		*a++ = strdup(*p);
	*a = 0;

	return namev;
}

/*
 *  read in the domain name.
 *  chop off beginning pieces until we find one with an mx record.
 */
extern char *
domainname_read(void)
{
	char **namev, *p;
	Ndbtuple *t;

	for(namev = sysnames_read(); namev && *namev; namev++){
		if(strchr(*namev, '.')){
			for(p=*namev-1; p && *++p; p=strchr(p, '.')){
				if((t = dnsquery(nil, p, "mx")) != nil){
					ndbfree(t);
					return p;
				}
			}
		}
	}
	return 0;
}

/*
 *  return true if the last error message meant file
 *  did not exist.
 */
extern int
e_nonexistent(void)
{
	rerrstr(err, sizeof(err));
	return strcmp(err, "file does not exist") == 0;
}

/*
 *  return true if the last error message meant file
 *  was locked.
 */
extern int
e_locked(void)
{
	rerrstr(err, sizeof(err));
	return strcmp(err, "open/create -- file is locked") == 0;
}

/*
 *  return the length of a file
 */
extern long
sysfilelen(Biobuf *fp)
{
	Dir *d;
	long rv;

	d = dirfstat(Bfildes(fp));
	if(d == nil)
		return -1;
	rv = d->length;
	free(d);
	return rv;
}

/*
 *  remove a file
 */
extern int
sysremove(char *path)
{
	return remove(path);
}

/*
 *  rename a file, fails unless both are in the same directory
 */
extern int
sysrename(char *old, char *new)
{
	Dir d;
	char *obase;
	char *nbase;

	obase = strrchr(old, '/');
	nbase = strrchr(new, '/');
	if(obase){
		if(nbase == 0)
			return -1;
		if(strncmp(old, new, obase-old) != 0)
			return -1;
		nbase++;
	} else {
		if(nbase)
			return -1;
		nbase = new;
	}
	nulldir(&d);
	d.name = nbase;
	return dirwstat(old, &d);
}

/*
 *  see if a file exists
 */
extern int
sysexist(char *file)
{
	Dir	*d;

	d = dirstat(file);
	if(d == nil)
		return 0;
	free(d);
	return 1;
}

/*
 *  return nonzero if file is a directory
 */
extern int
sysisdir(char *file)
{
	Dir	*d;
	int	rv;

	d = dirstat(file);
	if(d == nil)
		return 0;
	rv = d->mode & DMDIR;
	free(d);
	return rv;
}

/*
 *  kill a process
 */
extern int
syskill(int pid)
{
	return postnote(PNPROC, pid, "kill");
}

/*
 *  kill a process group
 */
extern int
syskillpg(int pid)
{
	return postnote(PNGROUP, pid, "kill");
}

extern int
sysdetach(void)
{
	if(rfork(RFENVG|RFNAMEG|RFNOTEG) < 0) {
		werrstr("rfork failed");
		return -1;
	}
	return 0;
}

/*
 *  catch a write on a closed pipe
 */
static int *closedflag;
static int
catchpipe(void *a, char *msg)
{
	static char *foo = "sys: write on closed pipe";

	USED(a);
	if(strncmp(msg, foo, strlen(foo)) == 0){
		if(closedflag)
			*closedflag = 1;
		return 1;
	}
	return 0;
}
void
pipesig(int *flagp)
{
	closedflag = flagp;
	atnotify(catchpipe, 1);
}
void
pipesigoff(void)
{
	atnotify(catchpipe, 0);
}

extern int
holdon(void)
{
	/* XXX talk to 9term? */
	return -1;
}

extern int
sysopentty(void)
{
	return open("/dev/tty", ORDWR);
}

extern void
holdoff(int fd)
{
	write(fd, "holdoff", 7);
	close(fd);
}

extern int
sysfiles(void)
{
	return 128;
}

/*
 *  expand a path relative to the user's mailbox directory
 *
 *  if the path starts with / or ./, don't change it
 *
 */
extern String *
mboxpath(char *path, char *user, String *to, int dot)
{
	char *dir;
	String *s;

	if (dot || *path=='/' || strncmp(path, "./", 2) == 0
			      || strncmp(path, "../", 3) == 0) {
		to = s_append(to, path);
	} else {
		if ((dir = homedir(user)) != nil) {
			s = s_copy(dir);
			s_append(s, "/mail/");
			if(access(s_to_c(s), AEXIST) >= 0){
				to = s_append(to, s_to_c(s));
				s_free(s);
				to = s_append(to, path);
				return to;
			}
			s_free(s);
		}
		to = s_append(to, MAILROOT);
		to = s_append(to, "/box/");
		to = s_append(to, user);
		to = s_append(to, "/");
		to = s_append(to, path);
	}
	return to;
}

extern String *
mboxname(char *user, String *to)
{
	return mboxpath("mbox", user, to, 0);
}

extern String *
deadletter(String *to)		/* pass in sender??? */
{
	char *cp;

	cp = getlog();
	if(cp == 0)
		return 0;
	return mboxpath("dead.letter", cp, to, 0);
}

String *
readlock(String *file)
{
	char *cp;

	cp = getlog();
	if(cp == 0)
		return 0;
	return mboxpath("reading", cp, file, 0);
}

String *
username(String *from)
{
	String* s;
	struct passwd* pw;

	setpwent();
	while((pw = getpwent()) != nil){
		if(strcmp(s_to_c(from), pw->pw_name) == 0){
			s = s_new();
			s_append(s, "\"");
			s_append(s, pw->pw_gecos);
			s_append(s, "\"");
			return s;
		}
	}
	return nil;
}

char *
homedir(char *user)
{
	static char buf[1024];
	struct passwd* pw;

	setpwent();
	while((pw = getpwent()) != nil)
		if(strcmp(user, pw->pw_name) == 0){
			strecpy(buf, buf+sizeof buf, pw->pw_dir);
			return buf;
		}
	return nil;
}

char *
remoteaddr(int fd, char *dir)
{
	char *raddr;
	NetConnInfo *nci;

	if((nci = getnetconninfo(dir, fd)) == nil)
		return nil;
	raddr = strdup(nci->raddr);
	freenetconninfo(nci);
	return raddr;
}

/*  create a file and  */
/*	1) ensure the modes we asked for */
/*	2) make gid == uid */
static int
docreate(char *file, int perm)
{
	int fd;
	Dir ndir;
	Dir *d;

	/*  create the mbox */
	fd = create(file, OREAD, perm);
	if(fd < 0){
		fprint(2, "couldn't create %s\n", file);
		return -1;
	}
	d = dirfstat(fd);
	if(d == nil){
		fprint(2, "couldn't stat %s\n", file);
		return -1;
	}
	nulldir(&ndir);
	ndir.mode = perm;
	ndir.gid = d->uid;
	if(dirfwstat(fd, &ndir) < 0)
		fprint(2, "couldn't chmod %s: %r\n", file);
	close(fd);
	return 0;
}

/*  create a mailbox */
int
creatembox(char *user, char *folder)
{
	char *p;
	String *mailfile;
	char buf[512];
	Mlock *ml;

	mailfile = s_new();
	if(folder == 0)
		mboxname(user, mailfile);
	else {
		snprint(buf, sizeof(buf), "%s/mbox", folder);
		mboxpath(buf, user, mailfile, 0);
	}

	/* don't destroy existing mailbox */
	if(access(s_to_c(mailfile), 0) == 0){
		fprint(2, "mailbox already exists\n");
		return -1;
	}
	fprint(2, "creating new mbox: %s\n", s_to_c(mailfile));

	/*  make sure preceding levels exist */
	for(p = s_to_c(mailfile); p; p++) {
		if(*p == '/')	/* skip leading or consecutive slashes */
			continue;
		p = strchr(p, '/');
		if(p == 0)
			break;
		*p = 0;
		if(access(s_to_c(mailfile), 0) != 0){
			if(docreate(s_to_c(mailfile), DMDIR|0711) < 0)
				return -1;
		}
		*p = '/';
	}

	/*  create the mbox */
	if(docreate(s_to_c(mailfile), 0622|DMAPPEND|DMEXCL) < 0)
		return -1;

	/*
	 *  create the lock file if it doesn't exist
	 */
	ml = trylock(s_to_c(mailfile));
	if(ml != nil)
		sysunlock(ml);

	return 0;
}
