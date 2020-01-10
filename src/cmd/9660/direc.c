#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>

#include "iso9660.h"

void
mkdirec(Direc *direc, XDir *d)
{
	memset(direc, 0, sizeof(Direc));
	direc->name = atom(d->name);
	direc->uid = atom(d->uid);
	direc->gid = atom(d->gid);
	direc->uidno = d->uidno;
	direc->gidno = d->gidno;
	direc->mode = d->mode;
	direc->length = d->length;
	direc->mtime = d->mtime;
	direc->atime = d->atime;
	direc->ctime = d->ctime;
	direc->symlink = d->symlink;
}

static int
strecmp(char *a, char *ea, char *b)
{
	int r;

	if((r = strncmp(a, b, ea-a)) != 0)
		return r;

	if(b[ea-a] == '\0')
		return 0;
	return 1;
}

/*
 * Binary search a list of directories for the
 * entry with name name.
 * If no entry is found, return a pointer to
 * where a new such entry would go.
 */
static Direc*
dbsearch(char *name, int nname, Direc *d, int n)
{
	int i;

	while(n > 0) {
		i = strecmp(name, name+nname, d[n/2].name);
		if(i < 0)
			n = n/2;
		else if(i > 0) {
			d += n/2+1;
			n -= (n/2+1);
		} else
			return &d[n/2];
	}
	return d;
}

/*
 * Walk to name, starting at d.
 */
Direc*
walkdirec(Direc *d, char *name)
{
	char *p, *nextp, *slashp;
	Direc *nd;

	for(p=name; p && *p; p=nextp) {
		if((slashp = strchr(p, '/')) != nil)
			nextp = slashp+1;
		else
			nextp = slashp = p+strlen(p);

		nd = dbsearch(p, slashp-p, d->child, d->nchild);
		if(nd >= d->child+d->nchild || strecmp(p, slashp, nd->name) != 0)
			return nil;
		d = nd;
	}
	return d;
}

/*
 * Add the file ``name'' with attributes d to the
 * directory ``root''.  Name may contain multiple
 * elements; all but the last must exist already.
 *
 * The child lists are kept sorted by utfname.
 */
Direc*
adddirec(Direc *root, char *name, XDir *d)
{
	char *p;
	Direc *nd;
	int off;

	if(name[0] == '/')
		name++;
	if((p = strrchr(name, '/')) != nil) {
		*p = '\0';
		root = walkdirec(root, name);
		if(root == nil) {
			sysfatal("error in proto file: no entry for /%s but /%s/%s\n", name, name, p+1);
			return nil;
		}
		*p = '/';
		p++;
	} else
		p = name;

	nd = dbsearch(p, strlen(p), root->child, root->nchild);
	off = nd - root->child;
	if(off < root->nchild && strcmp(nd->name, p) == 0) {
		if ((d->mode & DMDIR) == 0)
			fprint(2, "warning: proto lists %s twice\n", name);
		return nil;
	}

	if(root->nchild%Ndirblock == 0) {
		root->child = erealloc(root->child, (root->nchild+Ndirblock)*sizeof(Direc));
		nd = root->child + off;
	}

	memmove(nd+1, nd, (root->nchild - off)*sizeof(Direc));
	mkdirec(nd, d);
	nd->name = atom(p);
	root->nchild++;
	return nd;
}

/*
 * Copy the tree src into dst.
 */
void
copydirec(Direc *dst, Direc *src)
{
	int i, n;

	*dst = *src;

	if((src->mode & DMDIR) == 0)
		return;

	n = (src->nchild + Ndirblock - 1);
	n -= n%Ndirblock;
	dst->child = emalloc(n*sizeof(Direc));

	n = dst->nchild;
	for(i=0; i<n; i++)
		copydirec(&dst->child[i], &src->child[i]);
}

/*
 * Turn the Dbadname flag on for any entries
 * that have non-conforming names.
 */
static void
_checknames(Direc *d, int (*isbadname)(char*), int isroot)
{
	int i;

	if(!isroot && isbadname(d->name))
		d->flags |= Dbadname;

	if(strcmp(d->name, "_conform.map") == 0)
		d->flags |= Dbadname;

	for(i=0; i<d->nchild; i++)
		_checknames(&d->child[i], isbadname, 0);
}

void
checknames(Direc *d, int (*isbadname)(char*))
{
	_checknames(d, isbadname, 1);
}

/*
 * Set the names to conform to 8.3
 * by changing them to numbers.
 * Plan 9 gets the right names from its
 * own directory entry.
 *
 * We used to write a _conform.map file to translate
 * names.  Joliet should take care of most of the
 * interoperability with other systems now.
 */
void
convertnames(Direc *d, char* (*cvt)(char*, char*))
{
	int i;
	char new[1024];

	if(d->flags & Dbadname)
		cvt(new, conform(d->name, d->mode & DMDIR));
	else
		cvt(new, d->name);
	d->confname = atom(new);

	for(i=0; i<d->nchild; i++)
		convertnames(&d->child[i], cvt);
}

/*
 * Sort a directory with a given comparison function.
 * After this is called on a tree, adddirec should not be,
 * since the entries may no longer be sorted as adddirec expects.
 */
void
dsort(Direc *d, int (*cmp)(const void*, const void*))
{
	int i, n;

	n = d->nchild;
	qsort(d->child, n, sizeof(d[0]), cmp);

	for(i=0; i<n; i++)
		dsort(&d->child[i], cmp);
}
