#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>

void
moveto(Mousectl *m, Point pt)
{
	fprint(m->mfd, "m%d %d", pt.x, pt.y);
	m->xy = pt;
}

void
closemouse(Mousectl *mc)
{
	if(mc == nil)
		return;

	postnote(PNPROC, mc->pid, "kill");

	do; while(nbrecv(mc->c, &mc->Mouse) > 0);

	close(mc->mfd);
	close(mc->cfd);
	free(mc->file);
	free(mc->c);
	free(mc->resizec);
	free(mc);
}

int
readmouse(Mousectl *mc)
{
	if(mc->image)
		flushimage(mc->image->display, 1);
	if(recv(mc->c, &mc->Mouse) < 0){
		fprint(2, "readmouse: %r\n");
		return -1;
	}
	return 0;
}

static
void
_ioproc(void *arg)
{
	int n, nerr, one;
	char buf[1+5*12];
	Mouse m;
	Mousectl *mc;

	mc = arg;
	threadsetname("mouseproc");
	one = 1;
	memset(&m, 0, sizeof m);
	mc->pid = getpid();
	nerr = 0;
	for(;;){
		n = read(mc->mfd, buf, sizeof buf);
		if(n != 1+4*12){
			yield();	/* if error is due to exiting, we'll exit here */
			fprint(2, "mouse: bad count %d not 49: %r\n", n);
			if(n<0 || ++nerr>10)
				threadexits("read error");
			continue;
		}
		nerr = 0;
		switch(buf[0]){
		case 'r':
			send(mc->resizec, &one);
			/* fall through */
		case 'm':
			m.xy.x = atoi(buf+1+0*12);
			m.xy.y = atoi(buf+1+1*12);
			m.buttons = atoi(buf+1+2*12);
			m.msec = atoi(buf+1+3*12);
			send(mc->c, &m);
			/*
			 * mc->Mouse is updated after send so it doesn't have wrong value if we block during send.
			 * This means that programs should receive into mc->Mouse (see readmouse() above) if
			 * they want full synchrony.
			 */
			mc->Mouse = m;
			break;
		}
	}
}

Mousectl*
initmouse(char *file, Image *i)
{
	Mousectl *mc;
	char *t, *sl;

	mc = mallocz(sizeof(Mousectl), 1);
	if(file == nil)
		file = "/dev/mouse";
	mc->file = strdup(file);
	mc->mfd = open(file, ORDWR|OCEXEC);
	if(mc->mfd<0 && strcmp(file, "/dev/mouse")==0){
		bind("#m", "/dev", MAFTER);
		mc->mfd = open(file, ORDWR|OCEXEC);
	}
	if(mc->mfd < 0){
		free(mc);
		return nil;
	}
	t = malloc(strlen(file)+16);
	strcpy(t, file);
	sl = utfrrune(t, '/');
	if(sl)
		strcpy(sl, "/cursor");
	else
		strcpy(t, "/dev/cursor");
	mc->cfd = open(t, ORDWR|OCEXEC);
	free(t);
	mc->image = i;
	mc->c = chancreate(sizeof(Mouse), 0);
	mc->resizec = chancreate(sizeof(int), 2);
	proccreate(_ioproc, mc, 4096);
	return mc;
}

void
setcursor(Mousectl *mc, Cursor *c)
{
	char curs[2*4+2*2*16];

	if(c == nil)
		write(mc->cfd, curs, 0);
	else{
		BPLONG(curs+0*4, c->offset.x);
		BPLONG(curs+1*4, c->offset.y);
		memmove(curs+2*4, c->clr, 2*2*16);
		write(mc->cfd, curs, sizeof curs);
	}
}
