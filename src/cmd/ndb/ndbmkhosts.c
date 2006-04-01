#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <ip.h>

typedef struct x
{
	Ndbtuple *t;
	Ndbtuple *it;
	Ndbtuple *nt;
} X;

X x[4096];
int nx;
char *domname = "research.att.com";
int domnamlen;

char*
upper(char *x)
{
	char *p;
	int c;

	for(p = x; c = *p; p++)
		*p = toupper(c);
	return x;
}

void
printArecord(int fd, X *p)
{
	Ndbtuple *nt;
	char *c;
	char *dom = 0;
	char *curdom = 0;
	int i, cdlen = 0;
	int mxweight = 0;

	if(p->nt) {
		return;
	}
	for(nt=p->t; nt; nt = nt->entry) {
		/* we are only going to handle things in the specified domain */
		c = strchr(nt->val, '.');
		if (c==0 || strcmp(++c, domname)!=0)
			continue;
		i = c - nt->val - 1;
		if(strcmp(nt->attr, "dom") == 0) {
			curdom = nt->val;
			cdlen = i;
			if (dom == 0) {
				dom = curdom;
				fprint(fd, "%-.*s%.*s	IN A	%s\n", i, nt->val, 15-i, "               ", p->it->val);
			} else
				fprint(fd, "%-.*s%.*s	IN CNAME	%s.\n", i, nt->val, 15-i, "               ", dom);
		} else if(strcmp(nt->attr, "mx") == 0) {
			if (curdom != 0)
				fprint(fd, "%-.*s%.*s	MX	%d	%s.\n", cdlen, curdom, 15-cdlen, "               ", mxweight++, nt->val);
		}
	}
}

void
printentry(int fd, X *p)
{
	Ndbtuple *nt;

	if(p->nt)
		return;
	fprint(fd, "%s	", p->it->val);
	for(nt = p->t; nt; nt = nt->entry)
		if(strcmp(nt->attr, "dom") == 0)
			fprint(fd, " %s", nt->val);
	for(nt = p->t; nt; nt = nt->entry)
		if(strcmp(nt->attr, "sys") == 0)
			fprint(fd, " %s", nt->val);
	fprint(fd, "\n");
}

void
printsys(int fd, X *p)
{
	Ndbtuple *nt;

	for(nt = p->t; nt; nt = nt->entry)
		if(strcmp(nt->attr, "dom") == 0)
			fprint(fd, "%s\n", nt->val);
}

void
printtxt(int fd, X *p)
{
	int i;
	Ndbtuple *nt;

	if(p->nt){
		for(;;){
			i = strlen(p->it->val);
			if(strcmp(p->it->val+i-2, ".0") == 0)
				p->it->val[i-2] = 0;
			else
				break;
		}
		fprint(fd, "\nNET : %s : %s\n", p->it->val, upper(p->nt->val));
		return;
	}
	fprint(fd, "HOST : %s :", p->it->val);
	i = 0;
	for(nt = p->t; nt; nt = nt->entry)
		if(strcmp(nt->attr, "dom") == 0){
			if(i++ == 0)
				fprint(fd, " %s", upper(nt->val));
			else
				fprint(fd, ", %s", upper(nt->val));
		}
	fprint(fd, "\n");
}

void
parse(char *file)
{
	int i;
	Ndb *db;
	Ndbtuple *t, *nt, *tt, *ipnett;
	char *p;

	db = ndbopen(file);
	if(db == 0)
		exits("no database");
	while(t = ndbparse(db)){
		for(nt = t; nt; nt = nt->entry){
			if(strcmp(nt->attr, "ip") == 0)
				break;
			if(strcmp(nt->attr, "flavor") == 0
			&& strcmp(nt->val, "console") == 0)
				return;
		}
		if(nt == 0){
			ndbfree(t);
			continue;
		}

		/* dump anything not on our nets */
		ipnett = 0;
		for(tt = t; tt; tt = tt->entry){
			if(strcmp(tt->attr, "ipnet") == 0){
				ipnett = tt;
				break;
			}
			if(strcmp(tt->attr, "dom") == 0){
				i = strlen(tt->val);
				p = tt->val+i-domnamlen;
				if(p >= tt->val && strcmp(p, domname) == 0)
					break;
			}
		}
		if(tt == 0){
			ndbfree(t);
			continue;
		}

		for(; nt; nt = nt->entry){
			if(strcmp(nt->attr, "ip") != 0)
				continue;
			x[nx].it = nt;
			x[nx].nt = ipnett;
			x[nx++].t = t;
		}
	}
}

void
main(int argc, char *argv[])
{
	int i, fd;
	char fn[128];

	if (argc>1)
		domname = argv[1];
	domnamlen = strlen(domname);
	if(argc > 2){
		for(i = 2; i < argc; i++)
			parse(argv[i]);
	} else {
		parse(unsharp("#9/ndb/local"));
		parse(unsharp("#9/ndb/friends"));
	}

/*	sprint(fn, "/lib/ndb/hosts.%-.21s", domname); */
/*	fd = create(fn, OWRITE, 0664); */
/*	if(fd < 0){ */
/*		fprint(2, "can't create %s: %r\n", fn); */
/*		exits("boom"); */
/*	} */
/*	for(i = 0; i < nx; i++) */
/*		printentry(fd, &x[i]); */
/*	close(fd); */


	sprint(fn, "/lib/ndb/db.%-.24s", domname);
	fd = create(fn, OWRITE, 0664);
	if(fd < 0){
		fprint(2, "can't create %s: %r\n", fn);
		exits("boom");
	}
	fprint(fd, "; This file is generated automatically, do not edit!\n");
	for(i = 0; i < nx; i++)
		printArecord(fd, &x[i]);
	close(fd);

	sprint(fn, "/lib/ndb/equiv.%-.21s", domname);
	fd = create(fn, OWRITE, 0664);
	if(fd < 0){
		fprint(2, "can't create %s: %r\n", fn);
		exits("boom");
	}
	for(i = 0; i < nx; i++)
		printsys(fd, &x[i]);
	close(fd);

	sprint(fn, "/lib/ndb/txt.%-.23s", domname);
	fd = create(fn, OWRITE, 0664);
	if(fd < 0){
		fprint(2, "can't create %s: %r\n", fn);
		exits("boom");
	}
	for(i = 0; i < nx; i++)
		printtxt(fd, &x[i]);
	close(fd);

	exits(0);
}
