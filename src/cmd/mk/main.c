#include	"mk.h"

#define		MKFILE		"mkfile"

int debug;
Rule *rules, *metarules;
int nflag = 0;
int tflag = 0;
int iflag = 0;
int kflag = 0;
int aflag = 0;
int uflag = 0;
char *explain = 0;
Word *target1;
int nreps = 1;
Job *jobs;
Biobuf bout;
Rule *patrule;
void badusage(void);
#ifdef	PROF
short buf[10000];
#endif

int
main(int argc, char **argv)
{
	Word *w;
	char *s, *temp;
	char *files[256], **f = files, **ff;
	int sflag = 0;
	int i;
	int tfd = -1;
	Biobuf tb;
	Bufblock *buf;
	Bufblock *whatif;

	/*
	 *  start with a copy of the current environment variables
	 *  instead of sharing them
	 */

	Binit(&bout, 1, OWRITE);
	buf = newbuf();
	whatif = 0;
	USED(argc);
	for(argv++; *argv && (**argv == '-'); argv++)
	{
		bufcpy(buf, argv[0], strlen(argv[0]));
		insert(buf, ' ');
		switch(argv[0][1])
		{
		case 'a':
			aflag = 1;
			break;
		case 'd':
			if(*(s = &argv[0][2]))
				while(*s) switch(*s++)
				{
				case 'p':	debug |= D_PARSE; break;
				case 'g':	debug |= D_GRAPH; break;
				case 'e':	debug |= D_EXEC; break;
				}
			else
				debug = 0xFFFF;
			break;
		case 'e':
			explain = &argv[0][2];
			break;
		case 'f':
			if(*++argv == 0)
				badusage();
			*f++ = *argv;
			bufcpy(buf, argv[0], strlen(argv[0]));
			insert(buf, ' ');
			break;
		case 'i':
			iflag = 1;
			break;
		case 'k':
			kflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case 'w':
			if(whatif == 0)
				whatif = newbuf();
			else
				insert(whatif, ' ');
			if(argv[0][2])
				bufcpy(whatif, &argv[0][2], strlen(&argv[0][2]));
			else {
				if(*++argv == 0)
					badusage();
				bufcpy(whatif, &argv[0][0], strlen(&argv[0][0]));
			}
			break;
		default:
			badusage();
		}
	}
#ifdef	PROF
	{
		extern etext();
		monitor(main, etext, buf, sizeof buf, 300);
	}
#endif

	if(aflag)
		iflag = 1;
	usage();
	syminit();
	initshell();
	initenv();
	usage();

	/*
		assignment args become null strings
	*/
	temp = 0;
	for(i = 0; argv[i]; i++) if(utfrune(argv[i], '=')){
		bufcpy(buf, argv[i], strlen(argv[i]));
		insert(buf, ' ');
		if(tfd < 0){
			temp = maketmp(&tfd);
			if(temp == 0) {
				fprint(2, "temp file: %r\n");
				Exit();
			}
			Binit(&tb, tfd, OWRITE);
		}
		Bprint(&tb, "%s\n", argv[i]);
		*argv[i] = 0;
	}
	if(tfd >= 0){
		Bflush(&tb);
		LSEEK(tfd, 0L, 0);
		parse("command line args", tfd, 1);
		remove(temp);
	}

	if (buf->current != buf->start) {
		buf->current--;
		insert(buf, 0);
	}
	symlook("MKFLAGS", S_VAR, (void *) stow(buf->start));
	buf->current = buf->start;
	for(i = 0; argv[i]; i++){
		if(*argv[i] == 0) continue;
		if(i)
			insert(buf, ' ');
		bufcpy(buf, argv[i], strlen(argv[i]));
	}
	insert(buf, 0);
	symlook("MKARGS", S_VAR, (void *) stow(buf->start));
	freebuf(buf);

	if(f == files){
		if(access(MKFILE, 4) == 0)
			parse(MKFILE, open(MKFILE, 0), 0);
	} else
		for(ff = files; ff < f; ff++)
			parse(*ff, open(*ff, 0), 0);
	if(DEBUG(D_PARSE)){
		dumpw("default targets", target1);
		dumpr("rules", rules);
		dumpr("metarules", metarules);
		dumpv("variables");
	}
	if(whatif){
		insert(whatif, 0);
		timeinit(whatif->start);
		freebuf(whatif);
	}
	execinit();
	/* skip assignment args */
	while(*argv && (**argv == 0))
		argv++;

	catchnotes();
	if(*argv == 0){
		if(target1)
			for(w = target1; w; w = w->next)
				mk(w->s);
		else {
			fprint(2, "mk: nothing to mk\n");
			Exit();
		}
	} else {
		if(sflag){
			for(; *argv; argv++)
				if(**argv)
					mk(*argv);
		} else {
			Word *head, *tail, *t;

			/* fake a new rule with all the args as prereqs */
			tail = 0;
			t = 0;
			for(; *argv; argv++)
				if(**argv){
					if(tail == 0)
						tail = t = newword(*argv);
					else {
						t->next = newword(*argv);
						t = t->next;
					}
				}
			if(tail->next == 0)
				mk(tail->s);
			else {
				head = newword("command line arguments");
				addrules(head, tail, strdup(""), VIR, mkinline, 0);
				mk(head->s);
			}
		}
	}
	if(uflag)
		prusage();
	exits(0);
	return 0;
}

void
badusage(void)
{

	fprint(2, "Usage: mk [-f file] [-n] [-a] [-e] [-t] [-k] [-i] [-d[egp]] [targets ...]\n");
	Exit();
}

void *
Malloc(int n)
{
	register void *s;

	s = malloc(n);
	if(!s) {
		fprint(2, "mk: cannot alloc %d bytes\n", n);
		Exit();
	}
	return(s);
}

void *
Realloc(void *s, int n)
{
	if(s)
		s = realloc(s, n);
	else
		s = malloc(n);
	if(!s) {
		fprint(2, "mk: cannot alloc %d bytes\n", n);
		Exit();
	}
	return(s);
}

void
assert(char *s, int n)
{
	if(!n){
		fprint(2, "mk: Assertion ``%s'' failed.\n", s);
		Exit();
	}
}

void
regerror(char *s)
{
	if(patrule)
		fprint(2, "mk: %s:%d: regular expression error; %s\n",
			patrule->file, patrule->line, s);
	else
		fprint(2, "mk: %s:%d: regular expression error; %s\n",
			infile, mkinline, s);
	Exit();
}
