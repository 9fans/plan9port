/*
 *	news foo	prints /lib/news/foo
 *	news -a		prints all news items, latest first
 *	news -n		lists names of new items
 *	news		prints items changed since last news
 */

#include <u.h>
#include <libc.h>
#include <bio.h>

#define	NINC	50	/* Multiples of directory allocation */
char	*NEWS = "#9/news";
char	TFILE[] = "%s/lib/newstime";

/*
 *	The following items should not be printed.
 */
char*	ignore[] =
{
	"core",
	"dead.letter",
	0
};

typedef
struct
{
	long	time;
	char	*name;
	vlong	length;
} File;
File*	n_list;
int	n_count;
int	n_items;
Biobuf	bout;

int	fcmp(const void *a, const void *b);
void	read_dir(int update);
void	print_item(char *f);
void	eachitem(void (*emit)(char*), int all, int update);
void	note(char *s);

void
main(int argc, char *argv[])
{
	int i;

	NEWS = unsharp(NEWS);

	Binit(&bout, 1, OWRITE);
	if(argc == 1) {
		eachitem(print_item, 0, 1);
		exits(0);
	}
	ARGBEGIN{
	case 'a':	/* print all */
		eachitem(print_item, 1, 0);
		break;

	case 'n':	/* names only */
		eachitem(note, 0, 0);
		if(n_items)
			Bputc(&bout, '\n');
		break;

	default:
		fprint(2, "news: bad option %c\n", ARGC());
		exits("usage");
	}ARGEND
	for(i=0; i<argc; i++)
		print_item(argv[i]);
	exits(0);
}

int
fcmp(const void *a, const void *b)
{
	long x;

	x = ((File*)b)->time - ((File*)a)->time;
	if(x < 0)
		return -1;
	if(x > 0)
		return 1;
	return 0;
}

/*
 *	read_dir: get the file names and modification dates for the
 *	files in /usr/news into n_list; sort them in reverse by
 *	modification date.
 */
void
read_dir(int update)
{
	Dir *d;
	char newstime[100], *home;
	int i, j, n, na, fd;

	n_count = 0;
	n_list = malloc(NINC*sizeof(File));
	na = NINC;
	home = getenv("HOME");
	if(home) {
		sprint(newstime, TFILE, home);
		d = dirstat(newstime);
		if(d != nil) {
			n_list[n_count].name = strdup("");
			n_list[n_count].time =d->mtime-1;
			n_list[n_count].length = 0;
			n_count++;
			free(d);
		}
		if(update) {
			fd = create(newstime, OWRITE, 0644);
			if(fd >= 0)
				close(fd);
		}
	}
	fd = open(NEWS, OREAD);
	if(fd < 0) {
		fprint(2, "news: ");
		perror(NEWS);
		exits(NEWS);
	}

	n = dirreadall(fd, &d);
	for(i=0; i<n; i++) {
		for(j=0; ignore[j]; j++)
			if(strcmp(ignore[j], d[i].name) == 0)
				goto ign;
		if(na <= n_count) {
			na += NINC;
			n_list = realloc(n_list, na*sizeof(File));
		}
		n_list[n_count].name = strdup(d[i].name);
		n_list[n_count].time = d[i].mtime;
		n_list[n_count].length = d[i].length;
		n_count++;
	ign:;
	}
	free(d);

	close(fd);
	qsort(n_list, n_count, sizeof(File), fcmp);
}

void
print_item(char *file)
{
	char name[4096], *p, *ep;
	Dir *dbuf;
	int f, c;
	int bol, bop;

	sprint(name, "%s/%s", NEWS, file);
	f = open(name, OREAD);
	if(f < 0) {
		fprint(2, "news: ");
		perror(name);
		return;
	}
	strcpy(name, "...");
	dbuf = dirfstat(f);
	if(dbuf == nil)
		return;
	Bprint(&bout, "\n%s (%s) %s\n", file,
		dbuf->muid[0]? dbuf->muid : dbuf->uid,
		asctime(localtime(dbuf->mtime)));
	free(dbuf);

	bol = 1;	/* beginning of line ...\n */
	bop = 1;	/* beginning of page ...\n\n */
	for(;;) {
		c = read(f, name, sizeof(name));
		if(c <= 0)
			break;
		p = name;
		ep = p+c;
		while(p < ep) {
			c = *p++;
			if(c == '\n') {
				if(!bop) {
					Bputc(&bout, c);
					if(bol)
						bop = 1;
					bol = 1;
				}
				continue;
			}
			if(bol) {
				Bputc(&bout, '\t');
				bol = 0;
				bop = 0;
			}
			Bputc(&bout, c);
		}
	}
	if(!bol)
		Bputc(&bout, '\n');
	close(f);
}

void
eachitem(void (*emit)(char*), int all, int update)
{
	int i;

	read_dir(update);
	for(i=0; i<n_count; i++) {
		if(n_list[i].name[0] == 0) {	/* newstime */
			if(all)
				continue;
			break;
		}
		if(n_list[i].length == 0)		/* in progress */
			continue;
		(*emit)(n_list[i].name);
	}
}

void
note(char *file)
{

	if(!n_items)
		Bprint(&bout, "news:");
	Bprint(&bout, " %s", file);
	n_items++;
}
