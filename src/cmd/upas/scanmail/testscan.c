#include "sys.h"
#include "spam.h"

int 	debug;
Biobuf	bin;
char	patfile[128], header[Hdrsize+2];
char	cmd[1024];

char*	canon(Biobuf*, char*, char*, int*);
int	matcher(char *, Pattern*, char*, Resub*);
int	matchaction(Patterns*, char*);

void
usage(void)
{
	fprint(2, "missing or bad arguments to qer\n");
	exits("usage");
}

void *
Malloc(long n)
{
	void *p;

	p = malloc(n);
	if(p == 0){
		fprint(2, "malloc error");
		exits("malloc");
	}
	return p;
}

void*
Realloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(p == 0){
		fprint(2, "realloc error");
		exits("realloc");
	}
	return p;
}

void
dumppats(void)
{
	int i, j;
	Pattern *p;
	Spat *s, *q;

	for(i = 0; patterns[i].action; i++){
		for(p = patterns[i].regexps; p; p = p->next){
			print("%s <REGEXP>\n", patterns[i].action);
			if(p->alt)
				print("Alt:");
			for(s = p->alt; s; s = s->next)
				print("\t%s\n", s->string);
		}
		p = patterns[i].strings;
		if(p == 0)
			continue;

		for(j = 0; j < Nhash; j++){
			for(s = p->spat[j]; s; s = s->next){
				print("%s %s\n", patterns[i].action, s->string);
				if(s->alt)
					print("Alt:");
				for(q = s->alt; q; q = q->next)
					print("\t%s\n", q->string);
			}
		}
	}
}

void
main(int argc, char *argv[])
{
	int i, fd, n, aflag, vflag;
	char body[Bodysize+2], *raw, *ret;
	Biobuf *bp;

	sprint(patfile, "%s/patterns", UPASLIB);
	aflag = -1;
	vflag = 0;
	ARGBEGIN {
	case 'a':
		aflag = 1;
		break;
	case 'v':
		vflag = 1;
		break;
	case 'd':
		debug++;
		break;
	case 'p':
		strcpy(patfile,ARGF());
		break;
	} ARGEND

	bp = Bopen(patfile, OREAD);
	if(bp){
		parsepats(bp);
		Bterm(bp);
	}

	if(argc >= 1){
		fd = open(*argv, OREAD);
		if(fd < 0){
			fprint(2, "can't open %s\n", *argv);
			exits("open");
		}
		Binit(&bin, fd, OREAD);
	} else
		Binit(&bin, 0, OREAD);

	*body = 0;
	*header = 0;
	ret = 0;
	for(;;){
		raw = canon(&bin, header+1, body+1, &n);
		if(raw == 0)
			break;
		if(aflag == 0)
			continue;
		if(aflag < 0)
			aflag = 0;
		if(vflag){
			if(header[1]) {
				fprint(2, "\t**** Header ****\n\n");
				write(2, header+1, strlen(header+1));
				fprint(2, "\n");
			}
			fprint(2, "\t**** Body ****\n\n");
			if(body[1])
				write(2, body+1, strlen(body+1));
			fprint(2, "\n");
		}

		for(i = 0; patterns[i].action; i++){
			if(matchaction(&patterns[i], header+1))
				ret = patterns[i].action;
			if(i == HoldHeader)
				continue;
			if(matchaction(&patterns[i], body+1))
				ret = patterns[i].action;
		}
	}
	exits(ret);
}

char*
canon(Biobuf *bp, char *header, char *body, int *n)
{
	int hsize, base64;

	static char *raw;

	hsize = 0;
	base64 = 0;
	*header = 0;
	*body = 0;
	if(raw == 0){
		raw = readmsg(bp, &hsize, n);
		if(raw)
			base64 = convert(raw, raw+hsize, header, Hdrsize, 0);
	} else {
		free(raw);
		raw = readmsg(bp, 0, n);
	}
	if(raw){
		if(base64)
			conv64(raw+hsize, raw+*n, body, Bodysize);
		else
			convert(raw+hsize, raw+*n, body, Bodysize, 1);
	}
	return raw;
}

int
matchaction(Patterns *pp, char *message)
{
	char *name, *cp;
	int ret;
	Pattern *p;
	Resub m[1];

	if(message == 0 || *message == 0)
		return 0;

	name = pp->action;
	p = pp->strings;
	ret = 0;
	if(p)
		for(cp = message; matcher(name, p, cp, m); cp = m[0].e.ep)
				ret++;

	for(p = pp->regexps; p; p = p->next)
		for(cp = message; matcher(name, p, cp, m); cp = m[0].e.ep)
				ret++;
	return ret;
}

int
matcher(char *action, Pattern *p, char *message, Resub *m)
{
	if(matchpat(p, message, m)){
		if(p->action != Lineoff)
			xprint(1, action, m);
		return 1;
	}
	return 0;
}
