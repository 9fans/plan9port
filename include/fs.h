#ifndef _FS_H_
#define _FS_H_ 1
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Simple user-level 9P client.
 */

typedef struct Fsys Fsys;
typedef struct Fid Fid;

Fsys *fsinit(int);
Fsys *fsmount(int);

int fsversion(Fsys*, int, char*, int);
Fid *fsauth(Fsys*, char*);
Fid *fsattach(Fsys*, Fid*, char*, char*);
Fid *fsopen(Fsys*, char*, int);
int fsopenfd(Fsys*, char*, int);
long fsread(Fid*, void*, long);
long fsreadn(Fid*, void*, long);
long fswrite(Fid*, void*, long);
void fsclose(Fid*);
void fsunmount(Fsys*);
int fsrpc(Fsys*, Fcall*, Fcall*, void**);
Fid *fswalk(Fid*, char*);
struct Dir;	/* in case there's no lib9.h */
long fsdirread(Fid*, struct Dir**);
long fsdirreadall(Fid*, struct Dir**);
struct Dir *fsdirstat(Fsys*, char*);
struct Dir *fsdirfstat(Fid*);
int fsdirwstat(Fsys*, char*, struct Dir*);
int fsdirfwstat(Fid*, struct Dir*);
Fid *fsroot(Fsys*);

#ifdef __cplusplus
}
#endif
#endif
