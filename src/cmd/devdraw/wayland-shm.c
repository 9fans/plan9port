#define _POSIX_C_SOURCE 200112L
#define _DEFAULT_SOURCE
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/memfd.h>

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

// See also: https://github.com/a-darwish/memfd-examples
#ifndef F_LINUX_SPECIFIC_BASE
#define F_LINUX_SPECIFIC_BASE 1024
#endif

#ifndef F_ADD_SEALS
#define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)
#define F_GET_SEALS (F_LINUX_SPECIFIC_BASE + 10)

#define F_SEAL_SEAL 0x0001   /* prevent further seals from being set */
#define F_SEAL_SHRINK 0x0002 /* prevent file from shrinking */
#define F_SEAL_GROW 0x0004   /* prevent file from growing */
#define F_SEAL_WRITE 0x0008  /* prevent writes */
#endif

static int
memfd_create(const char *name, unsigned int flags) {
	return syscall(__NR_memfd_create, name, flags);
};

/*
Using memfds instead of shm files makes this all a lot simpler, even including
the definitions above.

Memfd(2) is basically, "what if malloc(3) returned a file descriptor?"
*/

int
allocate_shm_file(size_t size) {
	int fd = memfd_create("devdraw", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL);
	return fd;
}
