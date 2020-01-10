#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include "msgdb.h"

void
usage(void)
{
	fprint(2, "usage: upas/msgclass [-a] [-d name dbfile]... [-l lockfile] [-m mul] [-t thresh] [tokenfile ...]\n");
	exits("usage");
}

enum
{
	MAXBEST = 32,
	MAXLEN = 64,
	MAXTAB = 256
};

typedef struct Ndb Ndb;
struct Ndb
{
	char *name;
	char *file;
	Msgdb *db;
	double p;
	long nmsg;
};

typedef struct Word Word;
struct Word
{
	char s[MAXLEN];
	int count[MAXTAB];
	double p[MAXTAB];
	double mp;
	int mi; /* w.p[w.mi] = w.mp */
	int nmsg;
};

Ndb db[MAXTAB];
int ndb;

int add;
int mul;
Msgdb *indb;

Word best[MAXBEST];
int mbest = 15;
int nbest;

void process(Biobuf*, char*);
void lockfile(char*);

void
noteword(Word *w, char *s)
{
	int i;

	for(i=nbest-1; i>=0; i--)
		if(w->mp < best[i].mp)
			break;
	i++;

	if(i >= mbest)
		return;
	if(nbest == mbest)
		nbest--;
	if(i < nbest)
		memmove(&best[i+1], &best[i], (nbest-i)*sizeof(best[0]));
	best[i] = *w;
	strecpy(best[i].s, best[i].s+MAXLEN, s);
	nbest++;
}

void
main(int argc, char **argv)
{
	int i, bad, m, tot, nn, j;
	Biobuf bin, *b, bout;
	char *s, *lf;
	double totp, p, thresh;
	long n;
	Word w;

	lf = nil;
	thresh = 0;
	ARGBEGIN{
	case 'a':
		add = 1;
		break;
	case 'd':
		if(ndb >= MAXTAB)
			sysfatal("too many db classes");
		db[ndb].name = EARGF(usage());
		db[ndb].file = EARGF(usage());
		ndb++;
		break;
	case 'l':
		lf = EARGF(usage());
		break;
	case 'm':
		mul = atoi(EARGF(usage()));
		break;
	case 't':
		thresh = atof(EARGF(usage()));
		break;
	default:
		usage();
	}ARGEND

	if(ndb == 0){
		fprint(2, "must have at least one -d option\n");
		usage();
	}

	indb = mdopen(nil, 1);
	if(argc == 0){
		Binit(&bin, 0, OREAD);
		process(&bin, "<stdin>");
		Bterm(&bin);
	}else{
		bad = 0;
		for(i=0; i<argc; i++){
			if((b = Bopen(argv[i], OREAD)) == nil){
				fprint(2, "opening %s: %r\n", argv[i]);
				bad = 1;
				continue;
			}
			process(b, argv[i]);
			Bterm(b);
		}
		if(bad)
			exits("open inputs");
	}

	lockfile(lf);
	bad = 0;
	for(i=0; i<ndb; i++){
		if((db[i].db = mdopen(db[i].file, 0)) == nil){
			fprint(2, "opendb %s: %r\n", db[i].file);
			bad = 1;
		}
		db[i].nmsg = mdget(db[i].db, "*From*");
	}
	if(bad)
		exits("open databases");

	/* run conditional probabilities of input words, getting 15 most specific */
	mdenum(indb);
	nbest = 0;
	while(mdnext(indb, &s, &n) >= 0){
		tot = 0;
		totp = 0.0;
		for(i=0; i<ndb; i++){
			nn = mdget(db[i].db, s)*(i==0 ? 3 : 1);
			tot += nn;
			w.count[i] = nn;
			p = w.count[i]/(double)db[i].nmsg;
			if(p >= 1.0)
				p = 1.0;
			w.p[i] = p;
			totp += p;
		}
/*fprint(2, "%s tot %d totp %g\n", s, tot, totp); */
		if(tot < 2)
			continue;
		w.mp = 0.0;
		for(i=0; i<ndb; i++){
			p = w.p[i];
			p /= totp;
			if(p < 0.001)
				p = 0.001;
			else if(p > 0.999)
				p = 0.999;
			if(p > w.mp){
				w.mp = p;
				w.mi = i;
			}
			w.p[i] = p;
		}
		noteword(&w, s);
	}

	/* compute conditional probabilities of message classes using 15 most specific */
	totp = 0.0;
	for(i=0; i<ndb; i++){
		p = 1.0;
		for(j=0; j<nbest; j++)
			p *= best[j].p[i];
		db[i].p = p;
		totp += p;
	}
	for(i=0; i<ndb; i++)
		db[i].p /= totp;
	m = 0;
	for(i=1; i<ndb; i++)
		if(db[i].p > db[m].p)
			m = i;

	Binit(&bout, 1, OWRITE);
	if(db[m].p < thresh)
		m = -1;
	if(m >= 0)
		Bprint(&bout, "%s", db[m].name);
	else
		Bprint(&bout, "inconclusive");
	for(j=0; j<ndb; j++)
		Bprint(&bout, " %s=%g", db[j].name, db[j].p);
	Bprint(&bout, "\n");
	for(i=0; i<nbest; i++){
		Bprint(&bout, "%s", best[i].s);
		for(j=0; j<ndb; j++)
			Bprint(&bout, " %s=%g", db[j].name, best[i].p[j]);
		Bprint(&bout, "\n");
	}
		Bprint(&bout, "%s %g\n", best[i].s, best[i].p[m]);
	Bterm(&bout);

	if(m >= 0 && add){
		mdenum(indb);
		while(mdnext(indb, &s, &n) >= 0)
			mdput(db[m].db, s, mdget(db[m].db, s)+n*mul);
		mdclose(db[m].db);
	}
	exits(nil);
}

void
process(Biobuf *b, char*)
{
	char *s;
	char *p;
	long n;

	while((s = Brdline(b, '\n')) != nil){
		s[Blinelen(b)-1] = 0;
		if((p = strrchr(s, ' ')) != nil){
			*p++ = 0;
			n = atoi(p);
		}else
			n = 1;
		mdput(indb, s, mdget(indb, s)+n);
	}
}

int tpid;
void
killtickle(void)
{
	postnote(PNPROC, tpid, "die");
}

void
lockfile(char *s)
{
	int fd, t, w;
	char err[ERRMAX];

	if(s == nil)
		return;
	w = 50;
	t = 0;
	for(;;){
		fd = open(s, OREAD);
		if(fd >= 0)
			break;
		rerrstr(err, sizeof err);
		if(strstr(err, "file is locked")==nil && strstr(err, "exclusive lock")==nil))
			break;
		sleep(w);
		t += w;
		if(w < 1000)
			w = (w*3)/2;
		if(t > 120*1000)
			break;
	}
	if(fd < 0)
		sysfatal("could not lock %s", s);
	switch(tpid = fork()){
	case -1:
		sysfatal("fork: %r");
	case 0:
		for(;;){
			sleep(30*1000);
			free(dirfstat(fd));
		}
		_exits(nil);
	default:
		break;
	}
	close(fd);
	atexit(killtickle);
}
