#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/stat.h>

extern int _p9dir(struct stat*, struct stat*, char*, Dir*, char**, char*);

Dir*
dirstat(char *file)
{
	struct stat lst;
	struct stat st;
	int nstr;
	Dir *d;
	char *str;

	if(lstat(file, &lst) < 0)
		return nil;
	st = lst;
	if((lst.st_mode&S_IFMT) == S_IFLNK)
		stat(file, &st);

	nstr = _p9dir(&lst, &st, file, nil, nil, nil);
	d = mallocz(sizeof(Dir)+nstr, 1);
	if(d == nil)
		return nil;
	str = (char*)&d[1];
	_p9dir(&lst, &st, file, d, &str, str+nstr);
	return d;
}

