#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include <stdio.h>
#include <sys/time.h>
#include <utime.h>
#include <sys/stat.h>

int
dirwstat(char *file, Dir *dir)
{
	int ret;
	char *newfile, *p;
	struct utimbuf ub;

	/* BUG handle more (e.g. uid, gid, atime) */
	ret = 0;
	if(~dir->mode != 0){
		if(chmod(file, dir->mode) < 0)
			ret = -1;
	}
	if(~dir->mtime != 0){
		ub.actime = dir->mtime;
		ub.modtime = dir->mtime;
		if(utime(file, &ub) < 0)
			ret = -1;
	}
	if(~dir->length != 0){
		if(truncate(file, dir->length) < 0)
			ret = -1;
	}
	// Note: rename after all other operations so we can use
	// the original name in all other calls.
	if(dir->name != nil && dir->name[0] != '\0'){
		newfile = nil;
		if((p = strrchr(file, '/')) != nil){
			newfile = malloc((p-file) + 1 + strlen(dir->name) + 1);
			if(newfile == nil)
				return -1;
			memmove(newfile, file, p-file+1);
			strcpy(newfile+(p-file+1), dir->name);
		}
		if(rename(file, newfile != nil ? newfile : dir->name) < 0)
			ret = -1;
		free(newfile);
	}
	return ret;
}
