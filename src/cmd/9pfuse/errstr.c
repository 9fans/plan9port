#include "a.h"

enum
{
	EPLAN9 = 0x19283745	/* see /usr/local/plan9/src/lib9/errstr.c */
};

typedef struct Error Error;
struct Error
{
	char *text;
	int err;
	int len;
};

static Error errortab[] = {
	{ "permitted", EPERM },
	{ "permission", EACCES },
	{ "access", EACCES },
	{ "exists", EEXIST },
	{ "exist", ENOENT },
	{ "no such", ENOENT },
	{ "not found", ENOENT },
	{ "not implemented", ENOSYS},
	{ "input/output", EIO },
	{ "timeout", ETIMEDOUT },
	{ "timed out", ETIMEDOUT },
	{ "i/o", EIO },
	{ "too long", E2BIG },
	{ "interrupt", EINTR },
	{ "no such", ENODEV },
	{ "bad file", EBADF },
	{ " fid ", EBADF },
	{ "temporar", EAGAIN },
	{ "memory", ENOMEM },
	{ "is a directory", EISDIR },
	{ "directory", ENOTDIR },
	{ "argument", EINVAL },
	{ "pipe", EPIPE },
	{ "in use", EBUSY },
	{ "busy", EBUSY },
	{ "illegal", EINVAL },
	{ "invalid", EINVAL },
	{ "read-only", EROFS },
	{ "read only", EROFS },
	{ "stale ", ESTALE},
#ifdef EPROTO
	{ "proto", EPROTO },
#else
	{ "proto", EINVAL },
#endif
	{ "entry", ENOENT },
};

int
errstr2errno(void)
{
	char e[ERRMAX];
	int i, len;

	if(errno != EPLAN9)
		return errno;

	if(errortab[0].len == 0)
		for(i=0; i<nelem(errortab); i++)
			errortab[i].len = strlen(errortab[i].text);

	rerrstr(e, sizeof e);
	len = strlen(e);
	for(i=0; i<nelem(errortab); i++)
		if(errortab[i].len <= len && cistrstr(e, errortab[i].text))
			return errortab[i].err;
	return ERANGE;	/* who knows - be blatantly wrong */
}
