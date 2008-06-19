#include <u.h>
#include <libc.h>

int chattyfuse;

int
post9pservice(int fd, char *name, char *mtpt)
{
	int i, pid;
	char *ns, *addr;
	Waitmsg *w;

	if(name == nil && mtpt == nil){
		close(fd);
		werrstr("nothing to do");
		return -1;
	}

	if(name){
		if(strchr(name, '!'))	/* assume is already network address */
			addr = strdup(name);
		else{
			if((ns = getns()) == nil)
				return -1;
			addr = smprint("unix!%s/%s", ns, name);
			free(ns);
		}
		if(addr == nil)
			return -1;
		switch(pid = fork()){
		case -1:
			return -1;
		case 0:
			dup(fd, 0);
			dup(fd, 1);
			for(i=3; i<20; i++)
				close(i);
			execlp("9pserve", "9pserve", "-u", addr, (char*)0);
			fprint(2, "exec 9pserve: %r\n");
			_exits("exec");
		}
		close(fd);
		w = waitfor(pid);
		if(w == nil)
			return -1;
		if(w->msg && w->msg[0]){
			free(w);
			werrstr("9pserve failed");
			return -1;
		}
		free(w);
		if(mtpt){
			/* reopen */
			if((fd = dial(addr, nil, nil, nil)) < 0){
				werrstr("cannot reopen for mount: %r");
				return -1;
			}
		}
		free(addr);
	}
	if(mtpt){
		switch(pid = rfork(RFFDG|RFPROC|RFNOWAIT)){
		case -1:
			return -1;
		case 0:
			dup(fd, 0);
			for(i=3; i<20; i++)
				close(i);

			/* Try v9fs on Linux, which will mount 9P directly. */
			execlp("mount9p", "mount9p", "-", mtpt, (char*)0);

			if(chattyfuse)
				execlp("9pfuse", "9pfuse", "-D", "-", mtpt, (char*)0);
			else
				execlp("9pfuse", "9pfuse", "-", mtpt, (char*)0);
			fprint(2, "exec 9pfuse: %r\n");
			_exits("exec");
		}
		close(fd);
	}
	return 0;
}
