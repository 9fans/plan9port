#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>

/*
 *  search the database for matches
 */
void
usage(void)
{
	fprint(2, "usage: query attr value [returned attribute]\n");
	exits("usage");
}

void
search(Ndb *db, char *attr, char *val, char *rattr)
{
	Ndbs s;
	Ndbtuple *t;
	Ndbtuple *nt;
	char *p;

	if(rattr){
		p = ndbgetvalue(db, &s, attr, val, rattr, nil);
		if(p){
			print("%s\n", p);
			free(p);
		}
		return;
	}

	t = ndbsearch(db, &s, attr, val);
	while(t){
		for(nt = t; nt; nt = nt->entry)
			print("%s=%s ", nt->attr, nt->val);
		print("\n");
		ndbfree(t);
		t = ndbsnext(&s, attr, val);
	}
}

void
main(int argc, char **argv)
{
	char *rattr = 0;
	Ndb *db;
	char *dbfile = 0;
	int reps = 1;

	ARGBEGIN{
	case 'f':
		dbfile = ARGF();
		break;
	}ARGEND;

	switch(argc){
	case 4:
		reps = atoi(argv[3]);
		/* fall through */
	case 3:
		rattr = argv[2];
		break;
	case 2:
		rattr = 0;
		break;
	default:
		usage();
	}

	db = ndbopen(dbfile);
	if(db == 0){
		fprint(2, "no db files\n");
		exits("no db");
	}
	while(reps--)
		search(db, argv[0], argv[1], rattr);
	ndbclose(db);

	exits(0);
}
