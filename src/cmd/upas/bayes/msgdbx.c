#include <u.h>
#include <libc.h>
#include <db.h>
#include "msgdb.h"

struct Msgdb
{
	DB *db;
	int reset;
};

Msgdb*
mdopen(char *file, int create)
{
	Msgdb *mdb;
	DB *db;
	HASHINFO h;

	if((mdb = mallocz(sizeof(Msgdb), 1)) == nil)
		return nil;
	memset(&h, 0, sizeof h);
	h.cachesize = 2*1024*1024;
	if((db = dbopen(file, ORDWR|(create ? OCREATE:0), 0666, DB_HASH, &h)) == nil){
		free(mdb);
		return nil;
	}
	mdb->db = db;
	mdb->reset = 1;
	return mdb;
}

long
mdget(Msgdb *mdb, char *tok)
{
	DB *db = mdb->db;
	DBT key, val;
	uchar *p;

	key.data = tok;
	key.size = strlen(tok)+1;
	val.data = 0;
	val.size = 0;

	if(db->get(db, &key, &val, 0) < 0)
		return 0;
	if(val.data == 0)
		return 0;
	if(val.size != 4)
		return 0;
	p = val.data;
	return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
}

void
mdput(Msgdb *mdb, char *tok, long n)
{
	uchar p[4];
	DB *db = mdb->db;
	DBT key, val;

	key.data = tok;
	key.size = strlen(tok)+1;
	if(n <= 0){
		db->del(db, &key, 0);
		return;
	}

	p[0] = n>>24;
	p[1] = n>>16;
	p[2] = n>>8;
	p[3] = n;

	val.data = p;
	val.size = 4;
	db->put(db, &key, &val, 0);
}

void
mdenum(Msgdb *mdb)
{
	mdb->reset = 1;
}

int
mdnext(Msgdb *mdb, char **sp, long *vp)
{
	DBT key, val;
	uchar *p;
	DB *db = mdb->db;
	int i;

	i = db->seq(db, &key, &val, mdb->reset ? R_FIRST : R_NEXT);
	mdb->reset = 0;
	if(i)
		return -1;
	*sp = key.data;
	p = val.data;
	*vp = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
	return 0;
}

void
mdclose(Msgdb *mdb)
{
	DB *db = mdb->db;

	db->close(db);
	mdb->db = nil;
}
