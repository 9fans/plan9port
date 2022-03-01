#include "std.h"
#include "dat.h"
#include <9pclient.h>

int extrafactotumdir;
int debug;
int trysecstore = 1;
char *factname = "factotum";
char *service = "factotum";
char *owner;
char *authaddr;
void gflag(char*);

void
usage(void)
{
	fprint(2, "usage: factotum [-Dd] [-a authaddr] [-m mtpt] [-s service]\n");
	fprint(2, " or   factotum -g keypattern\n");
	fprint(2, " or   factotum -g 'badkeyattr\\nmsg\\nkeypattern'\n");
	threadexitsall("usage");
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char *argv[])
{
	char *mtpt;
	char err[ERRMAX];

/*	mtpt = "/mnt"; */
	mtpt = nil;
	owner = getuser();
	quotefmtinstall();
	fmtinstall('A', attrfmt);
	fmtinstall('H', encodefmt);
	fmtinstall('N', attrnamefmt);

	if(argc == 3 && strcmp(argv[1], "-g") == 0){
		gflag(argv[2]);
		threadexitsall(nil);
	}

	ARGBEGIN{
	default:
		usage();
	case 'D':
		chatty9p++;
		break;
	case 'a':
		authaddr = EARGF(usage());
		break;
	case 'd':
		debug = 1;
		break;
	case 'g':
		usage();
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		service = EARGF(usage());
		break;
	case 'n':
		trysecstore = 0;
		break;
	case 'x':
		extrafactotumdir = 1;
		break;
	}ARGEND

	if(argc != 0)
		usage();

	if(trysecstore && havesecstore()){
		while(secstorefetch() < 0){
			rerrstr(err, sizeof err);
			if(strcmp(err, "cancel") == 0)
				break;
			fprint(2, "secstorefetch: %r\n");
			fprint(2, "Enter an empty password to quit.\n");
		}
	}

	rfork(RFNOTEG);

	fsinit0();
	threadpostmountsrv(&fs, service, mtpt, MBEFORE);
	threadexits(nil);
}

/*
 *  prompt user for a key.  don't care about memory leaks, runs standalone
 */
static Attr*
promptforkey(int fd, char *params)
{
	char *v;
	Attr *a, *attr;
	char *def;

	attr = _parseattr(params);
	fprint(fd, "!adding key:");
	for(a=attr; a; a=a->next)
		if(a->type != AttrQuery && a->name[0] != '!')
			fprint(fd, " %q=%q", a->name, a->val);
	fprint(fd, "\n");

	for(a=attr; a; a=a->next){
		v = a->name;
		if(a->type != AttrQuery || v[0]=='!')
			continue;
		def = nil;
		if(strcmp(v, "user") == 0)
			def = getuser();
		a->val = readcons(v, def, 0);
		if(a->val == nil)
			sysfatal("user terminated key input");
		a->type = AttrNameval;
	}
	for(a=attr; a; a=a->next){
		v = a->name;
		if(a->type != AttrQuery || v[0]!='!')
			continue;
		def = nil;
		if(strcmp(v+1, "user") == 0)
			def = getuser();
		a->val = readcons(v+1, def, 1);
		if(a->val == nil)
			sysfatal("user terminated key input");
		a->type = AttrNameval;
	}
	fprint(fd, "!\n");
	close(fd);
	return attr;
}

/*
 *  send a key to the mounted factotum
 */
static int
sendkey(Attr *attr)
{
	int rv;
	char buf[8192];
	CFid *fid;

	fid = nsopen("factotum", nil, "ctl", OWRITE);
	if(fid == nil)
		sysfatal("opening factotum/ctl: %r");
	snprint(buf, sizeof buf, "key %A\n", attr);
	rv = fswrite(fid, buf, strlen(buf));
	fsclose(fid);
	return rv;
}

static void
askuser(int fd, char *params)
{
	Attr *attr;

	attr = promptforkey(fd, params);
	if(attr == nil)
		sysfatal("no key supplied");
	if(sendkey(attr) < 0)
		sysfatal("sending key to factotum: %r");
}

void
gflag(char *s)
{
	char *f[4];
	int nf;
	int fd;

	if((fd = open("/dev/tty", ORDWR)) < 0)
		sysfatal("open /dev/tty: %r");

	nf = getfields(s, f, nelem(f), 0, "\n");
	if(nf == 1){	/* needkey or old badkey */
		fprint(fd, "\n");
		askuser(fd, s);
		threadexitsall(nil);
	}
	if(nf == 3){	/* new badkey */
		fprint(fd, "\n");
		fprint(fd, "!replace: %s\n", f[0]);
		fprint(fd, "!because: %s\n", f[1]);
		askuser(fd, f[2]);
		threadexitsall(nil);
	}
	usage();
}
