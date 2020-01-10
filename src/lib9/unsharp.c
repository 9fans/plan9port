#include <u.h>
#include <libc.h>

/*
 * I don't want too many of these,
 * but the ones we have are just too useful.
 */
static struct {
	char *old;
	char *new;
} replace[] = {
	"#9", nil,	/* must be first */
	"#d", "/dev/fd",
};

char*
unsharp(char *old)
{
	char *new;
	int i, olen, nlen, len;

	if(replace[0].new == nil)
		replace[0].new = get9root();

	for(i=0; i<nelem(replace); i++){
		if(!replace[i].new)
			continue;
		olen = strlen(replace[i].old);
		if(strncmp(old, replace[i].old, olen) != 0
		|| (old[olen] != '\0' && old[olen] != '/'))
			continue;
		nlen = strlen(replace[i].new);
		len = strlen(old)+nlen-olen;
		new = malloc(len+1);
		if(new == nil)
			/* Most callers won't check the return value... */
			sysfatal("out of memory translating %s to %s%s", old, replace[i].new, old+olen);
		strcpy(new, replace[i].new);
		strcpy(new+nlen, old+olen);
		assert(strlen(new) == len);
		return new;
	}
	return old;
}
