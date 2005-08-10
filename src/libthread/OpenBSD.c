#include "threadimpl.h"
#include "BSD.c"

#include <dlfcn.h>

#define MIN(a,b)	((a) < (b) ? (a) : (b))
#define MAX(a,b)	((a) > (b) ? (a) : (b))

enum {
	FD_READ = 1,
	FD_WRITE,
	FD_RDWR,
	FDTBL_MAXSIZE = 128
};

struct fd_entry {
	QLock	lock;
	int	fd;
	int	id;
	short	rc;
	short	wc;
	short	ref;
};

static struct fd_entry fd_entry1 = { .fd -1 };
static struct fd_entry *fd_table = nil;
static spinlock_t fd_table_lock = { 0, 0, nil, 0 };
static spinlock_t mlock = { 0, 0, nil, 0 };
static spinlock_t dl_lock = { 0, 0, nil, 0 };

void
_thread_malloc_lock(void)
{
	_spinlock(&mlock);
}

void
_thread_malloc_unlock(void)
{
	_spinunlock(&mlock);
}

void
_thread_malloc_init(void)
{
}

/*
 * Must set errno on failure because the return value
 * of _thread_fd_entry is propagated back to the caller
 * of the thread-wrapped libc function.
 */
static struct fd_entry *
_thread_fd_lookup(int fd)
{
	struct fd_entry *t;
	static int cursize;
	int newsize;

	if(fd >= FDTBL_MAXSIZE) {
		errno = EBADF;
		return nil;
	}

	/*
	 * There are currently only a few libc functions using 
	 * _thread_fd_*, which are rarely called by P9P programs.
	 * So the contention for these locks is very small and so
	 * far have usually been limited to a single fd. So
	 * rather than malloc the fd_table everytime we just use
	 * a single fd_entry until a lock request for another fd
	 * comes in.
	 */
	if(fd_table == nil)
		if(fd_entry1.fd == -1) {
			fd_entry1.fd = fd;
			return &fd_entry1;
		} else if(fd_entry1.fd == fd)
			return &fd_entry1;
		else {
			cursize = MAX(fd_entry1.fd, 16);
			fd_table = malloc(cursize*sizeof(fd_table[0]));
			if(fd_table == nil) {
				errno = ENOMEM;
				return nil;
			}
			memset(fd_table, 0, cursize*sizeof(fd_table[0]));
			fd_table[fd_entry1.fd] = fd_entry1;
		}
	if(fd > cursize) {
		newsize = MIN(cursize*2, FDTBL_MAXSIZE);
		t = realloc(fd_table, newsize*sizeof(fd_table[0]));
		if(t == nil) {
			errno = ENOMEM;
			return nil;
		}
		fd_table = t;
		cursize = newsize; 
		memset(fd_table, 0, cursize*sizeof(fd_table[0]));
	}

	return &fd_table[fd];
}

/*
 * Mutiple readers just share the lock by incrementing the read count.
 * Writers must obtain an exclusive lock.
 */
int
_thread_fd_lock(int fd, int type, struct timespec *time)
{
	struct fd_entry *fde;
	int id;

	_spinlock(&fd_table_lock);
	fde = _thread_fd_lookup(fd);
	if(fde == nil)
		return -1;

	if(type == FD_READ) {
		if(fde->rc++ >= 1 && fde->wc == 0) {
			_spinunlock(&fd_table_lock);
			return 0;
		}
	} else
		fde->wc++;
	_spinunlock(&fd_table_lock);

	/* handle recursion */
	id = proc()->osprocid;
	if(id == fde->id) {
		fde->ref++;
		return 0;
	}

	qlock(&fde->lock);
	fde->id = id;
	return 0;
}

void
_thread_fd_unlock(int fd, int type)
{
	struct fd_entry *fde;
	int id;

	fde = _thread_fd_lookup(fd);
	if(fde == nil) {
		fprint(2, "_thread_fd_unlock: fd %d not in table!\n", fd);
		return;
	}

	if(type == FD_READ && --fde->rc >= 1)
		return;
	else
		fde->wc--;

	id = proc()->osprocid;
	if(id == fde->id && fde->ref > 0) {
		fde->ref--;
		return;
	}
	fde->id = 0;
	qunlock(&fde->lock);
}

void
_thread_dl_lock(int t)
{
	if(t)
		_spinunlock(&dl_lock);
	else
		_spinlock(&dl_lock);
}

void
_pthreadinit(void)
{
	__isthreaded = 1;
	dlctl(nil, DL_SETTHREADLCK, _thread_dl_lock);
	signal(SIGUSR2, sigusr2handler);
}
