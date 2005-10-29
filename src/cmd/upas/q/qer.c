#include "common.h"

typedef struct Qfile Qfile;
struct Qfile
{
	Qfile	*next;
	char	*name;
	char	*tname;
} *files;

char *user;
int isnone;

int	copy(Qfile*);

void
usage(void)
{
	fprint(2, "usage: qer [-f file] [-q dir] q-root description reply-to arg-list\n");
	exits("usage");
}

void
error(char *f, char *a)
{
	char err[Errlen+1];
	char buf[256];

	rerrstr(err, sizeof(err));
	snprint(buf, sizeof(buf),  f, a);
	fprint(2, "qer: %s: %s\n", buf, err);
	exits(buf);
}

void
main(int argc, char**argv)
{
	Dir	*dir;
	String	*f, *c;
	int	fd;
	char	file[1024];
	char	buf[1024];
	long	n;
	char	*cp, *qdir;
	int	i;
	Qfile	*q, **l;

	l = &files;
	qdir = 0;

	ARGBEGIN {
	case 'f':
		q = malloc(sizeof(Qfile));
		q->name = ARGF();
		q->next = *l;
		*l = q;
		break;
	case 'q':
		qdir = ARGF();
		if(qdir == 0)
			usage();
		break;
	default:
		usage();
	} ARGEND;

	if(argc < 3)
		usage();
	user = getuser();
	isnone = (qdir != 0) || (strcmp(user, "none") == 0);

	if(qdir == 0) {
		qdir = user;
		if(qdir == 0)
			error("unknown user", 0);
	}
	snprint(file, sizeof(file), "%s/%s", argv[0], qdir);

	/*
	 *  data file name
	 */
	f = s_copy(file);
	s_append(f, "/D.XXXXXX");
	mktemp(s_to_c(f));
	cp = utfrrune(s_to_c(f), '/');
	cp++;

	/*
	 *  create directory and data file.  once the data file
	 *  exists, runq won't remove the directory
	 */
	fd = -1;
	for(i = 0; i < 10; i++){
		int perm;

		dir = dirstat(file);
		if(dir == nil){
			perm = isnone?0777:0775;
			if(sysmkdir(file, perm) < 0)
				continue;
		} else {
			if((dir->qid.type&QTDIR)==0)
				error("not a directory %s", file);
		}
		perm = isnone?0664:0660;
		fd = create(s_to_c(f), OWRITE, perm);
		if(fd >= 0)
			break;
		sleep(250);
	}
	if(fd < 0)
		error("creating data file %s", s_to_c(f));

	/*
	 *  copy over associated files
	 */
	if(files){
		*cp = 'F';
		for(q = files; q; q = q->next){
			q->tname = strdup(s_to_c(f));
			if(copy(q) < 0)
				error("copying %s to queue", q->name);
			(*cp)++;
		}
	}

	/*
	 *  copy in the data file
	 */
	i = 0;
	while((n = read(0, buf, sizeof(buf)-1)) > 0){
		if(i++ == 0 && strncmp(buf, "From", 4) != 0){
			buf[n] = 0;
			syslog(0, "smtp", "qer usys data starts with %-40.40s\n", buf);
		}
		if(write(fd, buf, n) != n)
			error("writing data file %s", s_to_c(f));
	}
/*	if(n < 0)
		error("reading input"); */
	close(fd);

	/*
	 *  create control file
	 */
	*cp = 'C';
	fd = syscreatelocked(s_to_c(f), OWRITE, 0664);
	if(fd < 0)
		error("creating control file %s", s_to_c(f));
	c = s_new();
	for(i = 1; i < argc; i++){
		s_append(c, argv[i]);
		s_append(c, " ");
	}
	for(q = files; q; q = q->next){
		s_append(c, q->tname);
		s_append(c, " ");
	}
	s_append(c, "\n");
	if(write(fd, s_to_c(c), strlen(s_to_c(c))) < 0) {
		sysunlockfile(fd);
		error("writing control file %s", s_to_c(f));
	}
	sysunlockfile(fd);
	exits(0);
}

int
copy(Qfile *q)
{
	int from, to, n;
	char buf[4096];

	from = open(q->name, OREAD);
	if(from < 0)
		return -1;
	to = create(q->tname, OWRITE, 0660);
	if(to < 0){
		close(from);
		return -1;
	}
	for(;;){
		n = read(from, buf, sizeof(buf));
		if(n <= 0)
			break;
		n = write(to, buf, n);
		if(n < 0)
			break;
	}
	close(to);
	close(from);
	return n;
}
