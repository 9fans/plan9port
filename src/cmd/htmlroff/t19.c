#include "a.h"

/*
 * 19. Input/output file switching.
 */

/* .so - push new source file */
void
r_so(int argc, Rune **argv)
{
	USED(argc);
	pushinputfile(erunesmprint("%s", unsharp(esmprint("%S", argv[1]))));
}

/* .nx - end this file, switch to arg */
void
r_nx(int argc, Rune **argv)
{
	int n;

	if(argc == 1){
		while(popinput())
			;
	}else{
		if(argc > 2)
			warn("too many arguments for .nx");
		while((n=popinput()) && n != 2)
			;
		pushinputfile(argv[1]);
	}
}

/* .sy - system: run string */
void
r_sy(Rune *name)
{
	USED(name);
	warn(".sy not implemented");
}

/* .pi - pipe output to string */
void
r_pi(Rune *name)
{
	USED(name);
	warn(".pi not implemented");
}

/* .cf - copy contents of filename to output */
void
r_cf(int argc, Rune **argv)
{
	int c;
	char *p;
	Biobuf *b;

	USED(argc);
	p = esmprint("%S", argv[1]);
	if((b = Bopen(p, OREAD)) == nil){
		fprint(2, "%L: open %s: %r\n", p);
		free(p);
		return;
	}
	free(p);

	while((c = Bgetrune(b)) >= 0)
		outrune(c);
	Bterm(b);
}

void
r_inputpipe(Rune *name)
{
	Rune *cmd, *stop, *line;
	int n, pid, p[2], len;
	Waitmsg *w;

	USED(name);
	if(pipe(p) < 0){
		warn("pipe: %r");
		return;
	}
	stop = copyarg();
	cmd = readline(CopyMode);
	pid = fork();
	switch(pid){
	case 0:
		if(p[0] != 0){
			dup(p[0], 0);
			close(p[0]);
		}
		close(p[1]);
		execl(unsharp("#9/bin/rc"), "rc", "-c", esmprint("%S", cmd), nil);
		warn("%Cdp %S: %r", dot, cmd);
		_exits(nil);
	case -1:
		warn("fork: %r");
	default:
		close(p[0]);
		len = runestrlen(stop);
		fprint(p[1], ".ps %d\n", getnr(L(".s")));
		fprint(p[1], ".vs %du\n", getnr(L(".v")));
		fprint(p[1], ".ft %d\n", getnr(L(".f")));
		fprint(p[1], ".ll 8i\n");
		fprint(p[1], ".pl 30i\n");
		while((line = readline(~0)) != nil){
			if(runestrncmp(line, stop, len) == 0
			&& (line[len]==' ' || line[len]==0 || line[len]=='\t'
				|| (line[len]=='\\' && line[len+1]=='}')))
				break;
			n = runestrlen(line);
			line[n] = '\n';
			fprint(p[1], "%.*S", n+1, line);
			free(line);
		}
		free(stop);
		close(p[1]);
		w = wait();
		if(w == nil){
			warn("wait: %r");
			return;
		}
		if(w->msg[0])
			sysfatal("%C%S %S: %s", dot, name, cmd, w->msg);
		free(cmd);
		free(w);
	}
}

void
t19init(void)
{
	addreq(L("so"), r_so, 1);
	addreq(L("nx"), r_nx, -1);
	addraw(L("sy"), r_sy);
	addraw(L("inputpipe"), r_inputpipe);
	addraw(L("pi"), r_pi);
	addreq(L("cf"), r_cf, 1);

	nr(L("$$"), getpid());
}
