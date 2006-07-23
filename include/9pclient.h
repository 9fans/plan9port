#ifndef _9PCLIENT_H_
#define _9PCLIENT_H_ 1
#ifdef __cplusplus
extern "C" {
#endif

AUTOLIB(9pclient)
/*
 * Simple user-level 9P client.
 */

typedef struct CFsys CFsys;
typedef struct CFid CFid;

CFsys *fsinit(int);
CFsys *fsmount(int, char*);

int fsversion(CFsys*, int, char*, int);
CFid *fsauth(CFsys*, char*, char*);
CFid *fsattach(CFsys*, CFid*, char*, char*);
CFid *fsopen(CFsys*, char*, int);
int fsopenfd(CFsys*, char*, int);
long fsread(CFid*, void*, long);
long fsreadn(CFid*, void*, long);
long fspread(CFid*, void*, long, vlong);
long fspwrite(CFid*, void*, long, vlong);
vlong fsseek(CFid*, vlong, int);
long fswrite(CFid*, void*, long);
void fsclose(CFid*);
void fsunmount(CFsys*);
void _fsunmount(CFsys*);	/* do not close fd */
struct Dir;	/* in case there's no lib9.h */
long fsdirread(CFid*, struct Dir**);
long fsdirreadall(CFid*, struct Dir**);
struct Dir *fsdirstat(CFsys*, char*);
struct Dir *fsdirfstat(CFid*);
int fsdirwstat(CFsys*, char*, struct Dir*);
int fsdirfwstat(CFid*, struct Dir*);
CFid *fsroot(CFsys*);
void fssetroot(CFsys*, CFid*);
CFsys *nsinit(char*);
CFsys *nsmount(char*, char*);
CFid *nsopen(char*, char*, char*, int);
int	fsfremove(CFid*);
int	fsremove(CFsys*, char*);
CFid *fscreate(CFsys*, char*, int, ulong);
int fsaccess(CFsys*, char*, int);
int	fsvprint(CFid*, char*, va_list);
int	fsprint(CFid*, char*, ...);
Qid	fsqid(CFid*);

/* manipulate unopened fids */
CFid	*fswalk(CFid*, char*);
int fsfopen(CFid*, int);
int fsfcreate(CFid*, char*, int, ulong);

extern int chatty9pclient;
extern int eofkill9pclient;

#ifdef __cplusplus
}
#endif
#endif
