#include <u.h>
#include <libc.h>

/*
 * Search $PATH for an executable with the given name.
 * Like in rc, mid-name slashes do not disable search.
 * Should probably handle escaped colons,
 * but I don't know what the syntax is.
 */
char*
searchpath(char *name)
{
	char *path, *p, *next;
	char *s, *ss;
	int ns, l;

	s = nil;
	ns = 0;
	if((name[0] == '.' && name[1] == '/')
	|| (name[0] == '.' && name[1] == '.' && name[2] == '/')
	|| (name[0] == '/')){
		if(access(name, AEXEC) >= 0)
			return strdup(name);
		return nil;
	}

	path = getenv("PATH");
	for(p=path; p && *p; p=next){
		if((next = strchr(p, ':')) != nil)
			*next++ = 0;
		if(*p == 0){
			if(access(name, AEXEC) >= 0){
				free(s);
				free(path);
				return strdup(name);
			}
		}else{
			l = strlen(p)+1+strlen(name)+1;
			if(l > ns){
				ss = realloc(s, l);
				if(ss == nil){
					free(s);
					free(path);
					return nil;
				}
				s = ss;
				ns = l;
			}
			strcpy(s, p);
			strcat(s, "/");
			strcat(s, name);
			if(access(s, AEXEC) >= 0){
				free(path);
				return s;
			}
		}
	}
	free(s);
	free(path);
	return nil;
}

