#include "stdinc.h"

#include "9.h"

static struct {
	VtLock*	lock;

	Con*	con;
	int	confd[2];
	ushort	tag;
} cbox;

static ulong
cmd9pStrtoul(char* s)
{
	if(strcmp(s, "~0") == 0)
		return ~0UL;
	return strtoul(s, 0, 0);
}

static uvlong
cmd9pStrtoull(char* s)
{
	if(strcmp(s, "~0") == 0)
		return ~0ULL;
	return strtoull(s, 0, 0);
}

static int
cmd9pTag(Fcall*, int, char **argv)
{
	cbox.tag = strtoul(argv[0], 0, 0)-1;

	return 1;
}

static int
cmd9pTwstat(Fcall* f, int, char **argv)
{
	Dir d;
	static uchar buf[DIRMAX];

	memset(&d, 0, sizeof d);
	nulldir(&d);
	d.name = argv[1];
	d.uid = argv[2];
	d.gid = argv[3];
	d.mode = cmd9pStrtoul(argv[4]);
	d.mtime = cmd9pStrtoul(argv[5]);
	d.length = cmd9pStrtoull(argv[6]);

	f->fid = strtol(argv[0], 0, 0);
	f->stat = buf;
	f->nstat = convD2M(&d, buf, sizeof buf);
	if(f->nstat < BIT16SZ){
		vtSetError("Twstat: convD2M failed (internal error)");
		return 0;
	}

	return 1;
}

static int
cmd9pTstat(Fcall* f, int, char** argv)
{
	f->fid = strtol(argv[0], 0, 0);

	return 1;
}

static int
cmd9pTremove(Fcall* f, int, char** argv)
{
	f->fid = strtol(argv[0], 0, 0);

	return 1;
}

static int
cmd9pTclunk(Fcall* f, int, char** argv)
{
	f->fid = strtol(argv[0], 0, 0);

	return 1;
}

static int
cmd9pTwrite(Fcall* f, int, char** argv)
{
	f->fid = strtol(argv[0], 0, 0);
	f->offset = strtoll(argv[1], 0, 0);
	f->data = argv[2];
	f->count = strlen(argv[2]);

	return 1;
}

static int
cmd9pTread(Fcall* f, int, char** argv)
{
	f->fid = strtol(argv[0], 0, 0);
	f->offset = strtoll(argv[1], 0, 0);
	f->count = strtol(argv[2], 0, 0);

	return 1;
}

static int
cmd9pTcreate(Fcall* f, int, char** argv)
{
	f->fid = strtol(argv[0], 0, 0);
	f->name = argv[1];
	f->perm = strtol(argv[2], 0, 8);
	f->mode = strtol(argv[3], 0, 0);

	return 1;
}

static int
cmd9pTopen(Fcall* f, int, char** argv)
{
	f->fid = strtol(argv[0], 0, 0);
	f->mode = strtol(argv[1], 0, 0);

	return 1;
}

static int
cmd9pTwalk(Fcall* f, int argc, char** argv)
{
	int i;

	if(argc < 2){
		vtSetError("usage: Twalk tag fid newfid [name...]");
		return 0;
	}
	f->fid = strtol(argv[0], 0, 0);
	f->newfid = strtol(argv[1], 0, 0);
	f->nwname = argc-2;
	if(f->nwname > MAXWELEM){
		vtSetError("Twalk: too many names");
		return 0;
	}
	for(i = 0; i < argc-2; i++)
		f->wname[i] = argv[2+i];

	return 1;
}

static int
cmd9pTflush(Fcall* f, int, char** argv)
{
	f->oldtag = strtol(argv[0], 0, 0);

	return 1;
}

static int
cmd9pTattach(Fcall* f, int, char** argv)
{
	f->fid = strtol(argv[0], 0, 0);
	f->afid = strtol(argv[1], 0, 0);
	f->uname = argv[2];
	f->aname = argv[3];

	return 1;
}

static int
cmd9pTauth(Fcall* f, int, char** argv)
{
	f->afid = strtol(argv[0], 0, 0);
	f->uname = argv[1];
	f->aname = argv[2];

	return 1;
}

static int
cmd9pTversion(Fcall* f, int, char** argv)
{
	f->msize = strtoul(argv[0], 0, 0);
	if(f->msize > cbox.con->msize){
		vtSetError("msize too big");
		return 0;
	}
	f->version = argv[1];

	return 1;
}

typedef struct Cmd9p Cmd9p;
struct Cmd9p {
	char*	name;
	int	type;
	int	argc;
	char*	usage;
	int	(*f)(Fcall*, int, char**);
};

static Cmd9p cmd9pTmsg[] = {
	"Tversion", Tversion, 2, "msize version", cmd9pTversion,
	"Tauth", Tauth, 3, "afid uname aname", cmd9pTauth,
	"Tflush", Tflush, 1, "oldtag", cmd9pTflush,
	"Tattach", Tattach, 4, "fid afid uname aname", cmd9pTattach,
	"Twalk", Twalk, 0, "fid newfid [name...]", cmd9pTwalk,
	"Topen", Topen, 2, "fid mode", cmd9pTopen,
	"Tcreate", Tcreate, 4, "fid name perm mode", cmd9pTcreate,
	"Tread", Tread, 3, "fid offset count", cmd9pTread,
	"Twrite", Twrite, 3, "fid offset data", cmd9pTwrite,
	"Tclunk", Tclunk, 1, "fid", cmd9pTclunk,
	"Tremove", Tremove, 1, "fid", cmd9pTremove,
	"Tstat", Tstat, 1, "fid", cmd9pTstat,
	"Twstat", Twstat, 7, "fid name uid gid mode mtime length", cmd9pTwstat,
	"nexttag", 0, 0, "", cmd9pTag,
};

static int
cmd9p(int argc, char* argv[])
{
	int i, n;
	Fcall f, t;
	uchar *buf;
	char *usage;
	u32int msize;

	usage = "usage: 9p T-message ...";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc < 1)
		return cliError(usage);

	for(i = 0; i < nelem(cmd9pTmsg); i++){
		if(strcmp(cmd9pTmsg[i].name, argv[0]) == 0)
			break;
	}
	if(i == nelem(cmd9pTmsg))
		return cliError(usage);
	argc--;
	argv++;
	if(cmd9pTmsg[i].argc && argc != cmd9pTmsg[i].argc){
		vtSetError("usage: %s %s",
			cmd9pTmsg[i].name, cmd9pTmsg[i].usage);
		return 0;
	}

	memset(&t, 0, sizeof(t));
	t.type = cmd9pTmsg[i].type;
	if(t.type == Tversion)
		t.tag = NOTAG;
	else
		t.tag = ++cbox.tag;
	msize = cbox.con->msize;
	if(!cmd9pTmsg[i].f(&t, argc, argv))
		return 0;
	buf = vtMemAlloc(msize);
	n = convS2M(&t, buf, msize);
	if(n <= BIT16SZ){
		vtSetError("%s: convS2M error", cmd9pTmsg[i].name);
		vtMemFree(buf);
		return 0;
	}
	if(write(cbox.confd[0], buf, n) != n){
		vtSetError("%s: write error: %r", cmd9pTmsg[i].name);
		vtMemFree(buf);
		return 0;
	}
	consPrint("\t-> %F\n", &t);

	if((n = read9pmsg(cbox.confd[0], buf, msize)) <= 0){
		vtSetError("%s: read error: %r", cmd9pTmsg[i].name);
		vtMemFree(buf);
		return 0;
	}
	if(convM2S(buf, n, &f) == 0){
		vtSetError("%s: convM2S error", cmd9pTmsg[i].name);
		vtMemFree(buf);
		return 0;
	}
	consPrint("\t<- %F\n", &f);

	vtMemFree(buf);
	return 1;
}

static int
cmdDot(int argc, char* argv[])
{
	long l;
	Dir *dir;
	int fd, r;
	vlong length;
	char *f, *p, *s, *usage;

	usage = "usage: . file";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc != 1)
		return cliError(usage);

	if((dir = dirstat(argv[0])) == nil)
		return cliError(". dirstat %s: %r", argv[0]);
	length = dir->length;
	free(dir);

	r = 1;
	if(length != 0){
		/*
		 * Read the whole file in.
		 */
		if((fd = open(argv[0], OREAD)) < 0)
			return cliError(". open %s: %r", argv[0]);
		f = vtMemAlloc(dir->length+1);
		if((l = read(fd, f, length)) < 0){
			vtMemFree(f);
			close(fd);
			return cliError(". read %s: %r", argv[0]);
		}
		close(fd);
		f[l] = '\0';

		/*
		 * Call cliExec() for each line.
		 */
		for(p = s = f; *p != '\0'; p++){
			if(*p == '\n'){
				*p = '\0';
				if(cliExec(s) == 0){
					r = 0;
					consPrint("%s: %R\n", s);
				}
				s = p+1;
			}
		}
		vtMemFree(f);
	}

	if(r == 0)
		vtSetError("errors in . %#q", argv[0]);
	return r;
}

static int
cmdDflag(int argc, char* argv[])
{
	char *usage;

	usage = "usage: dflag";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc)
		return cliError(usage);

	Dflag ^= 1;
	consPrint("dflag %d\n", Dflag);

	return 1;
}

static int
cmdEcho(int argc, char* argv[])
{
	char *usage;
	int i, nflag;

	nflag = 0;
	usage = "usage: echo [-n] ...";

	ARGBEGIN{
	default:
		return cliError(usage);
	case 'n':
		nflag = 1;
		break;
	}ARGEND

	for(i = 0; i < argc; i++){
		if(i != 0)
			consPrint(" %s", argv[i]);
		else
			consPrint(argv[i]);
	}
	if(!nflag)
		consPrint("\n");

	return 1;
}

static int
cmdBind(int argc, char* argv[])
{
	ulong flag = 0;
	char *usage;

	usage = "usage: bind [-b|-a|-c|-bc|-ac] new old";

	ARGBEGIN{
	case 'a':
		flag |= MAFTER;
		break;
	case 'b':
		flag |= MBEFORE;
		break;
	case 'c':
		flag |= MCREATE;
		break;
	default:
		return cliError(usage);
	}ARGEND

	if(argc != 2 || (flag&MAFTER)&&(flag&MBEFORE))
		return cliError(usage);

	if(bind(argv[0], argv[1], flag) < 0){
		/* try to give a less confusing error than the default */
		if(access(argv[0], 0) < 0)
			return cliError("bind: %s: %r", argv[0]);
		else if(access(argv[1], 0) < 0)
			return cliError("bind: %s: %r", argv[1]);
		else
			return cliError("bind %s %s: %r", argv[0], argv[1]);
	}
	return 1;
}

int
cmdInit(void)
{
	cbox.lock = vtLockAlloc();
	cbox.confd[0] = cbox.confd[1] = -1;

	cliAddCmd(".", cmdDot);
	cliAddCmd("9p", cmd9p);
	cliAddCmd("dflag", cmdDflag);
	cliAddCmd("echo", cmdEcho);
	cliAddCmd("bind", cmdBind);

	if(pipe(cbox.confd) < 0)
		return 0;
	if((cbox.con = conAlloc(cbox.confd[1], "console", 0)) == nil){
		close(cbox.confd[0]);
		close(cbox.confd[1]);
		cbox.confd[0] = cbox.confd[1] = -1;
		return 0;

	}
	cbox.con->isconsole = 1;

	return 1;
}
