#include <u.h>
#include <libc.h>

static struct
{
	int	fd;
	int	consfd;
	char	*name;
	Dir	*d;
	Dir	*consd;
	Lock	lk;
} sl =
{
	-1, -1,
};

static void
_syslogopen(void)
{
	char buf[1024], *p;

	if(sl.fd >= 0)
		close(sl.fd);
	snprint(buf, sizeof(buf), "#9/log/%s", sl.name);
	p = unsharp(buf);
	sl.fd = open(p, OWRITE|OCEXEC|OAPPEND);
	free(p);
}

/*
 * Print
 *  sysname: time: mesg
 * on /sys/log/logname.
 * If cons or log file can't be opened, print on the system console, too.
 */
void
syslog(int cons, char *logname, char *fmt, ...)
{
	char buf[1024];
	char *ctim, *p;
	va_list arg;
	int n;
	Dir *d;
	char err[ERRMAX];

	err[0] = '\0';
	errstr(err, sizeof err);
	lock(&sl.lk);

	/*
	 *  paranoia makes us stat to make sure a fork+close
	 *  hasn't broken our fd's
	 */
	d = dirfstat(sl.fd);
	if(sl.fd < 0
	   || sl.name == nil
	   || strcmp(sl.name, logname)!=0
	   || sl.d == nil
	   || d == nil
	   || d->dev != sl.d->dev
	   || d->type != sl.d->type
	   || d->qid.path != sl.d->qid.path){
		free(sl.name);
		sl.name = strdup(logname);
		if(sl.name == nil)
			cons = 1;
		else{
			_syslogopen();
			if(sl.fd < 0)
				cons = 1;
			free(sl.d);
			sl.d = d;
			d = nil;	/* don't free it */
		}
	}
	free(d);
	if(cons){
		d = dirfstat(sl.consfd);
		if(sl.consfd < 0
		   || d == nil
		   || sl.consd == nil
		   || d->dev != sl.consd->dev
		   || d->type != sl.consd->type
		   || d->qid.path != sl.consd->qid.path){
			sl.consfd = open("/dev/tty", OWRITE|OCEXEC);
			free(sl.consd);
			sl.consd = d;
			d = nil;	/* don't free it */
		}
		free(d);
	}

	if(fmt == nil){
		unlock(&sl.lk);
		return;
	}

	ctim = ctime(time(0));
	werrstr(err);
	p = buf + snprint(buf, sizeof(buf)-1, "%s ", sysname());
	strncpy(p, ctim+4, 15);
	p += 15;
	*p++ = ' ';
	va_start(arg, fmt);
	p = vseprint(p, buf+sizeof(buf)-1, fmt, arg);
	va_end(arg);
	*p++ = '\n';
	n = p - buf;

	if(sl.fd >= 0){
		seek(sl.fd, 0, 2);
		write(sl.fd, buf, n);
	}

	if(cons && sl.consfd >=0)
		write(sl.consfd, buf, n);

	unlock(&sl.lk);
}
