#include <u.h>
#include <libc.h>
#include <auth.h>

int
auth_getkey(char *params)
{
	char *name;
	Dir *d;
	int pid;
	Waitmsg *w;

	/* start /factotum to query for a key */
	name = "/factotum";
	d = dirstat(name);
	if(d == nil){
		name = "/boot/factotum";
		d = dirstat(name);
	}
	if(d == nil){
		werrstr("auth_getkey: no /factotum or /boot/factotum: didn't get key %s", params);
		return -1;
	}
if(0)	if(d->type != '/'){
		werrstr("auth_getkey: /factotum may be bad: didn't get key %s", params);
		return -1;
	}
	switch(pid = fork()){
	case -1:
		werrstr("can't fork for %s: %r", name);
		return -1;
	case 0:
		execl(name, "getkey", "-g", params, nil);
		exits(0);
	default:
		for(;;){
			w = wait();
			if(w == nil)
				break;
			if(w->pid == pid){
				if(w->msg[0] != '\0'){
					free(w);
					return -1;
				}
				free(w);
				return 0;
			}
		}
	}
	return 0;
}
