#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <ndb.h>
#include "ndbhf.h"

static Ndb*	doopen(char*, char*);
static void	hffree(Ndb*);

static char *deffile = "#9/ndb/local";

/*
 *  the database entry in 'file' indicates the list of files
 *  that makeup the database.  Open each one and search in
 *  the same order.
 */
Ndb*
ndbopen(char *file)
{
	Ndb *db, *first, *last;
	Ndbs s;
	Ndbtuple *t, *nt;

	if(file == 0)
		file = unsharp(deffile);
	db = doopen(file, nil);
	if(db == 0)
		return 0;
	first = last = db;
	t = ndbsearch(db, &s, "database", "");
	Bseek(&db->b, 0, 0);
	if(t == 0)
		return db;
	for(nt = t; nt; nt = nt->entry){
		if(strcmp(nt->attr, "file") != 0)
			continue;
		if(strcmp(nt->val, file) == 0){
			/* default file can be reordered in the list */
			if(first->next == 0)
				continue;
			if(strcmp(first->file, file) == 0){
				db = first;
				first = first->next;
				last->next = db;
				db->next = 0;
				last = db;
			}
			continue;
		}
		db = doopen(nt->val, file);
		if(db == 0)
			continue;
		last->next = db;
		last = db;
	}
	ndbfree(t);
	return first;
}

/*
 *  open a single file
 */
static Ndb*
doopen(char *file, char *rel)
{
	char *p;
	Ndb *db;

	db = (Ndb*)malloc(sizeof(Ndb));
	if(db == 0)
		return 0;

	memset(db, 0, sizeof(Ndb));
	/*
	 * Rooted paths are taken as is.
	 * Unrooted paths are taken relative to db we opened.
	 */
	if(file[0]!='/' && rel && (p=strrchr(rel, '/'))!=nil)
		snprint(db->file, sizeof(db->file), "%.*s/%s",
			utfnlen(rel, p-rel), rel, file);
	else
		strncpy(db->file, file, sizeof(db->file)-1);

	if(ndbreopen(db) < 0){
		free(db);
		return 0;
	}

	return db;
}

/*
 *  dump any cached information, forget the hash tables, and reopen a single file
 */
int
ndbreopen(Ndb *db)
{
	int fd;
	Dir *d;

	/* forget what we know about the open files */
	if(db->mtime){
		_ndbcacheflush(db);
		hffree(db);
		close(Bfildes(&db->b));
		Bterm(&db->b);
		db->mtime = 0;
	}

	/* try the open again */
	fd = open(db->file, OREAD);
	if(fd < 0)
		return -1;
	d = dirfstat(fd);
	if(d == nil){
		close(fd);
		return -1;
	}

	db->qid = d->qid;
	db->mtime = d->mtime;
	db->length = d->length;
	Binit(&db->b, fd, OREAD);
	free(d);
	return 0;
}

/*
 *  close the database files
 */
void
ndbclose(Ndb *db)
{
	Ndb *nextdb;

	for(; db; db = nextdb){
		nextdb = db->next;
		_ndbcacheflush(db);
		hffree(db);
		close(Bfildes(&db->b));
		Bterm(&db->b);
		free(db);
	}
}

/*
 *  free the hash files belonging to a db
 */
static void
hffree(Ndb *db)
{
	Ndbhf *hf, *next;

	for(hf = db->hf; hf; hf = next){
		next = hf->next;
		close(hf->fd);
		free(hf);
	}
	db->hf = 0;
}

/*
 *  return true if any part of the database has changed
 */
int
ndbchanged(Ndb *db)
{
	Ndb *ndb;
	Dir *d;

	for(ndb = db; ndb != nil; ndb = ndb->next){
		d = dirfstat(Bfildes(&db->b));
		if(d == nil)
			continue;
		if(ndb->qid.path != d->qid.path
		|| ndb->qid.vers != d->qid.vers){
			free(d);
			return 1;
		}
		free(d);
	}
	return 0;
}
