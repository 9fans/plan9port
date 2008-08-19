#include <u.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9pclient.h>

#if defined(__APPLE__)
#define __FreeBSD__ 10
#endif

#include "fuse_kernel.h"

/* Somehow the FUSE guys forgot to define this one! */
struct fuse_create_out {
	struct fuse_entry_out e;
	struct fuse_open_out o;
};

typedef struct FuseMsg FuseMsg;
struct FuseMsg
{
	FuseMsg *next;
	uchar *buf;
	int nbuf;
	struct fuse_in_header *hdr;	/* = buf */
	void *tx;	/* = hdr+1 */
};

extern int debug;

extern int fusefd;
extern int fuseeof;
extern int fusebufsize;
extern int fusemaxwrite;
extern FuseMsg *fusemsglist;
extern char *fusemtpt;

void		freefusemsg(FuseMsg *m);
int		fusefmt(Fmt*);
void		initfuse(char *mtpt);
void	waitfuse(void);
FuseMsg*	readfusemsg(void);
void		replyfuse(FuseMsg *m, void *arg, int narg);
void		replyfuseerrno(FuseMsg *m, int e);
void		replyfuseerrstr(FuseMsg*);
void		request9p(Fcall *tx);

void*		emalloc(uint n);
void*		erealloc(void *p, uint n);
char*		estrdup(char *p);

int		errstr2errno(void);
void unmountatexit(void);
