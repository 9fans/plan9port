#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <mp.h>
#include <libsec.h>
#include "SConn.h"
#include "secstore.h"

char *SECSTORE_DIR;
char* secureidcheck(char *, char *);   /* from /sys/src/cmd/auth/ */
extern char* dirls(char *path);

int verbose;
Ndb *db;

static void
usage(void)
{
	fprint(2, "usage: secstored [-R] [-S servername] [-s tcp!*!5356] [-v] [-x netmtpt]\n");
	exits("usage");
}

static int
getdir(SConn *conn, char *id)
{
	char *ls, *s;
	uchar *msg;
	int n, len;

	s = emalloc(Maxmsg);
	snprint(s, Maxmsg, "%s/store/%s", SECSTORE_DIR, id);

	if((ls = dirls(s)) == nil)
		len = 0;
	else
		len = strlen(ls);

	/* send file size */
	snprint(s, Maxmsg, "%d", len);
	conn->write(conn, (uchar*)s, strlen(s));

	/* send directory listing in Maxmsg chunks */
	n = Maxmsg;
	msg = (uchar*)ls;
	while(len > 0){
		if(len < Maxmsg)
			n = len;
		conn->write(conn, msg, n);
		msg += n;
		len -= n;
	}
	free(s);
	free(ls);
	return 0;
}

char *
validatefile(char *f)
{
	char *nl;

	if(f==nil || *f==0)
		return nil;
	if(nl = strchr(f, '\n'))
		*nl = 0;
	if(strchr(f,'/') != nil || strcmp(f,"..")==0 || strlen(f) >= 300){
		syslog(0, LOG, "no slashes allowed: %s\n", f);
		return nil;
	}
	return f;
}

static int
getfile(SConn *conn, char *id, char *gf)
{
	int n, gd, len;
	ulong mode;
	char *s;
	Dir *st;

	if(strcmp(gf,".")==0)
		return getdir(conn, id);

	/* send file size */
	s = emalloc(Maxmsg);
	snprint(s, Maxmsg, "%s/store/%s/%s", SECSTORE_DIR, id, gf);
	gd = open(s, OREAD);
	if(gd < 0){
		syslog(0, LOG, "can't open %s: %r\n", s);
		free(s);
		conn->write(conn, (uchar*)"-1", 2);
		return -1;
	}
	st = dirfstat(gd);
	if(st == nil){
		syslog(0, LOG, "can't stat %s: %r\n", s);
		free(s);
		conn->write(conn, (uchar*)"-1", 2);
		return -1;
	}
	mode = st->mode;
	len = st->length;
	free(st);
	if(mode & DMDIR) {
		syslog(0, LOG, "%s should be a plain file, not a directory\n", s);
		free(s);
		conn->write(conn, (uchar*)"-1", 2);
		return -1;
	}
	if(len < 0 || len > MAXFILESIZE){
		syslog(0, LOG, "implausible filesize %d for %s\n", len, gf);
		free(s);
		conn->write(conn, (uchar*)"-3", 2);
		return -1;
	}
	snprint(s, Maxmsg, "%d", len);
	conn->write(conn, (uchar*)s, strlen(s));

	/* send file in Maxmsg chunks */
	while(len > 0){
		n = read(gd, s, Maxmsg);
		if(n <= 0){
			syslog(0, LOG, "read error on %s: %r\n", gf);
			free(s);
			return -1;
		}
		conn->write(conn, (uchar*)s, n);
		len -= n;
	}
	close(gd);
	free(s);
	return 0;
}

static int
putfile(SConn *conn, char *id, char *pf)
{
	int n, nw, pd;
	long len;
	char s[Maxmsg+1];

	/* get file size */
	n = readstr(conn, s);
	if(n < 0){
		syslog(0, LOG, "remote: %s: %r\n", s);
		return -1;
	}
	len = atoi(s);
	if(len == -1){
		syslog(0, LOG, "remote file %s does not exist\n", pf);
		return -1;
	}else if(len < 0 || len > MAXFILESIZE){
		syslog(0, LOG, "implausible filesize %ld for %s\n", len, pf);
		return -1;
	}

	/* get file in Maxmsg chunks */
	if(strchr(pf,'/') != nil || strcmp(pf,"..")==0){
		syslog(0, LOG, "no slashes allowed: %s\n", pf);
		return -1;
	}
	snprint(s, Maxmsg, "%s/store/%s/%s", SECSTORE_DIR, id, pf);
	pd = create(s, OWRITE, 0660);
	if(pd < 0){
		syslog(0, LOG, "can't open %s: %r\n", s);
		return -1;
	}
	while(len > 0){
		n = conn->read(conn, (uchar*)s, Maxmsg);
		if(n <= 0){
			syslog(0, LOG, "empty file chunk\n");
			return -1;
		}
		nw = write(pd, s, n);
		if(nw != n){
			syslog(0, LOG, "write error on %s: %r", pf);
			return -1;
		}
		len -= n;
	}
	close(pd);
	return 0;

}

static int
removefile(SConn *conn, char *id, char *f)
{
	Dir *d;
	char buf[Maxmsg];

	snprint(buf, Maxmsg, "%s/store/%s/%s", SECSTORE_DIR, id, f);

	if((d = dirstat(buf)) == nil){
		snprint(buf, sizeof buf, "remove failed: %r");
		writerr(conn, buf);
		return -1;
	}else if(d->mode & DMDIR){
		snprint(buf, sizeof buf, "can't remove a directory");
		writerr(conn, buf);
		free(d);
		return -1;
	}

	free(d);
	if(remove(buf) < 0){
		snprint(buf, sizeof buf, "remove failed: %r");
		writerr(conn, buf);
		return -1;
	}
	return 0;
}

/* given line directory from accept, returns ipaddr!port */
static char*
remoteIP(char *ldir)
{
	int fd, n;
	char rp[100], ap[500];

	snprint(rp, sizeof rp, "%s/remote", ldir);
	fd = open(rp, OREAD);
	if(fd < 0)
		return strdup("?!?");
	n = read(fd, ap, sizeof ap);
	if(n <= 0 || n == sizeof ap){
		fprint(2, "error %d reading %s: %r\n", n, rp);
		return strdup("?!?");
	}
	close(fd);
	ap[n--] = 0;
	if(ap[n] == '\n')
		ap[n] = 0;
	return strdup(ap);
}

static int
dologin(int fd, char *S, int forceSTA)
{
	int i, n, rv;
	char *file, *mess;
	char msg[Maxmsg+1];
	PW *pw;
	SConn *conn;

	pw = nil;
	rv = -1;

	/* collect the first message */
	if((conn = newSConn(fd)) == nil)
		return -1;
	if(readstr(conn, msg) < 0){
		fprint(2, "remote: %s: %r\n", msg);
		writerr(conn, "can't read your first message");
		goto Out;
	}

	/* authenticate */
	if(PAKserver(conn, S, msg, &pw) < 0){
		if(pw != nil)
			syslog(0, LOG, "secstore denied for %s", pw->id);
		goto Out;
	}
	if((forceSTA || pw->status&STA) != 0){
		conn->write(conn, (uchar*)"STA", 3);
		if(readstr(conn, msg) < 10 || strncmp(msg, "STA", 3) != 0){
			syslog(0, LOG, "no STA from %s", pw->id);
			goto Out;
		}
		mess = secureidcheck(pw->id, msg+3);
		if(mess != nil){
			syslog(0, LOG, "secureidcheck denied %s because %s", pw->id, mess);
			goto Out;
		}
	}
	conn->write(conn, (uchar*)"OK", 2);
	syslog(0, LOG, "AUTH %s", pw->id);

	/* perform operations as asked */
	while((n = readstr(conn, msg)) > 0){
		syslog(0, LOG, "[%s] %s", pw->id, msg);

		if(strncmp(msg, "GET ", 4) == 0){
			file = validatefile(msg+4);
			if(file==nil || getfile(conn, pw->id, file) < 0)
				goto Err;

		}else if(strncmp(msg, "PUT ", 4) == 0){
			file = validatefile(msg+4);
			if(file==nil || putfile(conn, pw->id, file) < 0){
				syslog(0, LOG, "failed PUT %s/%s", pw->id, file);
				goto Err;
			}

		}else if(strncmp(msg, "RM ", 3) == 0){
			file = validatefile(msg+3);
			if(file==nil || removefile(conn, pw->id, file) < 0){
				syslog(0, LOG, "failed RM %s/%s", pw->id, file);
				goto Err;
			}

		}else if(strncmp(msg, "CHPASS", 6) == 0){
			if(readstr(conn, msg) < 0){
				syslog(0, LOG, "protocol botch CHPASS for %s", pw->id);
				writerr(conn, "protocol botch while setting PAK");
				goto Out;
			}
			pw->Hi = strtomp(msg, nil, 64, pw->Hi);
			for(i=0; i < 4 && putPW(pw) < 0; i++)
				syslog(0, LOG, "password change failed for %s (%d): %r", pw->id, i);
			if(i==4)
				goto Out;

		}else if(strncmp(msg, "BYE", 3) == 0){
			rv = 0;
			break;

		}else{
			writerr(conn, "unrecognized operation");
			break;
		}

	}
	if(n <= 0)
		syslog(0, LOG, "%s closed connection without saying goodbye\n", pw->id);

Out:
	freePW(pw);
	conn->free(conn);
	return rv;
Err:
	writerr(conn, "operation failed");
	goto Out;
}

void
main(int argc, char **argv)
{
	int afd, dfd, lcfd, forceSTA = 0;
	char adir[40], ldir[40], *remote;
	char *serve = "tcp!*!5356", *p, aserve[128];
	char *S = "secstore";
	char *dbpath;
	Ndb *db2;

	S = sysname();
	SECSTORE_DIR = unsharp("#9/secstore");
/*	setnetmtpt(net, sizeof(net), nil); */
	ARGBEGIN{
	case 'R':
		forceSTA = 1;
		break;
	case 's':
		serve = EARGF(usage());
		break;
	case 'S':
		S = EARGF(usage());
		break;
	case 'x':
		p = ARGF();
		if(p == nil)
			usage();
		USED(p);
	/*	setnetmtpt(net, sizeof(net), p); */
		break;
	case 'v':
		verbose++;
		break;
	default:
		usage();
	}ARGEND;

	if(!verbose)
		switch(rfork(RFNOTEG|RFPROC|RFFDG)) {
		case -1:
			sysfatal("fork: %r");
		case 0:
			break;
		default:
			exits(0);
		}

	snprint(aserve, sizeof aserve, "%s", serve);
	afd = announce(aserve, adir);
	if(afd < 0)
		sysfatal("%s: %r\n", aserve);
	syslog(0, LOG, "ANNOUNCE %s", aserve);
	for(;;){
		if((lcfd = listen(adir, ldir)) < 0)
			exits("can't listen");
		switch(rfork(RFPROC|RFFDG|RFNOWAIT)){
		case -1:
			fprint(2, "secstore forking: %r\n");
			close(lcfd);
			break;
		case 0:
			/* "/lib/ndb/common.radius does not exist" if db set before fork */
			db = ndbopen(dbpath=unsharp("#9/ndb/auth"));
			if(db == 0)
				syslog(0, LOG, "no ndb/auth");
			db2 = ndbopen(0);
			if(db2 == 0)
				syslog(0, LOG, "no ndb/local");
			db = ndbcat(db, db2);
			if((dfd = accept(lcfd, ldir)) < 0)
				exits("can't accept");
			alarm(30*60*1000); 	/* 30 min */
			remote = remoteIP(ldir);
			syslog(0, LOG, "secstore from %s", remote);
			free(remote);
			dologin(dfd, S, forceSTA);
			exits(nil);
		default:
			close(lcfd);
			break;
		}
	}
}
