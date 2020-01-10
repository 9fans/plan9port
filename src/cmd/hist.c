#include	<u.h>
#include	<libc.h>
#include	<ctype.h>

#define	MINUTE(x)	((long)(x)*60L)
#define	HOUR(x)		(MINUTE(x)*60L)
#define	YEAR(x)		(HOUR(x)*24L*360L)

int	verb;
int	uflag;
int	force;
int	diff;
int	diffb;
char*	sflag;
char *sys;

void	ysearch(char*);
long	starttime(char*);
void	lastbefore(ulong, char*, char*, char*);
char*	prtime(ulong);

int
main(int argc, char *argv[])
{
	int i;

	sys = sysname();
	if(strncmp(sys, "amsterdam", 9) == 0)	/* BUG */
		sys = "am";

	ARGBEGIN {
	default:
		goto usage;
	case 'v':
		verb = 1;
		break;
	case 'f':
		force = 1;
		break;
	case 'd':
		diff = 1;
		break;
	case 'b':
		diffb = 1;
		break;
	case 's':
		sflag = ARGF();
		break;
	case 'u':
		uflag = 1;
		break;
	} ARGEND

	if(argc == 0) {
	usage:
		fprint(2, "usage: hist [-bdfuv] [-s yyyymmdd] files\n");
		exits(0);
	}

	for(i=0; i<argc; i++)
		ysearch(argv[i]);
	exits(0);
	return 0;
}

int
strprefix(char *a, char *aa)
{
	return memcmp(a, aa, strlen(a)) == 0;
}

void
ysearch(char *file)
{
	char *ndump;
	char fil[400], buf[500], nbuf[100], pair[2][500];
	Tm *tm;
	Dir *dir, *d;
	ulong otime, dt;
	int toggle, started, missing;

	started = 0;
	dir = dirstat(file);
	if(dir == nil)
		fprint(2, "history: warning: %s does not exist\n", file);
	else{
		print("%s %s %lld\n", prtime(dir->mtime), file, dir->length);
		started = 1;
		strcpy(pair[1], file);
	}
	free(dir);
	fil[0] = 0;
	if(file[0] != '/') {
		getwd(strchr(fil, 0), 100);
		strcat(fil, "/");
	}
	strcat(fil, file);
	cleanname(fil);

	sprint(nbuf, "/dump/%s", sys);
	ndump = nbuf;

	tm = localtime(time(0));
	sprint(buf, "%s/%.4d/", ndump, tm->year+1900);
	if(access(buf, AREAD) < 0){
		print("cannot access %s\n", buf);
		return;
	}

	otime = starttime(sflag);
	toggle = 0;
	for(;;) {
		lastbefore(otime, fil, buf, ndump);
		dir = dirstat(buf);
		if(dir == nil) {
			if(!force)
				return;
			dir = malloc(sizeof(Dir));
			nulldir(dir);
			dir->mtime = otime + 1;
		}
		dt = HOUR(12);
		missing = 0;
		while(otime <= dir->mtime){
			if(verb)
				print("backup %ld, %ld\n", dir->mtime, otime-dt);
			lastbefore(otime-dt, fil, buf, ndump);
			d = dirstat(buf);
			if(d == nil){
				if(!force)
					return;
				if(!missing)
					print("removed %s\n", buf);
				missing = 1;
			}else{
				free(dir);
				dir = d;
			}
			dt += HOUR(12);
		}
		strcpy(pair[toggle], buf);
		if(diff && started){
			if(verb)
				print("diff %s %s\n", pair[toggle^1], pair[toggle]);
			switch(rfork(RFFDG|RFPROC)){
			case 0:
				execlp("diff", "diff", diffb ? "-cb" : "-c", pair[toggle], pair[toggle ^ 1], (char*)0);
				fprint(2, "can't exec diff: %r\n");
				exits(0);
			case -1:
				fprint(2, "can't fork diff: %r\n");
				break;
			default:
				while(waitpid() != -1)
					;
				break;
			}
		}
		print("%s %s %lld\n", prtime(dir->mtime), buf, dir->length);
		toggle ^= 1;
		started = 1;
		otime = dir->mtime;
		free(dir);
	}
}

void
lastbefore(ulong t, char *f, char *b, char *ndump)
{
	Tm *tm;
	Dir *dir;
	int vers, try;
	ulong t0, mtime;

	t0 = t;
	if(verb)
		print("%ld lastbefore %s\n", t0, f);
	mtime = 0;
	for(try=0; try<10; try++) {
		tm = localtime(t);
		sprint(b, "%s/%.4d/%.2d%.2d", ndump,
			tm->year+1900, tm->mon+1, tm->mday);
		dir = dirstat(b);
		if(dir){
			mtime = dir->mtime;
			free(dir);
		}
		if(dir==nil || mtime > t0) {
			if(verb)
				print("%ld earlier %s\n", mtime, b);
			t -= HOUR(24);
			continue;
		}
		for(vers=0;; vers++) {
			sprint(b, "%s/%.4d/%.2d%.2d%d", ndump,
				tm->year+1900, tm->mon+1, tm->mday, vers+1);
			dir = dirstat(b);
			if(dir){
				mtime = dir->mtime;
				free(dir);
			}
			if(dir==nil || mtime > t0)
				break;
			if(verb)
				print("%ld later %s\n", mtime, b);
		}
		sprint(b, "%s/%.4d/%.2d%.2d%s", ndump,
			tm->year+1900, tm->mon+1, tm->mday, f);
		if(vers)
			sprint(b, "%s/%.4d/%.2d%.2d%d%s", ndump,
				tm->year+1900, tm->mon+1, tm->mday, vers, f);
		return;
	}
	strcpy(b, "XXX");	/* error */
}

char*
prtime(ulong t)
{
	static char buf[100];
	char *b;
	Tm *tm;

	if(uflag)
		tm = gmtime(t);
	else
		tm = localtime(t);
	b = asctime(tm);
	memcpy(buf, b+4, 24);
	buf[24] = 0;
	return buf;
}

long
starttime(char *s)
{
	Tm *tm;
	long t, dt;
	int i, yr, mo, da;

	t = time(0);
	if(s == 0)
		return t;
	for(i=0; s[i]; i++)
		if(s[i] < '0' || s[i] > '9') {
			fprint(2, "bad start time: %s\n", s);
			return t;
		}
	if(strlen(s)==6){
		yr = (s[0]-'0')*10 + s[1]-'0';
		mo = (s[2]-'0')*10 + s[3]-'0' - 1;
		da = (s[4]-'0')*10 + s[5]-'0';
		if(yr < 70)
			yr += 100;
	}else if(strlen(s)==8){
		yr = (((s[0]-'0')*10 + s[1]-'0')*10 + s[2]-'0')*10 + s[3]-'0';
		yr -= 1900;
		mo = (s[4]-'0')*10 + s[5]-'0' - 1;
		da = (s[6]-'0')*10 + s[7]-'0';
	}else{
		fprint(2, "bad start time: %s\n", s);
		return t;
	}
	t = 0;
	dt = YEAR(10);
	for(i=0; i<50; i++) {
		tm = localtime(t+dt);
		if(yr > tm->year ||
		  (yr == tm->year && mo > tm->mon) ||
		  (yr == tm->year && mo == tm->mon) && da > tm->mday) {
			t += dt;
			continue;
		}
		dt /= 2;
		if(dt == 0)
			break;
	}
	t += HOUR(12);	/* .5 day to get to noon of argument */
	return t;
}
