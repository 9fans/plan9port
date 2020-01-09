#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>

typedef struct Cmd Cmd;
struct Cmd {
	char *cmd;
	void	(*fn)(int, char**);
};

Biobuf b;
int fd;
uchar buf[64*1024];

void
startsrv(void)
{
	int pid, p[2];

	if(pipe(p) < 0)
		sysfatal("pipe");
	if((pid=fork()) < 0)
		sysfatal("fork");
	if(pid == 0){
		close(p[0]);
		dup(p[1], 0);
		dup(p[1], 1);
		execl("./o.devdraw", "o.devdraw", "-D", nil);
		sysfatal("exec: %r");
	}
	close(p[1]);
	fd = p[0];
}

int
domsg(Wsysmsg *m)
{
	int n, nn;

	n = convW2M(m, buf, sizeof buf);
fprint(2, "write %d to %d\n", n, fd);
	write(fd, buf, n);
	n = readwsysmsg(fd, buf, sizeof buf);
	nn = convM2W(buf, n, m);
	assert(nn == n);
	if(m->type == Rerror)
		return -1;
	return 0;
}

void
cmdinit(int argc, char **argv)
{
	Wsysmsg m;

	memset(&m, 0, sizeof m);
	m.type = Tinit;
	m.winsize = "100x100";
	m.label = "label";
	if(domsg(&m) < 0)
		sysfatal("domsg");
}

void
cmdmouse(int argc, char **argv)
{
	Wsysmsg m;

	memset(&m, 0, sizeof m);
	m.type = Trdmouse;
	if(domsg(&m) < 0)
		sysfatal("domsg");
	print("%c %d %d %d\n",
		m.resized ? 'r' : 'm',
		m.mouse.xy.x,
		m.mouse.xy.y,
		m.mouse.buttons);
}

void
cmdkbd(int argc, char **argv)
{
	Wsysmsg m;

	memset(&m, 0, sizeof m);
	m.type = Trdkbd;
	if(domsg(&m) < 0)
		sysfatal("domsg");
	print("%d\n", m.rune);
}

Cmd cmdtab[] = {
	{ "init", cmdinit, },
	{ "mouse", cmdmouse, },
	{ "kbd", cmdkbd, },
};

void
main(int argc, char **argv)
{
	char *p, *f[20];
	int i, nf;

	startsrv();

fprint(2, "started...\n");
	Binit(&b, 0, OREAD);
	while((p = Brdstr(&b, '\n', 1)) != nil){
fprint(2, "%s...\n", p);
		nf = tokenize(p, f, nelem(f));
		for(i=0; i<nelem(cmdtab); i++){
			if(strcmp(cmdtab[i].cmd, f[0]) == 0){
				cmdtab[i].fn(nf, f);
				break;
			}
		}
		if(i == nelem(cmdtab))
			print("! unrecognized command %s\n", f[0]);
		free(p);
	}
	exits(0);
}
