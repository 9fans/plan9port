/*
 * rasptest: oracle harness for the Go port of samterm's rasp (rasp.go).
 *
 * Reads a line-based script from stdin, drives a Rasp, and writes each
 * query's result to stdout. The Go samterm package runs the same script
 * (see rasp_test.go) and must produce byte-identical output.
 *
 * Script commands (one per line):
 *	resize A OLD NEW	rresize: replace OLD runes at A with a hole of NEW
 *	data P0 P1 TEXT		rdata: fill [P0,P1) with TEXT (rune length P1-P0)
 *	fill P0 P1 C		rdata: fill [P0,P1) with repeated character C
 *	clean			rclean: coalesce adjacent sections
 *	clear			rclear: drop the sections, keep nrunes
 *	init			rinit: reset to empty
 *	nrunes			print "nrunes = N"
 *	load P0 P1		print rload's present text (holes skipped)
 *	missing P0 P1		print rmissing
 *	contig P0 P1 T		print rcontig (T=1 present, T=0 absent)
 *	sects			print the section list: +N present, -N hole
 *	#...			comment (ignored)
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include "flayer.h"
#include "samterm.h"

Rune	*scratch;
long	nscralloc;

void
panic(char *s)
{
	fprint(2, "rasptest: panic: %s\n", s);
	abort();
}

void*
alloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == 0)
		panic("alloc");
	memset(p, 0, n);
	return p;
}

void
threadmain(int argc, char *argv[])
{
	Biobuf *b;
	Rasp r;
	Rune *rbuf, *s;
	Section *sect;
	char *line, *p, *args[4];
	long p0, p1;
	ulong nr;
	int i, n, tf;

	USED(argc);
	USED(argv);

	memset(&r, 0, sizeof r);
	rinit(&r);
	rbuf = malloc(64*1024*sizeof(Rune));
	b = Bfdopen(0, OREAD);
	if(b == nil)
		sysfatal("Bfdopen: %r");

	while((line = Brdline(b, '\n')) != nil){
		line[Blinelen(b)-1] = 0;
		if(line[0] == '#' || line[0] == 0)
			continue;
		/* split off command word; keep the remainder intact for data */
		p = line;
		while(*p && *p != ' ')
			p++;
		if(*p)
			*p++ = 0;
		if(strcmp(line, "resize") == 0){
			if(tokenize(p, args, nelem(args)) != 3)
				sysfatal("resize A OLD NEW");
			rresize(&r, atol(args[0]), atol(args[1]), atol(args[2]));
		}else if(strcmp(line, "data") == 0){
			p0 = strtol(p, &p, 10);
			p1 = strtol(p, &p, 10);
			if(*p != ' ')
				sysfatal("data P0 P1 TEXT");
			p++;
			n = 0;
			while(*p)
				p += chartorune(&rbuf[n], p), n++;
			if(n != p1-p0)
				sysfatal("data: TEXT has %d runes, want %ld", n, p1-p0);
			rdata(&r, p0, p1, rbuf);
		}else if(strcmp(line, "fill") == 0){
			if(tokenize(p, args, nelem(args)) != 3)
				sysfatal("fill P0 P1 C");
			p0 = atol(args[0]);
			p1 = atol(args[1]);
			for(i = 0; i < p1-p0; i++)
				rbuf[i] = args[2][0];
			rdata(&r, p0, p1, rbuf);
		}else if(strcmp(line, "clean") == 0){
			rclean(&r);
		}else if(strcmp(line, "clear") == 0){
			rclear(&r);
		}else if(strcmp(line, "init") == 0){
			rinit(&r);
		}else if(strcmp(line, "nrunes") == 0){
			print("nrunes = %ld\n", r.nrunes);
		}else if(strcmp(line, "load") == 0){
			if(tokenize(p, args, nelem(args)) != 2)
				sysfatal("load P0 P1");
			p0 = atol(args[0]);
			p1 = atol(args[1]);
			s = rload(&r, p0, p1, &nr);
			print("load %ld %ld = \"%S\"\n", p0, p1, s);
		}else if(strcmp(line, "missing") == 0){
			if(tokenize(p, args, nelem(args)) != 2)
				sysfatal("missing P0 P1");
			p0 = atol(args[0]);
			p1 = atol(args[1]);
			print("missing %ld %ld = %d\n", p0, p1, rmissing(&r, p0, p1));
		}else if(strcmp(line, "contig") == 0){
			if(tokenize(p, args, nelem(args)) != 3)
				sysfatal("contig P0 P1 T");
			p0 = atol(args[0]);
			p1 = atol(args[1]);
			tf = atoi(args[2]);
			print("contig %ld %ld %d = %d\n", p0, p1, tf, rcontig(&r, p0, p1, tf));
		}else if(strcmp(line, "sects") == 0){
			print("sects =");
			for(sect = r.sect; sect; sect = sect->next)
				print(sect->text ? " +%ld" : " -%ld", sect->nrunes);
			print(" (nrunes %ld)\n", r.nrunes);
		}else
			sysfatal("unknown command %s", line);
	}
	threadexitsall(nil);
}
