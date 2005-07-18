#include <u.h>
#include <libc.h>

int
post9pservice(int fd, char *name)
{
	int i;
	char *ns, *s;
	Waitmsg *w;

	if(strchr(name, '!'))	/* assume is already network address */
		s = strdup(name);
	else{
		if((ns = getns()) == nil)
			return -1;
		s = smprint("unix!%s/%s", ns, name);
		free(ns);
	}
	if(s == nil)
		return -1;
	switch(fork()){
	case -1:
		return -1;
	case 0:
		dup(fd, 0);
		dup(fd, 1);
		for(i=3; i<20; i++)
			close(i);
		execlp("9pserve", "9pserve", "-u", s, (char*)0);
		fprint(2, "exec 9pserve: %r\n");
		_exits("exec");
	default:
		w = wait();
		if(w == nil)
			return -1;
		close(fd);
		free(s);
		if(w->msg && w->msg[0]){
			free(w);
			werrstr("9pserve failed");
			return -1;
		}
		free(w);
		return 0;
	}
}
