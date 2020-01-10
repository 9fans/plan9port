#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <ctype.h>
#include <plumb.h>
#include <9pclient.h>
#include "dat.h"

char*
formathtml(char *body, int *np)
{
	int i, j, p[2], q[2];
	Exec *e;
	char buf[1024];
	Channel *sync;

	e = emalloc(sizeof(struct Exec));
	if(pipe(p) < 0 || pipe(q) < 0)
		error("can't create pipe: %r");

	e->p[0] = p[0];
	e->p[1] = p[1];
	e->q[0] = q[0];
	e->q[1] = q[1];
	e->argv = emalloc(3*sizeof(char*));
	e->argv[0] = estrdup("htmlfmt");
	e->argv[1] = estrdup("-cutf-8");
	e->argv[2] = nil;
	e->prog = "htmlfmt";
	sync = chancreate(sizeof(int), 0);
	e->sync = sync;
	proccreate(execproc, e, EXECSTACK);
	recvul(sync);
	close(p[0]);
	close(q[1]);

	if((i=write(p[1], body, *np)) != *np){
		fprint(2, "Mail: warning: htmlfmt failed: wrote %d of %d: %r\n", i, *np);
		close(p[1]);
		close(q[0]);
		return body;
	}
	close(p[1]);

	free(body);
	body = nil;
	i = 0;
	for(;;){
		j = read(q[0], buf, sizeof buf);
		if(j <= 0)
			break;
		body = realloc(body, i+j+1);
		if(body == nil)
			error("realloc failed: %r");
		memmove(body+i, buf, j);
		i += j;
		body[i] = '\0';
	}
	close(q[0]);

	*np = i;
	return body;
}

char*
readbody(char *type, char *dir, int *np)
{
	char *body;

	body = readfile(dir, "body", np);
	if(body != nil && strcmp(type, "text/html") == 0)
		return formathtml(body, np);
	return body;
}
