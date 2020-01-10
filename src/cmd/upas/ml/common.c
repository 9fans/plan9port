#include "common.h"
#include "dat.h"

String*
getaddr(Node *p)
{
	for(; p; p = p->next){
		if(p->s && p->addr)
			return p->s;
	}
	return nil;
}

/* send messae adding our own reply-to and precedence */
void
getaddrs(void)
{
	Field *f;

	for(f = firstfield; f; f = f->next){
		if(f->node->c == FROM && from == nil)
			from = getaddr(f->node);
		if(f->node->c == SENDER && sender == nil)
			sender = getaddr(f->node);
	}
}

/* write address file, should be append only */
void
writeaddr(char *file, char *addr, int rem, char *listname)
{
	int fd;
	Dir nd;

	fd = open(file, OWRITE);
	if(fd < 0){
		fd = create(file, OWRITE, DMAPPEND|0666);
		if(fd < 0)
			sysfatal("creating address list %s: %r", file);
		nulldir(&nd);
		nd.mode = DMAPPEND|0666;
		dirwstat(file, &nd);
	} else
		seek(fd, 0, 2);
	if(rem)
		fprint(fd, "!%s\n", addr);
	else
		fprint(fd, "%s\n", addr);
	close(fd);

	if(*addr != '#')
		sendnotification(addr, listname, rem);
}

void
remaddr(char *addr)
{
	Addr **l;
	Addr *a;

	for(l = &al; *l; l = &(*l)->next){
		a = *l;
		if(strcmp(addr, a->addr) == 0){
			(*l) = a->next;
			free(a);
			na--;
			break;
		}
	}
}

int
addaddr(char *addr)
{
	Addr **l;
	Addr *a;

	for(l = &al; *l; l = &(*l)->next){
		if(strcmp(addr, (*l)->addr) == 0)
			return 0;
	}
	na++;
	*l = a = malloc(sizeof(*a)+strlen(addr)+1);
	if(a == nil)
		sysfatal("allocating: %r");
	a->addr = (char*)&a[1];
	strcpy(a->addr, addr);
	a->next = nil;
	*l = a;
	return 1;
}

/* read address file */
void
readaddrs(char *file)
{
	Biobuf *b;
	char *p;

	b = Bopen(file, OREAD);
	if(b == nil)
		return;

	while((p = Brdline(b, '\n')) != nil){
		p[Blinelen(b)-1] = 0;
		if(*p == '#')
			continue;
		if(*p == '!')
			remaddr(p+1);
		else
			addaddr(p);
	}
	Bterm(b);
}

/* start a mailer sending to all the receivers */
int
startmailer(char *name)
{
	int pfd[2];
	char **av;
	int ac;
	Addr *a;

	putenv("upasname", "/dev/null");
	if(pipe(pfd) < 0)
		sysfatal("creating pipe: %r");
	switch(fork()){
	case -1:
		sysfatal("starting mailer: %r");
	case 0:
		close(pfd[1]);
		break;
	default:
		close(pfd[0]);
		return pfd[1];
	}

	dup(pfd[0], 0);
	close(pfd[0]);

	av = malloc(sizeof(char*)*(na+2));
	if(av == nil)
		sysfatal("starting mailer: %r");
	ac = 0;
	av[ac++] = name;
	for(a = al; a != nil; a = a->next)
		av[ac++] = a->addr;
	av[ac] = 0;
	exec("/bin/upas/send", av);
	sysfatal("execing mailer: %r");

	/* not reached */
	return -1;
}

void
sendnotification(char *addr, char *listname, int rem)
{
	int pfd[2];
	Waitmsg *w;

	putenv("upasname", "/dev/null");
	if(pipe(pfd) < 0)
		sysfatal("creating pipe: %r");
	switch(fork()){
	case -1:
		sysfatal("starting mailer: %r");
	case 0:
		close(pfd[1]);
		dup(pfd[0], 0);
		close(pfd[0]);
		execl("/bin/upas/send", "mlnotify", addr, nil);
		sysfatal("execing mailer: %r");
		break;
	default:
		close(pfd[0]);
		fprint(pfd[1], "From: %s-owner\n\n", listname);
		if(rem)
			fprint(pfd[1], "You have removed from the %s mailing list\n", listname);
		else{
			fprint(pfd[1], "You have been added to the %s mailing list\n", listname);
			fprint(pfd[1], "To be removed, send an email to %s-owner containing\n",
				listname);
			fprint(pfd[1], "the word 'remove' in the subject or body.\n");
		}
		close(pfd[1]);

		/* wait for mailer to end */
		while(w = wait()){
			if(w->msg != nil && w->msg[0])
				sysfatal("%s", w->msg);
			free(w);
		}
		break;
	}
}
