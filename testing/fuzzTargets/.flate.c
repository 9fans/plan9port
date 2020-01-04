#include <u.h>
#include <libc.h>
#include <flate.h>

typedef struct {
	uint8_t *arr;
	size_t cap;
} slice;

static
int
get(void *s) {
	slice *sl = (slice *)s;
	if (sl->cap == 0) {
		return -1;
	}
	sl->cap--;
	sl->arr++;
	return sl->arr[-1];
}

static
int
ignore(void *unused0, void *unused1, int size) {
	return size;
}
