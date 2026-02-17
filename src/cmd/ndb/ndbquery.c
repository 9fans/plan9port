#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>

static int all, multiple;

/*
 *  search the database for matches
 */
void
usage(void)
{
        fprint(2, "usage: ndbquery [-am] [-f ndbfile] attr value [returned attribute]\n");
        exits("usage");
}

static void
prmatch(Ndbtuple *nt, char *rattr)
{
        for(; nt; nt = nt->entry)
                if(strcmp(nt->attr, rattr) == 0)
                        print("%s\n", nt->val);
}

void
search(Ndb *db, char *attr, char *val, char *rattr)
{
        Ndbs s;
        Ndbtuple *t;
        Ndbtuple *nt;
        char *p;

        if(rattr && !all){
                p = ndbgetvalue(db, &s, attr, val, rattr, &t);
                if(multiple)
                        prmatch(t, rattr);
                else if(p){
                        print("%s\n", p);
                }
                ndbfree(t);
                free(p);
                return;
        }

        /* all entries with matching rattrs */
        if(rattr){
                t = ndbsearch(db, &s, attr, val);
                while(t != nil){
                        prmatch(t, rattr);
                        ndbfree(t);
                        t = ndbsnext(&s, attr, val);
                }
                return;
        }

        /* all entries */
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
        case 'a':
                all++;
                break;
        case 'm':
                multiple++;
                break;
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
