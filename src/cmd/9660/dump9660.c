#include <u.h>
#include <libc.h>
#include <bio.h>
#include <disk.h>
#include <libsec.h>
#include "iso9660.h"

ulong now;
int chatty;
int doabort;
int docolon;
int mk9660;
Conform *map;

static void addprotofile(char *new, char *old, Dir *d, void *a);
void usage(void);

char *argv0;

void
usage(void)
{
	if(mk9660)
		fprint(2, "usage: mk9660 [-D:] [-9cjr] [-b bootfile] [-p proto] [-s src] cdimage\n");
	else
		fprint(2, "usage: dump9660 [-D:] [-9cjr] [-m maxsize] [-n now] [-p proto] [-s src] cdimage\n");
	exits("usage");
}

int
main(int argc, char **argv)
{
	int fix;
	char buf[256], *dumpname, *proto, *s, *src, *status;
	ulong block, length, newnull, cblock, clength, maxsize;
	Cdimg *cd;
	Cdinfo info;
	XDir dir;
	Direc *iconform, idumproot, iroot, *jconform, jdumproot, jroot, *r;
	Dump *dump;

	fix = 0;
	status = nil;
	memset(&info, 0, sizeof info);
	proto = unsharp("#9/proto/allproto");
	src = "./";

	info.volumename = atom("9CD");
	info.volumeset = atom("9VolumeSet");
	info.publisher = atom("9Publisher");
	info.preparer = atom("dump9660");
	info.application = atom("dump9660");
	info.flags = CDdump;
	maxsize = 0;
	mk9660 = 0;
	fmtinstall('H', encodefmt);

	ARGBEGIN{
	case 'D':
		chatty++;
		break;
	case 'M':
		mk9660 = 1;
		argv0 = "disk/mk9660";
		info.flags &= ~CDdump;
		break;
	case '9':
		info.flags |= CDplan9;
		break;
	case ':':
		docolon = 1;
		break;
	case 'a':
		doabort = 1;
		break;
	case 'b':
		if(!mk9660)
			usage();
		info.flags |= CDbootable;
		info.bootimage = EARGF(usage());
		break;
	case 'c':
		info.flags |= CDconform;
		break;
	case 'f':
		fix = 1;
		break;
	case 'j':
		info.flags |= CDjoliet;
		break;
	case 'n':
		now = atoi(EARGF(usage()));
		break;
	case 'm':
		maxsize = strtoul(EARGF(usage()), 0, 0);
		break;
	case 'p':
		proto = EARGF(usage());
		break;
	case 'r':
		info.flags |= CDrockridge;
		break;
	case 's':
		src = EARGF(usage());
		break;
	case 'v':
		info.volumename = atom(EARGF(usage()));
		break;
	default:
		usage();
	}ARGEND

	if(mk9660 && (fix || now || maxsize))
		usage();

	if(argc != 1)
		usage();

	if(now == 0)
		now = (ulong)time(0);
	if(mk9660){
		if((cd = createcd(argv[0], info)) == nil)
			sysfatal("cannot create '%s': %r", argv[0]);
	}else{
		if((cd = opencd(argv[0], info)) == nil)
			sysfatal("cannot open '%s': %r", argv[0]);
		if(!(cd->flags & CDdump))
			sysfatal("not a dump cd");
	}

	/* create ISO9660/Plan 9 tree in memory */
	memset(&dir, 0, sizeof dir);
	dir.name = atom("");
	dir.uid = atom("sys");
	dir.gid = atom("sys");
	dir.uidno = 0;
	dir.gidno = 0;
	dir.mode = DMDIR | 0755;
	dir.mtime = now;
	dir.atime = now;
	dir.ctime = now;

	mkdirec(&iroot, &dir);
	iroot.srcfile = src;

	/*
	 * Read new files into memory
	 */
	if(rdproto(proto, src, addprotofile, 0, &iroot) < 0)
		sysfatal("rdproto: %r");

	if(mk9660){
		dump = emalloc(sizeof *dump);
		dumpname = nil;
	}else{
		/*
		 * Read current dump tree and _conform.map.
		 */
		idumproot = readdumpdirs(cd, &dir, isostring);
		readdumpconform(cd);
		if(cd->flags & CDjoliet)
			jdumproot = readdumpdirs(cd, &dir, jolietstring);

		if(fix){
			dumpname = nil;
			cd->nextblock = cd->nulldump+1;
			cd->nulldump = 0;
			Cwseek(cd, cd->nextblock*Blocksize);
			goto Dofix;
		}

		dumpname = adddumpdir(&idumproot, now, &dir);
		/* note that we assume all names are conforming and thus sorted */
		if(cd->flags & CDjoliet) {
			s = adddumpdir(&jdumproot, now, &dir);
			if(s != dumpname)
				sysfatal("dumpnames don't match %s %s\n", dumpname, s);
		}
		dump = dumpcd(cd, &idumproot);
		cd->nextblock = cd->nulldump+1;
	}

	/*
	 * Write new files, starting where the dump tree was.
 	 * Must be done before creation of the Joliet tree so that
 	 * blocks and lengths are correct.
	 */
	Cwseek(cd, cd->nextblock*Blocksize);
	writefiles(dump, cd, &iroot);

	if(cd->bootimage){
		findbootimage(cd, &iroot);
		Cupdatebootcat(cd);
	}

	/* create Joliet tree */
	if(cd->flags & CDjoliet)
		copydirec(&jroot, &iroot);

	if(info.flags & CDconform) {
		checknames(&iroot, isbadiso9660);
		convertnames(&iroot, struprcpy);
	} else
		convertnames(&iroot, (char* (*)(char*, char*))strcpy);

/*	isoabstract = findconform(&iroot, abstract); */
/*	isobiblio = findconform(&iroot, biblio); */
/*	isonotice = findconform(&iroot, notice); */

	dsort(&iroot, isocmp);

	if(cd->flags & CDjoliet) {
	/*	jabstract = findconform(&jroot, abstract); */
	/*	jbiblio = findconform(&jroot, biblio); */
	/*	jnotice = findconform(&jroot, notice); */

		checknames(&jroot, isbadjoliet);
		convertnames(&jroot, (char* (*)(char*, char*))strcpy);
		dsort(&jroot, jolietcmp);
	}

	/*
	 * Write directories.
	 */
	writedirs(cd, &iroot, Cputisodir);
	if(cd->flags & CDjoliet)
		writedirs(cd, &jroot, Cputjolietdir);

	if(mk9660){
		cblock = 0;
		clength = 0;
		newnull = 0;
	}else{
		/*
		 * Write incremental _conform.map block.
		 */
		wrconform(cd, cd->nconform, &cblock, &clength);

		/* jump here if we're just fixing up the cd */
Dofix:
		/*
		 * Write null dump header block; everything after this will be
		 * overwritten at the next dump.  Because of this, it needs to be
		 * reconstructable.  We reconstruct the _conform.map and dump trees
		 * from the header blocks in dump.c, and we reconstruct the path
		 * tables by walking the cd.
		 */
		newnull = Cputdumpblock(cd);
	}

	/*
	 * Write _conform.map.
	 */
	dir.mode = 0444;
	if(cd->flags & (CDconform|CDjoliet)) {
		if(!mk9660 && cd->nconform == 0){
			block = cblock;
			length = clength;
		}else
			wrconform(cd, 0, &block, &length);

		if(mk9660)
{
			idumproot = iroot;
			jdumproot = jroot;
		}
		if(length) {
			/* The ISO9660 name will get turned into uppercase when written. */
			if((iconform = walkdirec(&idumproot, "_conform.map")) == nil)
				iconform = adddirec(&idumproot, "_conform.map", &dir);
			jconform = nil;
			if(cd->flags & CDjoliet) {
				if((jconform = walkdirec(&jdumproot, "_conform.map")) == nil)
					jconform = adddirec(&jdumproot, "_conform.map", &dir);
			}
			iconform->block = block;
			iconform->length = length;
			if(cd->flags & CDjoliet) {
				jconform->block = block;
				jconform->length = length;
			}
		}
		if(mk9660) {
			iroot = idumproot;
			jroot = jdumproot;
		}
	}

	if(mk9660){
		/*
		 * Patch in root directories.
		 */
		setroot(cd, cd->iso9660pvd, iroot.block, iroot.length);
		setvolsize(cd, cd->iso9660pvd, cd->nextblock*Blocksize);
		if(cd->flags & CDjoliet){
			setroot(cd, cd->jolietsvd, jroot.block, jroot.length);
			setvolsize(cd, cd->jolietsvd, cd->nextblock*Blocksize);
		}
	}else{
		/*
		 * Write dump tree at end.  We assume the name characters
		 * are all conforming, so everything is already sorted properly.
		 */
		convertnames(&idumproot, (info.flags & CDconform) ? struprcpy : (char* (*)(char*, char*)) strcpy);
		if(cd->nulldump) {
			r = walkdirec(&idumproot, dumpname);
			assert(r != nil);
			copybutname(r, &iroot);
		}
		if(cd->flags & CDjoliet) {
			convertnames(&jdumproot, (char* (*)(char*, char*))strcpy);
			if(cd->nulldump) {
				r = walkdirec(&jdumproot, dumpname);
				assert(r != nil);
				copybutname(r, &jroot);
			}
		}

		writedumpdirs(cd, &idumproot, Cputisodir);
		if(cd->flags & CDjoliet)
			writedumpdirs(cd, &jdumproot, Cputjolietdir);

		/*
		 * Patch in new root directory entry.
		 */
		setroot(cd, cd->iso9660pvd, idumproot.block, idumproot.length);
		setvolsize(cd, cd->iso9660pvd, cd->nextblock*Blocksize);
		if(cd->flags & CDjoliet){
			setroot(cd, cd->jolietsvd, jdumproot.block, jdumproot.length);
			setvolsize(cd, cd->jolietsvd, cd->nextblock*Blocksize);
		}
	}
	writepathtables(cd);

	if(!mk9660){
		/*
		 * If we've gotten too big, truncate back to what we started with,
		 * fix up the cd, and exit with a non-zero status.
		 */
		Cwflush(cd);
		if(cd->nulldump && maxsize && Cwoffset(cd) > maxsize){
			fprint(2, "too big; writing old tree back\n");
			status = "cd too big; aborted";

			rmdumpdir(&idumproot, dumpname);
			rmdumpdir(&jdumproot, dumpname);

			cd->nextblock = cd->nulldump+1;
			cd->nulldump = 0;
			Cwseek(cd, cd->nextblock*Blocksize);
			goto Dofix;
		}

		/*
		 * Write old null header block; this commits all our changes.
		 */
		if(cd->nulldump){
			Cwseek(cd, cd->nulldump*Blocksize);
			sprint(buf, "plan 9 dump cd\n");
			sprint(buf+strlen(buf), "%s %lud %lud %lud %lud %lud %lud",
				dumpname, now, newnull, cblock, clength, iroot.block,
				iroot.length);
			if(cd->flags & CDjoliet)
				sprint(buf+strlen(buf), " %lud %lud",
					jroot.block, jroot.length);
			strcat(buf, "\n");
			Cwrite(cd, buf, strlen(buf));
			Cpadblock(cd);
			Cwflush(cd);
		}
	}
	fdtruncate(cd->fd, cd->nextblock*Blocksize);
	exits(status);
	return 0;
}

static void
addprotofile(char *new, char *old, Dir *d, void *a)
{
	char *name, *p;
	Direc *direc;
	XDir xd;

	dirtoxdir(&xd, d);
	name = nil;
	if(docolon && strchr(new, ':')) {
		name = emalloc(strlen(new)+1);
		strcpy(name, new);
		while((p=strchr(name, ':')))
			*p=' ';
		new = name;
	}
	if((direc = adddirec((Direc*)a, new, &xd))) {
		direc->srcfile = atom(old);

		/* BUG: abstract, biblio, notice */
	}
	if(name)
		free(name);

}
