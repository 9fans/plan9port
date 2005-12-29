#include "threadimpl.h"
#include "BSD.c"

#include <dlfcn.h>

struct thread_tag {
	struct thread_tag	*next;
	spinlock_t		l;
	volatile int		key;
	void			*data;
};

static spinlock_t mlock;
static spinlock_t dl_lock;
static spinlock_t tag_lock;
static struct thread_tag *thread_tag_store = nil;
static uint nextkey = 0;

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
 * for ld.so
 */
void
_thread_dl_lock(int t)
{
	if(t)
		_spinunlock(&dl_lock);
	else
		_spinlock(&dl_lock);
}

/*
 * for libc
 */
static void
_thread_tag_init(void **tag)
{
	struct thread_tag *t;

	_spinlock(&tag_lock);
	if(*tag == nil) {
		t = malloc(sizeof (*t));
		if(t != nil) {
			memset(&t->l, 0, sizeof(t->l));
			t->key = nextkey++;
			*tag = t;
		}
	}
	_spinunlock(&tag_lock);
}

void
_thread_tag_lock(void **tag)
{
	struct thread_tag *t;

	if(*tag == nil)
		_thread_tag_init(tag);
	t = *tag;
	_spinlock(&t->l);
}

void
_thread_tag_unlock(void **tag)
{
	struct thread_tag *t;

	if(*tag == nil)
		_thread_tag_init(tag);
	t = *tag;
	_spinunlock(&t->l);
}

static void *
_thread_tag_insert(struct thread_tag *t, void *v)
{
	t->data = v;
	t->next = thread_tag_store;
	thread_tag_store = t;
	return t;
}

static void *
_thread_tag_lookup(struct thread_tag *tag, int size)
{
	struct thread_tag *t;
	void *p;

	_spinlock(&tag->l);
	for(t = thread_tag_store; t != nil; t = t->next)
		if(t->key == tag->key)
			break;
	if(t == nil) {
		p = malloc(size);
		if(p == nil) {
			_spinunlock(&tag->l);
			return nil;
		}
		_thread_tag_insert(tag, p);
	}
	_spinunlock(&tag->l);
	return tag->data;
}

void *
_thread_tag_storage(void **tag, void *storage, size_t n, void *err)
{
	struct thread_tag *t;
	void *r;

	if(*tag == nil)
		_thread_tag_init(tag);
	t = *tag;

	r = _thread_tag_lookup(t, n);
	if(r == nil)
		r = err;
	else
		memcpy(r, storage, n);
	return r;
}

void
_pthreadinit(void)
{
	__isthreaded = 1;
	dlctl(nil, DL_SETTHREADLCK, _thread_dl_lock);
	signal(SIGUSR2, sigusr2handler);
}
