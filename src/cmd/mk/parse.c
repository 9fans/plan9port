#include	"mk.h"

char *infile;
int mkinline;
static int rhead(char *, Word **, Word **, int *, char **);
static char *rbody(Biobuf*);
extern Word *target1;

void
parse(char *f, int fd, int varoverride)
{
	int hline;
	char *body;
	Word *head, *tail;
	int attr, set, pid;
	char *prog, *p;
	int newfd;
	Biobuf in;
	Bufblock *buf;
	char *err;

	if(fd < 0){
		fprint(2, "open %s: %r\n", f);
		Exit();
	}
	pushshell();
	ipush();
	infile = strdup(f);
	mkinline = 1;
	Binit(&in, fd, OREAD);
	buf = newbuf();
	while(assline(&in, buf)){
		hline = mkinline;
		switch(rhead(buf->start, &head, &tail, &attr, &prog))
		{
		case '<':
			p = wtos(tail, ' ');
			if(*p == 0){
				SYNERR(-1);
				fprint(2, "missing include file name\n");
				Exit();
			}
			newfd = open(p, OREAD);
			if(newfd < 0){
				fprint(2, "warning: skipping missing include file %s: %r\n", p);
			} else
				parse(p, newfd, 0);
			break;
		case '|':
			p = wtos(tail, ' ');
			if(*p == 0){
				SYNERR(-1);
				fprint(2, "missing include program name\n");
				Exit();
			}
			execinit();
			pid=pipecmd(p, envy, &newfd, shellt, shellcmd);
			if(newfd < 0){
				fprint(2, "warning: skipping missing program file %s: %r\n", p);
			} else
				parse(p, newfd, 0);
			while(waitup(-3, &pid) >= 0)
				;
			if(pid != 0){
				fprint(2, "bad include program status\n");
				Exit();
			}
			break;
		case ':':
			body = rbody(&in);
			addrules(head, tail, body, attr, hline, prog);
			break;
		case '=':
			if(head->next){
				SYNERR(-1);
				fprint(2, "multiple vars on left side of assignment\n");
				Exit();
			}
			if(symlook(head->s, S_OVERRIDE, 0)){
				set = varoverride;
			} else {
				set = 1;
				if(varoverride)
					symlook(head->s, S_OVERRIDE, (void *)"");
			}
			if(set){
/*
char *cp;
dumpw("tail", tail);
cp = wtos(tail, ' '); print("assign %s to %s\n", head->s, cp); free(cp);
*/
				setvar(head->s, (void *) tail);
				symlook(head->s, S_WESET, (void *)"");
				if(strcmp(head->s, "MKSHELL") == 0){
					if((err = setshell(tail)) != nil){
						SYNERR(hline);
						fprint(2, "%s\n", err);
						Exit();
						break;
					}
				}
			}
			if(attr)
				symlook(head->s, S_NOEXPORT, (void *)"");
			break;
		default:
			SYNERR(hline);
			fprint(2, "expected one of :<=\n");
			Exit();
			break;
		}
	}
	close(fd);
	freebuf(buf);
	ipop();
	popshell();
}

void
addrules(Word *head, Word *tail, char *body, int attr, int hline, char *prog)
{
	Word *w;

	assert("addrules args", head && body);
		/* tuck away first non-meta rule as default target*/
	if(target1 == 0 && !(attr&REGEXP)){
		for(w = head; w; w = w->next)
			if(shellt->charin(w->s, "%&"))
				break;
		if(w == 0)
			target1 = wdup(head);
	}
	for(w = head; w; w = w->next)
		addrule(w->s, tail, body, head, attr, hline, prog);
}

static int
rhead(char *line, Word **h, Word **t, int *attr, char **prog)
{
	char *p;
	char *pp;
	int sep;
	Rune r;
	int n;
	Word *w;

	p = shellt->charin(line,":=<");
	if(p == 0)
		return('?');
	sep = *p;
	*p++ = 0;
	if(sep == '<' && *p == '|'){
		sep = '|';
		p++;
	}
	*attr = 0;
	*prog = 0;
	if(sep == '='){
		pp = shellt->charin(p, shellt->termchars);	/* termchars is shell-dependent */
		if (pp && *pp == '=') {
			while (p != pp) {
				n = chartorune(&r, p);
				switch(r)
				{
				default:
					SYNERR(-1);
					fprint(2, "unknown attribute '%c'\n",*p);
					Exit();
				case 'U':
					*attr = 1;
					break;
				}
				p += n;
			}
			p++;		/* skip trailing '=' */
		}
	}
	if((sep == ':') && *p && (*p != ' ') && (*p != '\t')){
		while (*p) {
			n = chartorune(&r, p);
			if (r == ':')
				break;
			p += n;
			switch(r)
			{
			default:
				SYNERR(-1);
				fprint(2, "unknown attribute '%c'\n", p[-1]);
				Exit();
			case 'D':
				*attr |= DEL;
				break;
			case 'E':
				*attr |= NOMINUSE;
				break;
			case 'n':
				*attr |= NOVIRT;
				break;
			case 'N':
				*attr |= NOREC;
				break;
			case 'P':
				pp = utfrune(p, ':');
				if (pp == 0 || *pp == 0)
					goto eos;
				*pp = 0;
				*prog = strdup(p);
				*pp = ':';
				p = pp;
				break;
			case 'Q':
				*attr |= QUIET;
				break;
			case 'R':
				*attr |= REGEXP;
				break;
			case 'U':
				*attr |= UPD;
				break;
			case 'V':
				*attr |= VIR;
				break;
			}
		}
		if (*p++ != ':') {
	eos:
			SYNERR(-1);
			fprint(2, "missing trailing :\n");
			Exit();
		}
	}
	*h = w = stow(line);
	if(*w->s == 0 && sep != '<' && sep != '|' && sep != 'S') {
		SYNERR(mkinline-1);
		fprint(2, "no var on left side of assignment/rule\n");
		Exit();
	}
	*t = stow(p);
	return(sep);
}

static char *
rbody(Biobuf *in)
{
	Bufblock *buf;
	int r, lastr;
	char *p;

	lastr = '\n';
	buf = newbuf();
	for(;;){
		r = Bgetrune(in);
		if (r < 0)
			break;
		if (lastr == '\n') {
			if (r == '#')
				rinsert(buf, r);
			else if (r != ' ' && r != '\t') {
				Bungetrune(in);
				break;
			}
		} else
			rinsert(buf, r);
		lastr = r;
		if (r == '\n')
			mkinline++;
	}
	insert(buf, 0);
	p = strdup(buf->start);
	freebuf(buf);
	return p;
}

struct input
{
	char *file;
	int line;
	struct input *next;
};
static struct input *inputs = 0;

void
ipush(void)
{
	struct input *in, *me;

	me = (struct input *)Malloc(sizeof(*me));
	me->file = infile;
	me->line = mkinline;
	me->next = 0;
	if(inputs == 0)
		inputs = me;
	else {
		for(in = inputs; in->next; )
			in = in->next;
		in->next = me;
	}
}

void
ipop(void)
{
	struct input *in, *me;

	assert("pop input list", inputs != 0);
	if(inputs->next == 0){
		me = inputs;
		inputs = 0;
	} else {
		for(in = inputs; in->next->next; )
			in = in->next;
		me = in->next;
		in->next = 0;
	}
	infile = me->file;
	mkinline = me->line;
	free((char *)me);
}
