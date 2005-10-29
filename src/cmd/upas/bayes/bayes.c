#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include "hash.h"

enum
{
	MAXTAB = 256,
	MAXBEST = 32,
};

typedef struct Table Table;
struct Table
{
	char *file;
	Hash *hash;
	int nmsg;
};

typedef struct Word Word;
struct Word
{
	Stringtab *s;	/* from hmsg */
	int count[MAXTAB];	/* counts from each table */
	double p[MAXTAB];	/* probabilities from each table */
	double mp;	/* max probability */
	int mi;		/* w.p[w.mi] = w.mp */
};

Table tab[MAXTAB];
int ntab;

Word best[MAXBEST];
int mbest;
int nbest;

int debug;

void
usage(void)
{
	fprint(2, "usage: bayes [-D] [-m maxword] boxhash ... ~ msghash ...\n");
	exits("usage");
}

void*
emalloc(int n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

void
noteword(Word *w)
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
	nbest++;
}

Hash*
hread(char *s)
{
	Hash *h;
	Biobuf *b;

	if((b = Bopenlock(s, OREAD)) == nil)
		sysfatal("open %s: %r", s);

	h = emalloc(sizeof(Hash));
	Breadhash(b, h, 1);
	Bterm(b);
	return h;
}

void
main(int argc, char **argv)
{
	int i, j, a, mi, oi, tot, keywords;
	double totp, p, xp[MAXTAB];
	Hash *hmsg;
	Word w;
	Stringtab *s, *t;
	Biobuf bout;

	mbest = 15;
	keywords = 0;
	ARGBEGIN{
	case 'D':
		debug = 1;
		break;
	case 'k':
		keywords = 1;
		break;
	case 'm':
		mbest = atoi(EARGF(usage()));
		if(mbest > MAXBEST)
			sysfatal("cannot keep more than %d words", MAXBEST);
		break;
	default:
		usage();
	}ARGEND

	for(i=0; i<argc; i++)
		if(strcmp(argv[i], "~") == 0)
			break;

	if(i > MAXTAB)
		sysfatal("cannot handle more than %d tables", MAXTAB);

	if(i+1 >= argc)
		usage();

	for(i=0; i<argc; i++){
		if(strcmp(argv[i], "~") == 0)
			break;
		tab[ntab].file = argv[i];
		tab[ntab].hash = hread(argv[i]);
		s = findstab(tab[ntab].hash, "*nmsg*", 6, 1);
		if(s == nil || s->count == 0)
			tab[ntab].nmsg = 1;
		else
			tab[ntab].nmsg = s->count;
		ntab++;
	}

	Binit(&bout, 1, OWRITE);

	oi = ++i;
	for(a=i; a<argc; a++){
		hmsg = hread(argv[a]);
		nbest = 0;
		for(s=hmsg->all; s; s=s->link){
			w.s = s;
			tot = 0;
			totp = 0.0;
			for(i=0; i<ntab; i++){
				t = findstab(tab[i].hash, s->str, s->n, 0);
				if(t == nil)
					w.count[i] = 0;
				else
					w.count[i] = t->count;
				tot += w.count[i];
				p = w.count[i]/(double)tab[i].nmsg;
				if(p >= 1.0)
					p = 1.0;
				w.p[i] = p;
				totp += p;
			}

			if(tot < 5){		/* word does not appear enough; give to box 0 */
				w.p[0] = 0.5;
				for(i=1; i<ntab; i++)
					w.p[i] = 0.1;
				w.mp = 0.5;
				w.mi = 0;
				noteword(&w);
				continue;
			}

			w.mp = 0.0;
			for(i=0; i<ntab; i++){
				p = w.p[i];
				p /= totp;
				if(p < 0.01)
					p = 0.01;
				else if(p > 0.99)
					p = 0.99;
				if(p > w.mp){
					w.mp = p;
					w.mi = i;
				}
				w.p[i] = p;
			}
			noteword(&w);
		}

		totp = 0.0;
		for(i=0; i<ntab; i++){
			p = 1.0;
			for(j=0; j<nbest; j++)
				p *= best[j].p[i];
			xp[i] = p;
			totp += p;
		}
		for(i=0; i<ntab; i++)
			xp[i] /= totp;
		mi = 0;
		for(i=1; i<ntab; i++)
			if(xp[i] > xp[mi])
				mi = i;
		if(oi != argc-1)
			Bprint(&bout, "%s: ", argv[a]);
		Bprint(&bout, "%s %f", tab[mi].file, xp[mi]);
		if(keywords){
			for(i=0; i<nbest; i++){
				Bprint(&bout, " ");
				Bwrite(&bout, best[i].s->str, best[i].s->n);
				Bprint(&bout, " %f", best[i].p[mi]);
			}
		}
		freehash(hmsg);
		Bprint(&bout, "\n");
		if(debug){
			for(i=0; i<nbest; i++){
				Bwrite(&bout, best[i].s->str, best[i].s->n);
				Bprint(&bout, " %f", best[i].p[mi]);
				if(best[i].p[mi] < best[i].mp)
					Bprint(&bout, " (%f %s)", best[i].mp, tab[best[i].mi].file);
				Bprint(&bout, "\n");
			}
		}
	}
	Bterm(&bout);
}
