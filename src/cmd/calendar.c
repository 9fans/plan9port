#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include <ctype.h>

typedef struct Date	Date;
struct Date {
	Reprog *p;	/* an RE to match this date */
	Date *next;	/* pointer to next in list */
};

enum{
	Secondsperday = 24*60*60
};

Biobuf in;
int debug, matchyear;

Date *dates(Date**, Tm*);
void upper2lower(char*, char*, int);
void *alloc(unsigned int);

void
main(int argc, char *argv[])
{
	int i, fd, ahead;
	long now;
	char *line;
	Tm *tm;
	Date *first, *last, *d;
	char buf[1024];

	ahead = 0;
	ARGBEGIN{
	case 'y':
		matchyear = 1;
		break;
	case 'd':
		debug = 1;
		break;
	case 'p':
		ahead = atoi(ARGF());
		break;
	default:
		fprint(2, "usage: calendar [-y] [-d] [files ...]\n");
		exits("usage");
	}ARGEND;

	/* make a list of dates */
	now = time(0);
	tm = localtime(now);
	last = nil;
	first = dates(&last, tm);
	now += Secondsperday;
	tm = localtime(now);
	dates(&last, tm);
	if(tm->wday == 6){
		now += Secondsperday;
		tm = localtime(now);
		dates(&last, tm);
	}
	if(tm->wday == 0){
		now += Secondsperday;
		tm = localtime(now);
		dates(&last, tm);
	}
	if(ahead){
		now = time(0);
		now += ahead * Secondsperday;
		tm = localtime(now);
		dates(&last, tm);
	}

	for(i=0; i<argc || (i==0 && argc==0); i++){
		if(i==0 && argc==0)
			snprint(buf, sizeof(buf),
				"%s/lib/calendar", getenv("HOME"));
		else
			strcpy(buf, argv[i]);
		fd = open(buf, OREAD);
		if(fd<0 || Binit(&in, fd, OREAD)<0){
			fprint(2, "calendar: can't open %s: %r\n", buf);
			exits("open");
		}

		/* go through the file */
		while(line = Brdline(&in, '\n')){
			line[Blinelen(&in) - 1] = 0;
			upper2lower(buf, line, sizeof buf);
			for(d=first; d; d=d->next)
				if(regexec(d->p, buf, 0, 0)){
					print("%s\n", line);
					break;
				}
		}
		close(fd);
	}
	exits("");
}

char *months[] =
{
	"january",
	"february",
	"march",
	"april",
	"may",
	"june",
	"july",
	"august",
	"september",
	"october",
	"november",
	"december"
};

/*
 * Generate two Date structures.  First has month followed by day;
 * second has day followed by month.  Link them into list after
 * last, and return the first.
 */
Date*
dates(Date **last, Tm *tm)
{
	Date *first;
	Date *nd;
	char mo[128], buf[128];

	if(utflen(months[tm->mon]) > 3)
		snprint(mo, sizeof mo, "%3.3s(%s)?",
			months[tm->mon], months[tm->mon]+3);
	else
		snprint(mo, sizeof mo, "%3.3s", months[tm->mon]);
	if (matchyear)
		snprint(buf, sizeof buf,
			"(^| |\t)((%s( |\t)+)|(%d/))%d( |\t|$)(((%d|%d)( |\t|$))|[^0-9]|([0-9]+[^0-9 \t]))",
			mo, tm->mon+1, tm->mday, tm->year+1900, tm->year%100);
	else
		snprint(buf, sizeof buf,
			"(^| |\t)((%s( |\t)+)|(%d/))%d( |\t|$)",
			mo, tm->mon+1, tm->mday);
	if(debug)
		print("%s\n", buf);

	first = alloc(sizeof(Date));
	if(*last)
		(*last)->next = first;
	first->p = regcomp(buf);

	if (matchyear)
		snprint(buf, sizeof buf,
			"(^| |\t)%d( |\t)+(%s)( |\t|$)(((%d|%d)( |\t|$))|[^0-9]|([0-9]+[^0-9 \t]))",
			tm->mday, mo, tm->year+1900, tm->year%100);
	else
		snprint(buf, sizeof buf,
			"(^| |\t)%d( |\t)+(%s)( |\t|$)",
			tm->mday, mo);
	if(debug)
		print("%s\n", buf);
	nd = alloc(sizeof(Date));
	nd->p = regcomp(buf);
	nd->next = 0;
	first->next = nd;
	*last = nd;

	return first;
}

/*
 * Copy 'from' to 'to', converting to lower case
 */
void
upper2lower(char *to, char *from, int len)
{
	while(--len>0 && *from!='\0')
		*to++ = tolower((uchar)*from++);
	*to = 0;
}

/*
 * Call malloc and check for errors
 */
void*
alloc(unsigned int n)
{
	void *p;

	p = malloc(n);
	if(p == 0){
		fprint(2, "calendar: malloc failed: %r\n");
		exits("malloc");
	}
	return p;
}
