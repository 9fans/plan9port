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
	{ "permitted",		EPERM,		0 },
	{ "permission",		EACCES,		0 },
	{ "access",		EACCES,		0 },
	{ "exists",		EEXIST,		0 },
	{ "exist",		ENOENT,		0 },
	{ "no such",		ENOENT,		0 },
	{ "not found",		ENOENT,		0 },
	{ "input/output",	EIO,		0 },
	{ "timeout",		ETIMEDOUT,	0 },
	{ "timed out",		ETIMEDOUT,	0 },
	{ "i/o",		EIO,		0 },
	{ "too long",		E2BIG,		0 },
	{ "interrupt",		EINTR,		0 },
	{ "no such",		ENODEV,		0 },
	{ "bad file",		EBADF,		0 },
	{ " fid ",		EBADF,		0 },
	{ "temporar",		EAGAIN,		0 },
	{ "memory",		ENOMEM,		0 },
	{ "is a directory",	EISDIR,		0 },
	{ "directory",		ENOTDIR,	0 },
	{ "argument",		EINVAL,		0 },
	{ "pipe",		EPIPE,		0 },
	{ "in use",		EBUSY,		0 },
	{ "busy",		EBUSY,		0 },
	{ "illegal",		EINVAL,		0 },
	{ "invalid",		EINVAL,		0 },
	{ "read-only",		EROFS,		0 },
	{ "read only",		EROFS,		0 },
#ifdef EPROTO
	{ "proto",		EPROTO,		0 },
#else
	{ "proto",		EINVAL,		0 },
#endif
	{ "entry",		ENOENT,		0 },
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
