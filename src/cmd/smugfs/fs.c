#include "a.h"

enum
{
	Qroot = 0,        //  /smug/
	Qctl,             //  /smug/ctl
	Qrpclog,          //  /smug/rpclog
	Quploads,		// /smug/uploads
	Qnick,            //  /smug/nick/
	Qnickctl,         //  /smug/nick/ctl
	Qalbums,          //  /smug/nick/albums/
	Qalbumsctl,       //  /smug/nick/albums/ctl
	Qcategory,        //  /smug/nick/Category/
	Qcategoryctl,     //  /smug/nick/Category/ctl
	Qalbum,           //  /smug/nick/Category/Album/
	Qalbumctl,        //  /smug/nick/Category/Album/ctl
	Qalbumsettings,   //  /smug/nick/Category/Album/settings
	Quploadfile,      //  /smug/nick/Category/Album/upload/file.jpg
	Qimage,           //  /smug/nick/Category/Album/Image/
	Qimagectl,        //  /smug/nick/Category/Album/Image/ctl
	Qimageexif,       //  /smug/nick/Category/Album/Image/exif
	Qimagesettings,   //  /smug/nick/Category/Album/Image/settings
	Qimageurl,        //  /smug/nick/Category/Album/Image/url
	Qimagefile,       //  /smug/nick/Category/Album/Image/file.jpg
};

void
mylock(Lock *lk)
{
	lock(lk);
	fprint(2, "locked from %p\n", getcallerpc(&lk));
}

void
myunlock(Lock *lk)
{
	unlock(lk);
	fprint(2, "unlocked from %p\n", getcallerpc(&lk));
}

//#define lock mylock
//#define unlock myunlock

typedef struct Upload Upload;

typedef struct SmugFid SmugFid;
struct SmugFid
{
	int type;
	int nickid;
	vlong category;  // -1 for "albums"
	vlong album;
	char *albumkey;
	vlong image;
	char *imagekey;
	Upload *upload;
	int upwriter;
};

#define QTYPE(p) ((p)&0xFF)
#define QARG(p) ((p)>>8)
#define QPATH(p, q) ((p)|((q)<<8))

char **nick;
int nnick;

struct Upload
{
	Lock lk;
	int fd;
	char *name;
	char *file;
	vlong album;
	vlong length;
	char *albumkey;
	int size;
	int ready;
	int nwriters;
	int uploaded;
	int ref;
	int uploading;
};

Upload **up;
int nup;
QLock uploadlock;
Rendez uploadrendez;

void uploader(void*);

Upload*
newupload(SmugFid *sf, char *name)
{
	Upload *u;
	int fd, i;
	char tmp[] = "/var/tmp/smugfs.XXXXXX";

	if((fd = opentemp(tmp, ORDWR)) < 0)
		return nil;
	qlock(&uploadlock);
	for(i=0; i<nup; i++){
		u = up[i];
		lock(&u->lk);
		if(u->ref == 0){
			u->ref = 1;
			goto Reuse;
		}
		unlock(&u->lk);
	}
	if(nup == 0){
		uploadrendez.l = &uploadlock;
		proccreate(uploader, nil, STACKSIZE);
	}
	u = emalloc(sizeof *u);
	lock(&u->lk);
	u->ref = 1;
	up = erealloc(up, (nup+1)*sizeof up[0]);
	up[nup++] = u;
Reuse:
	qunlock(&uploadlock);
	u->fd = fd;
	u->name = estrdup(name);
	u->file = estrdup(tmp);
	u->album = sf->album;
	u->albumkey = estrdup(sf->albumkey);
	u->nwriters = 1;
	unlock(&u->lk);
	return u;
}

void
closeupload(Upload *u)
{
	lock(&u->lk);
	if(--u->ref > 0){
		unlock(&u->lk);
		return;
	}
	if(u->ref < 0)
		abort();
	if(u->fd >= 0){
		close(u->fd);
		u->fd = -1;
	}
	if(u->name){
		free(u->name);
		u->name = nil;
	}
	if(u->file){
		remove(u->file);
		free(u->file);
		u->file = nil;
	}
	u->album = 0;
	if(u->albumkey){
		free(u->albumkey);
		u->albumkey = nil;
	}
	u->size = 0;
	u->ready = 0;
	u->nwriters = 0;
	u->uploaded = 0;
	u->uploading = 0;
	u->length = 0;
	unlock(&u->lk);
}

Upload*
getuploadindex(SmugFid *sf, int *index)
{
	int i;
	Upload *u;

	qlock(&uploadlock);
	for(i=0; i<nup; i++){
		u = up[i];
		lock(&u->lk);
		if(u->ref > 0 && !u->uploaded && u->album == sf->album && (*index)-- == 0){
			qunlock(&uploadlock);
			u->ref++;
			unlock(&u->lk);
			return u;
		}
		unlock(&u->lk);
	}
	qunlock(&uploadlock);
	return nil;
}

Upload*
getuploadname(SmugFid *sf, char *name)
{
	int i;
	Upload *u;

	qlock(&uploadlock);
	for(i=0; i<nup; i++){
		u = up[i];
		lock(&u->lk);
		if(u->ref > 0 && !u->uploaded && u->album == sf->album && strcmp(name, u->name) == 0){
			qunlock(&uploadlock);
			u->ref++;
			unlock(&u->lk);
			return u;
		}
		unlock(&u->lk);
	}
	qunlock(&uploadlock);
	return nil;
}

void doupload(Upload*);

void
uploader(void *v)
{
	int i, did;
	Upload *u;

	qlock(&uploadlock);
	for(;;){
		did = 0;
		for(i=0; i<nup; i++){
			u = up[i];
			lock(&u->lk);
			if(u->ref > 0 && u->ready && !u->uploading && !u->uploaded){
				u->uploading = 1;
				unlock(&u->lk);
				qunlock(&uploadlock);
				doupload(u);
				closeupload(u);
				did = 1;
				qlock(&uploadlock);
			}else
				unlock(&u->lk);
		}
		if(!did)
			rsleep(&uploadrendez);
	}
}

void
kickupload(Upload *u)
{
	Dir *d;

	lock(&u->lk);
	if((d = dirfstat(u->fd)) != nil)
		u->length = d->length;
	close(u->fd);
	u->fd = -1;
	u->ref++;
	u->ready = 1;
	unlock(&u->lk);
	qlock(&uploadlock);
	rwakeup(&uploadrendez);
	qunlock(&uploadlock);
}

void
doupload(Upload *u)
{
	Dir *d;
	vlong datalen;
	Fmt fmt;
	char *req;
	char buf[8192];
	int n, total;
	uchar digest[MD5dlen];
	DigestState ds;
	Json *jv;

	if((u->fd = open(u->file, OREAD)) < 0){
		fprint(2, "cannot reopen temporary file %s: %r\n", u->file);
		return;
	}
	if((d = dirfstat(u->fd)) == nil){
		fprint(2, "fstat: %r\n");
		return;
	}
	datalen = d->length;
	free(d);

	memset(&ds, 0, sizeof ds);
	seek(u->fd, 0, 0);
	total = 0;
	while((n = read(u->fd, buf, sizeof buf)) > 0){
		md5((uchar*)buf, n, nil, &ds);
		total += n;
	}
	if(total != datalen){
		fprint(2, "bad total: %lld %lld\n", total, datalen);
		return;
	}
	md5(nil, 0, digest, &ds);

	fmtstrinit(&fmt);
	fmtprint(&fmt, "PUT /%s HTTP/1.0\r\n", u->name);
	fmtprint(&fmt, "Content-Length: %lld\r\n", datalen);
	fmtprint(&fmt, "Content-MD5: %.16lH\r\n", digest);
	fmtprint(&fmt, "X-Smug-SessionID: %s\r\n", sessid);
	fmtprint(&fmt, "X-Smug-Version: %s\r\n", API_VERSION);
	fmtprint(&fmt, "X-Smug-ResponseType: JSON\r\n");
	// Can send X-Smug-ImageID instead to replace existing files.
	fmtprint(&fmt, "X-Smug-AlbumID: %lld\r\n", u->album);
	fmtprint(&fmt, "X-Smug-FileName: %s\r\n", u->name);
	fmtprint(&fmt, "\r\n");
	req = fmtstrflush(&fmt);

	seek(u->fd, 0, 0);
	jv = jsonupload(&http, UPLOAD_HOST, req, u->fd, datalen);
	free(req);
	if(jv == nil){
		fprint(2, "upload: %r\n");
		return;
	}

	close(u->fd);
	remove(u->file);
	free(u->file);
	u->file = nil;
	u->fd = -1;
	u->uploaded = 1;
	rpclog("uploaded: %J", jv);
	jclose(jv);
}

int
nickindex(char *name)
{
	int i;
	Json *v;

	for(i=0; i<nnick; i++)
		if(strcmp(nick[i], name) == 0)
			return i;
	v = smug("smugmug.users.getTree", "NickName", name, nil);
	if(v == nil)
		return -1;
	nick = erealloc(nick, (nnick+1)*sizeof nick[0]);
	nick[nnick] = estrdup(name);
	return nnick++;
}

char*
nickname(int i)
{
	if(i < 0 || i >= nnick)
		return nil;
	return nick[i];
}

void
responderrstr(Req *r)
{
	char err[ERRMAX];

	rerrstr(err, sizeof err);
	respond(r, err);
}

static char*
xclone(Fid *oldfid, Fid *newfid)
{
	SmugFid *sf;

	if(oldfid->aux == nil)
		return nil;

	sf = emalloc(sizeof *sf);
	*sf = *(SmugFid*)oldfid->aux;
	sf->upload = nil;
	sf->upwriter = 0;
	if(sf->albumkey)
		sf->albumkey = estrdup(sf->albumkey);
	if(sf->imagekey)
		sf->imagekey = estrdup(sf->imagekey);
	newfid->aux = sf;
	return nil;
}

static void
xdestroyfid(Fid *fid)
{
	SmugFid *sf;

	sf = fid->aux;
	free(sf->albumkey);
	free(sf->imagekey);
	if(sf->upload){
		if(sf->upwriter && --sf->upload->nwriters == 0){
			fprint(2, "should upload %s\n", sf->upload->name);
			kickupload(sf->upload);
		}
		closeupload(sf->upload);
		sf->upload = nil;
	}
	free(sf);
}

static Json*
getcategories(SmugFid *sf)
{
	Json *v, *w;

	v = smug("smugmug.categories.get", "NickName", nickname(sf->nickid), nil);
	w = jincref(jwalk(v, "Categories"));
	jclose(v);
	return w;
}

static Json*
getcategorytree(SmugFid *sf)
{
	Json *v, *w;

	v = smug("smugmug.users.getTree", "NickName", nickname(sf->nickid), nil);
	w = jincref(jwalk(v, "Categories"));
	jclose(v);
	return w;
}

static Json*
getcategory(SmugFid *sf, vlong id)
{
	int i;
	Json *v, *w;

	v = getcategorytree(sf);
	if(v == nil)
		return nil;
	for(i=0; i<v->len; i++){
		if(jint(jwalk(v->value[i], "id")) == id){
			w = jincref(v->value[i]);
			jclose(v);
			return w;
		}
	}
	jclose(v);
	return nil;
}

static vlong
getcategoryid(SmugFid *sf, char *name)
{
	int i;
	vlong id;
	Json *v;

	v = getcategories(sf);
	if(v == nil)
		return -1;
	for(i=0; i<v->len; i++){
		if(jstrcmp(jwalk(v->value[i], "Name"), name) == 0){
			id = jint(jwalk(v->value[i], "id"));
			if(id < 0){
				jclose(v);
				return -1;
			}
			jclose(v);
			return id;
		}
	}
	jclose(v);
	return -1;
}

static vlong
getcategoryindex(SmugFid *sf, int i)
{
	Json *v;
	vlong id;

	v = getcategories(sf);
	if(v == nil)
		return -1;
	if(i < 0 || i >= v->len){
		jclose(v);
		return -1;
	}
	id = jint(jwalk(v->value[i], "id"));
	jclose(v);
	return id;
}

static Json*
getalbum(SmugFid *sf, vlong albumid, char *albumkey)
{
	char id[50];
	Json *v, *w;

	snprint(id, sizeof id, "%lld", albumid);
	v = smug("smugmug.albums.getInfo",
		"AlbumID", id, "AlbumKey", albumkey,
		"NickName", nickname(sf->nickid), nil);
	w = jincref(jwalk(v, "Album"));
	jclose(v);
	return w;
}

static Json*
getalbums(SmugFid *sf)
{
	Json *v, *w;

	if(sf->category >= 0)
		v = getcategory(sf, sf->category);
	else
		v = smug("smugmug.albums.get",
			"NickName", nickname(sf->nickid), nil);
	w = jincref(jwalk(v, "Albums"));
	jclose(v);
	return w;
}

static vlong
getalbumid(SmugFid *sf, char *name, char **keyp)
{
	int i;
	vlong id;
	Json *v;
	char *key;

	v = getalbums(sf);
	if(v == nil)
		return -1;
	for(i=0; i<v->len; i++){
		if(jstrcmp(jwalk(v->value[i], "Title"), name) == 0){
			id = jint(jwalk(v->value[i], "id"));
			key = jstring(jwalk(v->value[i], "Key"));
			if(id < 0 || key == nil){
				jclose(v);
				return -1;
			}
			if(keyp)
				*keyp = estrdup(key);
			jclose(v);
			return id;
		}
	}
	jclose(v);
	return -1;
}

static vlong
getalbumindex(SmugFid *sf, int i, char **keyp)
{
	vlong id;
	Json *v;
	char *key;

	v = getalbums(sf);
	if(v == nil)
		return -1;
	if(i < 0 || i >= v->len){
		jclose(v);
		return -1;
	}
	id = jint(jwalk(v->value[i], "id"));
	key = jstring(jwalk(v->value[i], "Key"));
	if(id < 0 || key == nil){
		jclose(v);
		return -1;
	}
	if(keyp)
		*keyp = estrdup(key);
	jclose(v);
	return id;
}

static Json*
getimages(SmugFid *sf, vlong albumid, char *albumkey)
{
	char id[50];
	Json *v, *w;

	snprint(id, sizeof id, "%lld", albumid);
	v = smug("smugmug.images.get",
		"AlbumID", id, "AlbumKey", albumkey,
		"NickName", nickname(sf->nickid), nil);
	w = jincref(jwalk(v, "Images"));
	jclose(v);
	return w;
}

static vlong
getimageid(SmugFid *sf, char *name, char **keyp)
{
	int i;
	vlong id;
	Json *v;
	char *p;
	char *key;

	id = strtol(name, &p, 10);
	if(*p != 0 || *name == 0)
		return -1;

	v = getimages(sf, sf->album, sf->albumkey);
	if(v == nil)
		return -1;
	for(i=0; i<v->len; i++){
		if(jint(jwalk(v->value[i], "id")) == id){
			key = jstring(jwalk(v->value[i], "Key"));
			if(key == nil){
				jclose(v);
				return -1;
			}
			if(keyp)
				*keyp = estrdup(key);
			jclose(v);
			return id;
		}
	}
	jclose(v);
	return -1;
}

static Json*
getimageinfo(SmugFid *sf, vlong imageid, char *imagekey)
{
	char id[50];
	Json *v, *w;

	snprint(id, sizeof id, "%lld", imageid);
	v = smug("smugmug.images.getInfo",
		"ImageID", id, "ImageKey", imagekey,
		"NickName", nickname(sf->nickid), nil);
	w = jincref(jwalk(v, "Image"));
	jclose(v);
	return w;
}

static Json*
getimageexif(SmugFid *sf, vlong imageid, char *imagekey)
{
	char id[50];
	Json *v, *w;

	snprint(id, sizeof id, "%lld", imageid);
	v = smug("smugmug.images.getEXIF",
		"ImageID", id, "ImageKey", imagekey,
		"NickName", nickname(sf->nickid), nil);
	w = jincref(jwalk(v, "Image"));
	jclose(v);
	return w;
}

static vlong
getimageindex(SmugFid *sf, int i, char **keyp)
{
	vlong id;
	Json *v;
	char *key;

	v = getimages(sf, sf->album, sf->albumkey);
	if(v == nil)
		return -1;
	if(i < 0 || i >= v->len){
		jclose(v);
		return -1;
	}
	id = jint(jwalk(v->value[i], "id"));
	key = jstring(jwalk(v->value[i], "Key"));
	if(id < 0 || key == nil){
		jclose(v);
		return -1;
	}
	if(keyp)
		*keyp = estrdup(key);
	jclose(v);
	return id;
}

static char*
categoryname(SmugFid *sf)
{
	Json *v;
	char *s;

	v = getcategory(sf, sf->category);
	s = jstring(jwalk(v, "Name"));
	if(s)
		s = estrdup(s);
	jclose(v);
	return s;
}

static char*
albumname(SmugFid *sf)
{
	Json *v;
	char *s;

	v = getalbum(sf, sf->album, sf->albumkey);
	s = jstring(jwalk(v, "Title"));
	if(s)
		s = estrdup(s);
	jclose(v);
	return s;
}

static char*
imagename(SmugFid *sf)
{
	char *s;
	Json *v;

	v = getimageinfo(sf, sf->image, sf->imagekey);
	s = jstring(jwalk(v, "FileName"));
	if(s && s[0])
		s = estrdup(s);
	else
		s = smprint("%lld.jpg", sf->image);	// TODO: use Format
	jclose(v);
	return s;
}

static vlong
imagelength(SmugFid *sf)
{
	vlong length;
	Json *v;

	v = getimageinfo(sf, sf->image, sf->imagekey);
	length = jint(jwalk(v, "Size"));
	jclose(v);
	return length;
}

static struct {
	char *key;
	char *name;
} urls[] = {
	"AlbumURL", "album",
	"TinyURL", "tiny",
	"ThumbURL", "thumb",
	"SmallURL", "small",
	"MediumURL", "medium",
	"LargeURL", "large",
	"XLargeURL", "xlarge",
	"X2LargeURL", "xxlarge",
	"X3LargeURL", "xxxlarge",
	"OriginalURL", "original",
};

static char*
imageurl(SmugFid *sf)
{
	Json *v;
	char *s;
	int i;

	v = getimageinfo(sf, sf->image, sf->imagekey);
	for(i=nelem(urls)-1; i>=0; i--){
		if((s = jstring(jwalk(v, urls[i].key))) != nil){
			s = estrdup(s);
			jclose(v);
			return s;
		}
	}
	jclose(v);
	return nil;
}

static char* imagestrings[] =
{
	"Caption",
	"LastUpdated",
	"FileName",
	"MD5Sum",
	"Watermark",
	"Format",
	"Keywords",
	"Date",
	"AlbumURL",
	"TinyURL",
	"ThumbURL",
	"SmallURL",
	"MediumURL",
	"LargeURL",
	"XLargeURL",
	"X2LargeURL",
	"X3LargeURL",
	"OriginalURL",
	"Album",
};

static char* albumbools[] =
{
	"Public",
	"Printable",
	"Filenames",
	"Comments",
	"External",
	"Originals",
	"EXIF",
	"Share",
	"SortDirection",
	"FamilyEdit",
	"FriendEdit",
	"HideOwner",
	"CanRank",
	"Clean",
	"Geography",
	"SmugSearchable",
	"WorldSearchable",
	"SquareThumbs",
	"X2Larges",
	"X3Larges",
};

static char* albumstrings[] =
{
	"Description"
	"Keywords",
	"Password",
	"PasswordHint",
	"SortMethod",
	"LastUpdated",
};

static char*
readctl(SmugFid *sf)
{
	int i;
	Upload *u;
	char *s;
	Json *v, *vv;
	Fmt fmt;

	v = nil;
	switch(sf->type){
	case Qctl:
		return smprint("%#J\n", userinfo);

	case Quploads:
		fmtstrinit(&fmt);
		qlock(&uploadlock);
		for(i=0; i<nup; i++){
			u = up[i];
			lock(&u->lk);
			if(u->ready && !u->uploaded && u->ref > 0)
				fmtprint(&fmt, "%s %s%s\n", u->name, u->file, u->uploading ? " [uploading]" : "");
			unlock(&u->lk);
		}
		qunlock(&uploadlock);
		return fmtstrflush(&fmt);

	case Qnickctl:
		v = getcategories(sf);
		break;

	case Qcategoryctl:
		v = getcategory(sf, sf->category);
		break;

	case Qalbumctl:
		v = getimages(sf, sf->album, sf->albumkey);
		break;

	case Qalbumsctl:
		v = getalbums(sf);
		break;

	case Qimagectl:
		v = getimageinfo(sf, sf->image, sf->imagekey);
		break;

	case Qimageurl:
		v = getimageinfo(sf, sf->image, sf->imagekey);
		fmtstrinit(&fmt);
		for(i=0; i<nelem(urls); i++)
			if((s = jstring(jwalk(v, urls[i].key))) != nil)
				fmtprint(&fmt, "%s %s\n", urls[i].name, s);
		jclose(v);
		return fmtstrflush(&fmt);

	case Qimageexif:
		v = getimageexif(sf, sf->image, sf->imagekey);
		break;

	case Qalbumsettings:
		v = getalbum(sf, sf->album, sf->albumkey);
		fmtstrinit(&fmt);
		fmtprint(&fmt, "id\t%lld\n", jint(jwalk(v, "id")));
		// TODO: Category/id
		// TODO: SubCategory/id
		// TODO: Community/id
		// TODO: Template/id
		fmtprint(&fmt, "Highlight\t%lld\n", jint(jwalk(v, "Highlight/id")));
		fmtprint(&fmt, "Position\t%lld\n", jint(jwalk(v, "Position")));
		fmtprint(&fmt, "ImageCount\t%lld\n", jint(jwalk(v, "ImageCount")));
		for(i=0; i<nelem(albumbools); i++){
			vv = jwalk(v, albumbools[i]);
			if(vv)
				fmtprint(&fmt, "%s\t%J\n", albumbools[i], vv);
		}
		for(i=0; i<nelem(albumstrings); i++){
			s = jstring(jwalk(v, albumstrings[i]));
			if(s)
				fmtprint(&fmt, "%s\t%s\n", albumstrings[i], s);
		}
		s = fmtstrflush(&fmt);
		jclose(v);
		return s;

	case Qimagesettings:
		v = getimageinfo(sf, sf->image, sf->imagekey);
		fmtstrinit(&fmt);
		fmtprint(&fmt, "id\t%lld\n", jint(jwalk(v, "id")));
		fmtprint(&fmt, "Position\t%lld\n", jint(jwalk(v, "Position")));
		fmtprint(&fmt, "Serial\t%lld\n", jint(jwalk(v, "Serial")));
		fmtprint(&fmt, "Size\t%lld\t%lldx%lld\n",
			jint(jwalk(v, "Size")),
			jint(jwalk(v, "Width")),
			jint(jwalk(v, "Height")));
		vv = jwalk(v, "Hidden");
		fmtprint(&fmt, "Hidden\t%J\n", vv);
		// TODO: Album/id
		for(i=0; i<nelem(imagestrings); i++){
			s = jstring(jwalk(v, imagestrings[i]));
			if(s)
				fmtprint(&fmt, "%s\t%s\n", imagestrings[i], s);
		}
		s = fmtstrflush(&fmt);
		jclose(v);
		return s;
	}

	if(v == nil)
		return estrdup("");
	s = smprint("%#J\n", v);
	jclose(v);
	return s;
}


static void
dostat(SmugFid *sf, Qid *qid, Dir *dir)
{
	Qid q;
	char *name;
	int freename;
	ulong mode;
	char *uid;
	char *s;
	vlong length;

	memset(&q, 0, sizeof q);
	name = nil;
	freename = 0;
	uid = "smugfs";
	q.type = 0;
	q.vers = 0;
	q.path = QPATH(sf->type, sf->nickid);
	length = 0;
	mode = 0444;

	switch(sf->type){
	case Qroot:
		name = "/";
		q.type = QTDIR;
		break;
	case Qctl:
		name = "ctl";
		mode |= 0222;
		break;
	case Quploads:
		name = "uploads";
		s = readctl(sf);
		if(s){
			length = strlen(s);
			free(s);
		}
		break;
	case Qrpclog:
		name = "rpclog";
		break;
	case Qnick:
		name = nickname(sf->nickid);
		q.type = QTDIR;
		break;
	case Qnickctl:
		name = "ctl";
		mode |= 0222;
		break;
	case Qalbums:
		name = "albums";
		q.type = QTDIR;
		break;
	case Qalbumsctl:
		name = "ctl";
		mode |= 0222;
		break;
	case Qcategory:
		name = categoryname(sf);
		freename = 1;
		q.path |= QPATH(0, sf->category << 8);
		q.type = QTDIR;
		break;
	case Qcategoryctl:
		name = "ctl";
		mode |= 0222;
		q.path |= QPATH(0, sf->category << 8);
		break;
	case Qalbum:
		name = albumname(sf);
		freename = 1;
		q.path |= QPATH(0, sf->album << 8);
		q.type = QTDIR;
		break;
	case Qalbumctl:
		name = "ctl";
		mode |= 0222;
		q.path |= QPATH(0, sf->album << 8);
		break;
	case Qalbumsettings:
		name = "settings";
		mode |= 0222;
		q.path |= QPATH(0, sf->album << 8);
		break;
	case Quploadfile:
		q.path |= QPATH(0, (uintptr)sf->upload << 8);
		if(sf->upload){
			Dir *dd;
			name = sf->upload->name;
			if(sf->upload->fd >= 0){
				dd = dirfstat(sf->upload->fd);
				if(dd){
					length = dd->length;
					free(dd);
				}
			}else
				length = sf->upload->length;
			if(!sf->upload->ready)
				mode |= 0222;
		}
		break;
	case Qimage:
		name = smprint("%lld", sf->image);
		freename = 1;
		q.path |= QPATH(0, sf->image << 8);
		q.type = QTDIR;
		break;
	case Qimagectl:
		name = "ctl";
		mode |= 0222;
		q.path |= QPATH(0, sf->image << 8);
		break;
	case Qimagesettings:
		name = "settings";
		mode |= 0222;
		q.path |= QPATH(0, sf->image << 8);
		break;
	case Qimageexif:
		name = "exif";
		q.path |= QPATH(0, sf->image << 8);
		break;
	case Qimageurl:
		name = "url";
		q.path |= QPATH(0, sf->image << 8);
		break;
	case Qimagefile:
		name = imagename(sf);
		freename = 1;
		q.path |= QPATH(0, sf->image << 8);
		length = imagelength(sf);
		break;
	default:
		name = "?egreg";
		q.path = 0;
		break;
	}

	if(name == nil){
		name = "???";
		freename = 0;
	}

	if(qid)
		*qid = q;
	if(dir){
		memset(dir, 0, sizeof *dir);
		dir->name = estrdup9p(name);
		dir->muid = estrdup9p("muid");
		mode |= q.type<<24;
		if(mode & DMDIR)
			mode |= 0755;
		dir->mode = mode;
		dir->uid = estrdup9p(uid);
		dir->gid = estrdup9p("smugfs");
		dir->qid = q;
		dir->length = length;
	}
	if(freename)
		free(name);
}

static char*
xwalk1(Fid *fid, char *name, Qid *qid)
{
	int dotdot, i;
	vlong id;
	char *key;
	SmugFid *sf;
	char *x;
	Upload *u;

	dotdot = strcmp(name, "..") == 0;
	sf = fid->aux;
	switch(sf->type){
	default:
	NotFound:
		return "file not found";

	case Qroot:
		if(dotdot)
			break;
		if(strcmp(name, "ctl") == 0){
			sf->type = Qctl;
			break;
		}
		if(strcmp(name, "uploads") == 0){
			sf->type = Quploads;
			break;
		}
		if(strcmp(name, "rpclog") == 0){
			sf->type = Qrpclog;
			break;
		}
		if((i = nickindex(name)) >= 0){
			sf->nickid = i;
			sf->type = Qnick;
			break;
		}
		goto NotFound;

	case Qnick:
		if(dotdot){
			sf->type = Qroot;
			sf->nickid = 0;
			break;
		}
		if(strcmp(name, "ctl") == 0){
			sf->type = Qnickctl;
			break;
		}
		if(strcmp(name, "albums") == 0){
			sf->category = -1;
			sf->type = Qalbums;
			break;
		}
		if((id = getcategoryid(sf, name)) >= 0){
			sf->category = id;
			sf->type = Qcategory;
			break;
		}
		goto NotFound;

	case Qalbums:
	case Qcategory:
		if(dotdot){
			sf->category = 0;
			sf->type = Qnick;
			break;
		}
		if(strcmp(name, "ctl") == 0){
			sf->type++;
			break;
		}
		if((id = getalbumid(sf, name, &key)) >= 0){
			sf->album = id;
			sf->albumkey = key;
			sf->type = Qalbum;
			break;
		}
		goto NotFound;

	case Qalbum:
		if(dotdot){
			free(sf->albumkey);
			sf->albumkey = nil;
			sf->album = 0;
			if(sf->category == -1)
				sf->type = Qalbums;
			else
				sf->type = Qcategory;
			break;
		}
		if(strcmp(name, "ctl") == 0){
			sf->type = Qalbumctl;
			break;
		}
		if(strcmp(name, "settings") == 0){
			sf->type = Qalbumsettings;
			break;
		}
		if((id = getimageid(sf, name, &key)) >= 0){
			sf->image = id;
			sf->imagekey = key;
			sf->type = Qimage;
			break;
		}
		if((u = getuploadname(sf, name)) != nil){
			sf->upload = u;
			sf->type = Quploadfile;
			break;
		}
		goto NotFound;

	case Qimage:
		if(dotdot){
			free(sf->imagekey);
			sf->imagekey = nil;
			sf->image = 0;
			sf->type = Qalbum;
			break;
		}
		if(strcmp(name, "ctl") == 0){
			sf->type = Qimagectl;
			break;
		}
		if(strcmp(name, "url") == 0){
			sf->type = Qimageurl;
			break;
		}
		if(strcmp(name, "settings") == 0){
			sf->type = Qimagesettings;
			break;
		}
		if(strcmp(name, "exif") == 0){
			sf->type = Qimageexif;
			break;
		}
		x = imagename(sf);
		if(x && strcmp(name, x) == 0){
			free(x);
			sf->type = Qimagefile;
			break;
		}
		free(x);
		goto NotFound;
	}
	dostat(sf, qid, nil);
	fid->qid = *qid;
	return nil;
}

static int
dodirgen(int i, Dir *d, void *v)
{
	SmugFid *sf, xsf;
	char *key;
	vlong id;
	Upload *u;

	sf = v;
	xsf = *sf;
	if(i-- == 0){
		xsf.type++;	// ctl in every directory
		dostat(&xsf, nil, d);
		return 0;
	}

	switch(sf->type){
	default:
		return -1;

	case Qroot:
		if(i-- == 0){
			xsf.type = Qrpclog;
			dostat(&xsf, nil, d);
			return 0;
		}
		if(i < 0 || i >= nnick)
			return -1;
		xsf.type = Qnick;
		xsf.nickid = i;
		dostat(&xsf, nil, d);
		return 0;

	case Qnick:
		if(i-- == 0){
			xsf.type = Qalbums;
			dostat(&xsf, nil, d);
			return 0;
		}
		if((id = getcategoryindex(sf, i)) < 0)
			return -1;
		xsf.type = Qcategory;
		xsf.category = id;
		dostat(&xsf, nil, d);
		return 0;

	case Qalbums:
	case Qcategory:
		if((id = getalbumindex(sf, i, &key)) < 0)
			return -1;
		xsf.type = Qalbum;
		xsf.album = id;
		xsf.albumkey = key;
		dostat(&xsf, nil, d);
		free(key);
		return 0;

	case Qalbum:
		if(i-- == 0){
			xsf.type = Qalbumsettings;
			dostat(&xsf, nil, d);
			return 0;
		}
		if((u = getuploadindex(sf, &i)) != nil){
			xsf.upload = u;
			xsf.type = Quploadfile;
			dostat(&xsf, nil, d);
			closeupload(u);
			return 0;
		}
		if((id = getimageindex(sf, i, &key)) < 0)
			return -1;
		xsf.type = Qimage;
		xsf.image = id;
		xsf.imagekey = key;
		dostat(&xsf, nil, d);
		free(key);
		return 0;

	case Qimage:
		if(i-- == 0){
			xsf.type = Qimagefile;
			dostat(&xsf, nil, d);
			return 0;
		}
		if(i-- == 0){
			xsf.type = Qimageexif;
			dostat(&xsf, nil, d);
			return 0;
		}
		if(i-- == 0){
			xsf.type = Qimagesettings;
			dostat(&xsf, nil, d);
			return 0;
		}
		if(i-- == 0){
			xsf.type = Qimageurl;
			dostat(&xsf, nil, d);
			return 0;
		}
		return -1;
	}
}

static void
xstat(Req *r)
{
	dostat(r->fid->aux, nil, &r->d);
	respond(r, nil);
}

static void
xwstat(Req *r)
{
	SmugFid *sf;
	Json *v;
	char *s;
	char strid[50];

	sf = r->fid->aux;
	if(r->d.uid[0] || r->d.gid[0] || r->d.muid[0] || ~r->d.mode != 0
	|| ~r->d.atime != 0 || ~r->d.mtime != 0 || ~r->d.length != 0){
		respond(r, "invalid wstat");
		return;
	}
	if(r->d.name[0]){
		switch(sf->type){
		default:
			respond(r, "invalid wstat");
			return;
		// TODO: rename category
		case Qalbum:
			snprint(strid, sizeof strid, "%lld", sf->album);
			v = ncsmug("smugmug.albums.changeSettings",
				"AlbumID", strid, "Title", r->d.name, nil);
			if(v == nil)
				responderrstr(r);
			else
				respond(r, nil);
			s = smprint("&AlbumID=%lld&", sf->album);
			jcacheflush(s);
			free(s);
			jcacheflush("smugmug.albums.get&");
			return;
		}
	}
	respond(r, "invalid wstat");
}

static void
xattach(Req *r)
{
	SmugFid *sf;

	sf = emalloc(sizeof *sf);
	r->fid->aux = sf;
	sf->type = Qroot;
	dostat(sf, &r->ofcall.qid, nil);
	r->fid->qid = r->ofcall.qid;
	respond(r, nil);
}

void
xopen(Req *r)
{
	SmugFid *sf;

	if((r->ifcall.mode&~OTRUNC) > 2){
		respond(r, "permission denied");
		return;
	}

	sf = r->fid->aux;
	switch(sf->type){
	case Qctl:
	case Qnickctl:
	case Qalbumsctl:
	case Qcategoryctl:
	case Qalbumctl:
	case Qimagectl:
	case Qalbumsettings:
	case Qimagesettings:
		break;

	case Quploadfile:
		if(r->ifcall.mode != OREAD){
			lock(&sf->upload->lk);
			if(sf->upload->ready){
				unlock(&sf->upload->lk);
				respond(r, "permission denied");
				return;
			}
			sf->upwriter = 1;
			sf->upload->nwriters++;
			unlock(&sf->upload->lk);
		}
		break;

	default:
		if(r->ifcall.mode != OREAD){
			respond(r, "permission denied");
			return;
		}
		break;
	}

	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

void
xcreate(Req *r)
{
	SmugFid *sf;
	Json *v;
	vlong id;
	char strid[50], *key;
	Upload *u;

	sf = r->fid->aux;
	switch(sf->type){
	case Qnick:
		// Create new category.
		if(!(r->ifcall.perm&DMDIR))
			break;
		v = ncsmug("smugmug.categories.create",
			"Name", r->ifcall.name, nil);
		if(v == nil){
			responderrstr(r);
			return;
		}
		id = jint(jwalk(v, "Category/id"));
		if(id < 0){
			fprint(2, "Create category: %J\n", v);
			jclose(v);
			responderrstr(r);
			return;
		}
		sf->type = Qcategory;
		sf->category = id;
		jcacheflush("method=smugmug.users.getTree&");
		jcacheflush("method=smugmug.categories.get&");
		dostat(sf, &r->ofcall.qid, nil);
		respond(r, nil);
		return;

	case Qcategory:
		// Create new album.
		if(!(r->ifcall.perm&DMDIR))
			break;
		snprint(strid, sizeof strid, "%lld", sf->category);
		// Start with most restrictive settings.
		v = ncsmug("smugmug.albums.create",
			"Title", r->ifcall.name,
			"CategoryID", strid,
			"Public", "0",
			"WorldSearchable", "0",
			"SmugSearchable", "0",
			nil);
		if(v == nil){
			responderrstr(r);
			return;
		}
		id = jint(jwalk(v, "Album/id"));
		key = jstring(jwalk(v, "Album/Key"));
		if(id < 0 || key == nil){
			fprint(2, "Create album: %J\n", v);
			jclose(v);
			responderrstr(r);
			return;
		}
		sf->type = Qalbum;
		sf->album = id;
		sf->albumkey = estrdup(key);
		jclose(v);
		jcacheflush("method=smugmug.users.getTree&");
		dostat(sf, &r->ofcall.qid, nil);
		respond(r, nil);
		return;

	case Qalbum:
		// Upload image to album.
		if(r->ifcall.perm&DMDIR)
			break;
		u = newupload(sf, r->ifcall.name);
		if(u == nil){
			responderrstr(r);
			return;
		}
		sf->upload = u;
		sf->upwriter = 1;
		sf->type = Quploadfile;
		dostat(sf, &r->ofcall.qid, nil);
		respond(r, nil);
		return;
	}
	respond(r, "permission denied");
}

static int
writetofd(Req *r, int fd)
{
	int total, n;

	total = 0;
	while(total < r->ifcall.count){
		n = pwrite(fd, (char*)r->ifcall.data+total, r->ifcall.count-total, r->ifcall.offset+total);
		if(n <= 0)
			return -1;
		total += n;
	}
	r->ofcall.count = r->ifcall.count;
	return 0;
}

static void
readfromfd(Req *r, int fd)
{
	int n;
	n = pread(fd, r->ofcall.data, r->ifcall.count, r->ifcall.offset);
	if(n < 0)
		n = 0;
	r->ofcall.count = n;
}

void
xread(Req *r)
{
	SmugFid *sf;
	char *data;
	int fd;
	HTTPHeader hdr;
	char *url;

	sf = r->fid->aux;
	r->ofcall.count = 0;
	switch(sf->type){
	default:
		respond(r, "not implemented");
		return;
	case Qroot:
	case Qnick:
	case Qalbums:
	case Qcategory:
	case Qalbum:
	case Qimage:
		dirread9p(r, dodirgen, sf);
		break;
	case Qrpclog:
		rpclogread(r);
		return;
	case Qctl:
	case Qnickctl:
	case Qalbumsctl:
	case Qcategoryctl:
	case Qalbumctl:
	case Qimagectl:
	case Qimageurl:
	case Qimageexif:
	case Quploads:
	case Qimagesettings:
	case Qalbumsettings:
		data = readctl(sf);
		readstr(r, data);
		free(data);
		break;
	case Qimagefile:
		url = imageurl(sf);
		if(url == nil || (fd = download(url, &hdr)) < 0){
			free(url);
			responderrstr(r);
			return;
		}
		readfromfd(r, fd);
		free(url);
		close(fd);
		break;
	case Quploadfile:
		if(sf->upload)
			readfromfd(r, sf->upload->fd);
		break;
	}
	respond(r, nil);
}

void
xwrite(Req *r)
{
	int sync;
	char *s, *t, *p;
	Json *v;
	char strid[50];
	SmugFid *sf;

	sf = r->fid->aux;
	r->ofcall.count = r->ifcall.count;
	sync = (r->ifcall.count==4 && memcmp(r->ifcall.data, "sync", 4) == 0);
	switch(sf->type){
	case Qctl:
		if(sync){
			jcacheflush(nil);
			respond(r, nil);
			return;
		}
		break;
	case Qnickctl:
		if(sync){
			s = smprint("&NickName=%s&", nickname(sf->nickid));
			jcacheflush(s);
			free(s);
			respond(r, nil);
			return;
		}
		break;
	case Qalbumsctl:
	case Qcategoryctl:
		jcacheflush("smugmug.categories.get");
		break;
	case Qalbumctl:
		if(sync){
			s = smprint("&AlbumID=%lld&", sf->album);
			jcacheflush(s);
			free(s);
			respond(r, nil);
			return;
		}
		break;
	case Qimagectl:
		if(sync){
			s = smprint("&ImageID=%lld&", sf->image);
			jcacheflush(s);
			free(s);
			respond(r, nil);
			return;
		}
		break;
	case Quploadfile:
		if(sf->upload){
			if(writetofd(r, sf->upload->fd) < 0){
				responderrstr(r);
				return;
			}
			respond(r, nil);
			return;
		}
		break;
	case Qimagesettings:
	case Qalbumsettings:
		s = (char*)r->ifcall.data;	// lib9p nul-terminated it
		t = strpbrk(s, " \r\t\n");
		if(t == nil)
			t = "";
		else{
			*t++ = 0;
			while(*t == ' ' || *t == '\r' || *t == '\t' || *t == '\n')
				t++;
		}
		p = strchr(t, '\n');
		if(p && p[1] == 0)
			*p = 0;
		else if(p){
			respond(r, "newline in argument");
			return;
		}
		if(sf->type == Qalbumsettings)
			goto Albumsettings;
		snprint(strid, sizeof strid, "%lld", sf->image);
		v = ncsmug("smugmug.images.changeSettings",
			"ImageID", strid,
			s, t, nil);
		if(v == nil)
			responderrstr(r);
		else
			respond(r, nil);
		s = smprint("&ImageID=%lld&", sf->image);
		jcacheflush(s);
		free(s);
		return;
	Albumsettings:
		snprint(strid, sizeof strid, "%lld", sf->album);
		v = ncsmug("smugmug.albums.changeSettings",
			"AlbumID", strid, s, t, nil);
		if(v == nil)
			responderrstr(r);
		else
			respond(r, nil);
		s = smprint("&AlbumID=%lld&", sf->album);
		jcacheflush(s);
		free(s);
		return;
	}
	respond(r, "invalid control message");
	return;
}

void
xremove(Req *r)
{
	char id[100];
	SmugFid *sf;
	Json *v;

	sf = r->fid->aux;
	switch(sf->type){
	default:
		respond(r, "permission denied");
		return;
	case Qcategoryctl:
	case Qalbumctl:
	case Qalbumsettings:
	case Qimagectl:
	case Qimagesettings:
	case Qimageexif:
	case Qimageurl:
	case Qimagefile:
		/* ignore remove request, but no error, so rm -r works */
		/* you can pretend they get removed and immediately grow back! */
		respond(r, nil);
		return;
	case Qcategory:
		v = getalbums(sf);
		if(v && v->len > 0){
			respond(r, "directory not empty");
			return;
		}
		snprint(id, sizeof id, "%lld", sf->category);
		v = ncsmug("smugmug.categories.delete",
			"CategoryID", id, nil);
		if(v == nil)
			responderrstr(r);
		else{
			jclose(v);
			jcacheflush("smugmug.users.getTree");
			jcacheflush("smugmug.categories.get");
			respond(r, nil);
		}
		return;
	case Qalbum:
		v = getimages(sf, sf->album, sf->albumkey);
		if(v && v->len > 0){
			respond(r, "directory not empty");
			return;
		}
		snprint(id, sizeof id, "%lld", sf->album);
		v = ncsmug("smugmug.albums.delete",
			"AlbumID", id, nil);
		if(v == nil)
			responderrstr(r);
		else{
			jclose(v);
			jcacheflush("smugmug.users.getTree");
			jcacheflush("smugmug.categories.get");
			jcacheflush("smugmug.albums.get");
			respond(r, nil);
		}
		return;

	case Qimage:
		snprint(id, sizeof id, "%lld", sf->image);
		v = ncsmug("smugmug.images.delete",
			"ImageID", id, nil);
		if(v == nil)
			responderrstr(r);
		else{
			jclose(v);
			snprint(id, sizeof id, "ImageID=%lld&", sf->image);
			jcacheflush(id);
			jcacheflush("smugmug.images.get&");
			respond(r, nil);
		}
		return;
	}
}

void
xflush(Req *r)
{
	rpclogflush(r->oldreq);
	respond(r, nil);
}

Srv xsrv;

void
xinit(void)
{
	xsrv.attach = xattach;
	xsrv.open = xopen;
	xsrv.create = xcreate;
	xsrv.read = xread;
	xsrv.stat = xstat;
	xsrv.walk1 = xwalk1;
	xsrv.clone = xclone;
	xsrv.destroyfid = xdestroyfid;
	xsrv.remove = xremove;
	xsrv.write = xwrite;
	xsrv.flush = xflush;
	xsrv.wstat = xwstat;
}
