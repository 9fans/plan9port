#include <u.h>
#include <libc.h>

static Waitmsg*
_wait(int n, char *buf)
{
	int l;
	char *fld[5];
	Waitmsg *w;

	if(n <= 0)
		return nil;
	buf[n] = '\0';
	if(tokenize(buf, fld, nelem(fld)) != nelem(fld)){
		werrstr("couldn't parse wait message");
		return nil;
	}
	l = strlen(fld[4])+1;
	w = malloc(sizeof(Waitmsg)+l);
	if(w == nil)
		return nil;
	w->pid = atoi(fld[0]);
	w->time[0] = atoi(fld[1]);
	w->time[1] = atoi(fld[2]);
	w->time[2] = atoi(fld[3]);
	w->msg = (char*)&w[1];
	memmove(w->msg, fld[4], l);
	return w;
}

Waitmsg*
wait(void)
{
	char buf[256];

	return _wait(await(buf, sizeof buf-1), buf);
}

Waitmsg*
waitnohang(void)
{
	char buf[256];

	return _wait(awaitnohang(buf, sizeof buf-1), buf);
}

Waitmsg*
waitfor(int pid)
{
	char buf[256];

	return _wait(awaitfor(pid, buf, sizeof buf-1), buf);
}
