#include <u.h>
#include <sys/stat.h>
#include <libc.h>

#define rmdir p9rmdir

char	errbuf[ERRMAX];
int	ignerr = 0;

static void
err(char *f)
{
	if(!ignerr){
		errbuf[0] = '\0';
		errstr(errbuf, sizeof errbuf);
		fprint(2, "rm: %s: %s\n", f, errbuf);
	}
}

int
issymlink(char *name)
{
	struct stat s;
	return lstat(name, &s) >= 0 && S_ISLNK(s.st_mode);
}

/*
 * f is a non-empty directory. Remove its contents and then it.
 */
void
rmdir(char *f)
{
	char *name;
	int fd, i, j, n, ndir, nname;
	Dir *dirbuf;

	fd = open(f, OREAD);
	if(fd < 0){
		err(f);
		return;
	}
	n = dirreadall(fd, &dirbuf);
	close(fd);
	if(n < 0){
		err("dirreadall");
		return;
	}

	nname = strlen(f)+1+STATMAX+1;	/* plenty! */
	name = malloc(nname);
	if(name == 0){
		err("memory allocation");
		return;
	}

	ndir = 0;
	for(i=0; i<n; i++){
		snprint(name, nname, "%s/%s", f, dirbuf[i].name);
		if(remove(name) != -1 || issymlink(name))
			dirbuf[i].qid.type = QTFILE;	/* so we won't recurse */
		else{
			if(dirbuf[i].qid.type & QTDIR)
				ndir++;
			else
				err(name);
		}
	}
	if(ndir)
		for(j=0; j<n; j++)
			if(dirbuf[j].qid.type & QTDIR){
				snprint(name, nname, "%s/%s", f, dirbuf[j].name);
				rmdir(name);
			}
	if(remove(f) == -1)
		err(f);
	free(name);
	free(dirbuf);
}
void
main(int argc, char *argv[])
{
	int i;
	int recurse;
	char *f;
	Dir *db;

	ignerr = 0;
	recurse = 0;
	ARGBEGIN{
	case 'r':
		recurse = 1;
		break;
	case 'f':
		ignerr = 1;
		break;
	default:
		fprint(2, "usage: rm [-fr] file ...\n");
		exits("usage");
	}ARGEND
	for(i=0; i<argc; i++){
		f = argv[i];
		if(remove(f) != -1)
			continue;
		db = nil;
		if(recurse && (db=dirstat(f))!=nil && (db->qid.type&QTDIR))
			rmdir(f);
		else
			err(f);
		free(db);
	}
	exits(errbuf);
}
