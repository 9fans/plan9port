#include <u.h>
#include <libc.h>
#include <bio.h>

#define TBLOCK	512
#define NBLOCK	40	/* maximum blocksize */
#define DBLOCK	20	/* default blocksize */
#define NAMSIZ	100
union	hblock
{
	char	dummy[TBLOCK];
	struct	header
	{
		char	name[NAMSIZ];
		char	mode[8];
		char	uid[8];
		char	gid[8];
		char	size[12];
		char	mtime[12];
		char	chksum[8];
		char	linkflag;
		char	linkname[NAMSIZ];
	} dbuf;
} dblock, tbuf[NBLOCK];

Dir *stbuf;
Biobuf bout;

int	rflag, xflag, vflag, tflag, mt, cflag, fflag, Tflag, Rflag;
int	uflag, gflag;
int	chksum, recno, first;
int	nblock = DBLOCK;

void	usage(void);
void	dorep(char **);
int	endtar(void);
void	getdir(void);
void	passtar(void);
void	putfile(char*, char *, char *);
void	doxtract(char **);
void	dotable(void);
void	putempty(void);
void	longt(Dir *);
int	checkdir(char *, int, Qid*);
void	tomodes(Dir *);
int	checksum(void);
int	checkupdate(char *);
int	prefix(char *, char *);
int	readtar(char *);
int	writetar(char *);
void	backtar(void);
void	flushtar(void);
void	affix(int, char *);
int	volprompt(void);
void
main(int argc, char **argv)
{
	char *usefile;
	char *cp, *ap;

	if (argc < 2)
		usage();

	Binit(&bout, 1, OWRITE);
	usefile =  0;
	argv[argc] = 0;
	argv++;
	for (cp = *argv++; *cp; cp++) 
		switch(*cp) {
		case 'f':
			usefile = *argv++;
			if(!usefile)
				usage();
			fflag++;
			break;
		case 'u':
			ap = *argv++;
			if(!ap)
				usage();
			uflag = strtoul(ap, 0, 0);
			break;
		case 'g':
			ap = *argv++;
			if(!ap)
				usage();
			gflag = strtoul(ap, 0, 0);
			break;
		case 'c':
			cflag++;
			rflag++;
			break;
		case 'r':
			rflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'x':
			xflag++;
			break;
		case 'T':
			Tflag++;
			break;
		case 't':
			tflag++;
			break;
		case 'R':
			Rflag++;
			break;
		case '-':
			break;
		default:
			fprint(2, "tar: %c: unknown option\n", *cp);
			usage();
		}

	fmtinstall('M', dirmodefmt);

	if (rflag) {
		if (!usefile) {
			if (cflag == 0) {
				fprint(2, "tar: can only create standard output archives\n");
				exits("arg error");
			}
			mt = dup(1, -1);
			nblock = 1;
		}
		else if ((mt = open(usefile, ORDWR)) < 0) {
			if (cflag == 0 || (mt = create(usefile, OWRITE, 0666)) < 0) {
				fprint(2, "tar: cannot open %s: %r\n", usefile);
				exits("open");
			}
		}
		dorep(argv);
	}
	else if (xflag)  {
		if (!usefile) {
			mt = dup(0, -1);
			nblock = 1;
		}
		else if ((mt = open(usefile, OREAD)) < 0) {
			fprint(2, "tar: cannot open %s: %r\n", usefile);
			exits("open");
		}
		doxtract(argv);
	}
	else if (tflag) {
		if (!usefile) {
			mt = dup(0, -1);
			nblock = 1;
		}
		else if ((mt = open(usefile, OREAD)) < 0) {
			fprint(2, "tar: cannot open %s: %r\n", usefile);
			exits("open");
		}
		dotable();
	}
	else
		usage();
	exits(0);
}

void
usage(void)
{
	fprint(2, "tar: usage  tar {txrc}[Rvf] [tarfile] file1 file2...\n");
	exits("usage");
}

void
dorep(char **argv)
{
	char cwdbuf[2048], *cwd, thisdir[2048];
	char *cp, *cp2;
	int cd;

	if (getwd(cwdbuf, sizeof(cwdbuf)) == 0) {
		fprint(2, "tar: can't find current directory: %r\n");
		exits("cwd");
	}
	cwd = cwdbuf;

	if (!cflag) {
		getdir();
		do {
			passtar();
			getdir();
		} while (!endtar());
	}

	while (*argv) {
		cp2 = *argv;
		if (!strcmp(cp2, "-C") && argv[1]) {
			argv++;
			if (chdir(*argv) < 0)
				perror(*argv);
			cwd = *argv;
			argv++;
			continue;
		}
		cd = 0;
		for (cp = *argv; *cp; cp++)
			if (*cp == '/')
				cp2 = cp;
		if (cp2 != *argv) {
			*cp2 = '\0';
			chdir(*argv);
			if(**argv == '/')
				strncpy(thisdir, *argv, sizeof(thisdir));
			else
				snprint(thisdir, sizeof(thisdir), "%s/%s", cwd, *argv);
			*cp2 = '/';
			cp2++;
			cd = 1;
		} else
			strncpy(thisdir, cwd, sizeof(thisdir));
		putfile(thisdir, *argv++, cp2);
		if(cd && chdir(cwd) < 0) {
			fprint(2, "tar: can't cd back to %s: %r\n", cwd);
			exits("cwd");
		}
	}
	putempty();
	putempty();
	flushtar();
}

int
endtar(void)
{
	if (dblock.dbuf.name[0] == '\0') {
		backtar();
		return(1);
	}
	else
		return(0);
}

void
getdir(void)
{
	Dir *sp;

	readtar((char*)&dblock);
	if (dblock.dbuf.name[0] == '\0')
		return;
	if(stbuf == nil){
		stbuf = malloc(sizeof(Dir));
		if(stbuf == nil) {
			fprint(2, "tar: can't malloc: %r\n");
			exits("malloc");
		}
	}
	sp = stbuf;
	sp->mode = strtol(dblock.dbuf.mode, 0, 8);
	sp->uid = "adm";
	sp->gid = "adm";
	sp->length = strtol(dblock.dbuf.size, 0, 8);
	sp->mtime = strtol(dblock.dbuf.mtime, 0, 8);
	chksum = strtol(dblock.dbuf.chksum, 0, 8);
	if (chksum != checksum()) {
		fprint(2, "directory checksum error\n");
		exits("checksum error");
	}
	sp->qid.type = 0;
	/* the mode test is ugly but sometimes necessary */
	if (dblock.dbuf.linkflag == '5' || (sp->mode&0170000) == 040000) {
		sp->qid.type |= QTDIR;
		sp->mode |= DMDIR;
	}
}

void
passtar(void)
{
	long blocks;
	char buf[TBLOCK];

	if (dblock.dbuf.linkflag == '1' || dblock.dbuf.linkflag == 's')
		return;
	blocks = stbuf->length;
	blocks += TBLOCK-1;
	blocks /= TBLOCK;

	while (blocks-- > 0)
		readtar(buf);
}

void
putfile(char *dir, char *longname, char *sname)
{
	int infile;
	long blocks;
	char buf[TBLOCK];
	char curdir[4096];
	char shortname[4096];
	char *cp, *cp2;
	Dir *db;
	int i, n;

	if(strlen(sname) > sizeof shortname - 3){
		fprint(2, "tar: %s: name too long (max %d)\n", sname, sizeof shortname - 3);
		return;
	}
	
	snprint(shortname, sizeof shortname, "./%s", sname);
	infile = open(shortname, OREAD);
	if (infile < 0) {
		fprint(2, "tar: %s: cannot open file - %r\n", longname);
		return;
	}

	if(stbuf != nil)
		free(stbuf);
	stbuf = dirfstat(infile);

	if (stbuf->qid.type & QTDIR) {
		/* Directory */
		for (i = 0, cp = buf; *cp++ = longname[i++];);
		*--cp = '/';
		*++cp = 0;
		if( (cp - buf) >= NAMSIZ) {
			fprint(2, "tar: %s: file name too long\n", longname);
			close(infile);
			return;
		}
		stbuf->length = 0;
		tomodes(stbuf);
		strcpy(dblock.dbuf.name,buf);
		dblock.dbuf.linkflag = '5';		/* Directory */
		sprint(dblock.dbuf.chksum, "%6o", checksum());
		writetar( (char *) &dblock);
		if (chdir(shortname) < 0) {
			fprint(2, "tar: can't cd to %s: %r\n", shortname);
			snprint(curdir, sizeof(curdir), "cd %s", shortname);
			exits(curdir);
		}
		sprint(curdir, "%s/%s", dir, sname);
		while ((n = dirread(infile, &db)) > 0) {
			for(i = 0; i < n; i++){
				strncpy(cp, db[i].name, sizeof buf - (cp-buf));
				putfile(curdir, buf, db[i].name);
			}free(db);
		}
		close(infile);
		if (chdir(dir) < 0 && chdir("..") < 0) {
			fprint(2, "tar: can't cd to ..(%s): %r\n", dir);
			snprint(curdir, sizeof(curdir), "cd ..(%s)", dir);
			exits(curdir);
		}
		return;
	}


	tomodes(stbuf);

	cp2 = longname;
	for (cp = dblock.dbuf.name, i=0; (*cp++ = *cp2++) && i < NAMSIZ; i++);
	if (i >= NAMSIZ) {
		fprint(2, "%s: file name too long\n", longname);
		close(infile);
		return;
	}

	blocks = (stbuf->length + (TBLOCK-1)) / TBLOCK;
	if (vflag) {
		fprint(2, "a %s ", longname);
		fprint(2, "%ld blocks\n", blocks);
	}
	dblock.dbuf.linkflag = 0;			/* Regular file */
	sprint(dblock.dbuf.chksum, "%6o", checksum());
	writetar( (char *) &dblock);

	while ((i = readn(infile, buf, TBLOCK)) > 0 && blocks > 0) {
		writetar(buf);
		blocks--;
	}
	close(infile);
	if (blocks != 0 || i != 0)
		fprint(2, "%s: file changed size\n", longname);
	while (blocks-- >  0)
		putempty();
}


void
doxtract(char **argv)
{
	Dir null;
	long blocks, bytes;
	char buf[TBLOCK], outname[NAMSIZ+4];
	char **cp;
	int ofile;

	for (;;) {
		getdir();
		if (endtar())
			break;

		if (*argv == 0)
			goto gotit;

		for (cp = argv; *cp; cp++)
			if (prefix(*cp, dblock.dbuf.name))
				goto gotit;
		passtar();
		continue;

gotit:
		if(checkdir(dblock.dbuf.name, stbuf->mode, &(stbuf->qid)))
			continue;

		if (dblock.dbuf.linkflag == '1') {
			fprint(2, "tar: can't link %s %s\n",
				dblock.dbuf.linkname, dblock.dbuf.name);
			remove(dblock.dbuf.name);
			continue;
		}
		if (dblock.dbuf.linkflag == 's') {
			fprint(2, "tar: %s: cannot symlink\n", dblock.dbuf.name);
			continue;
		}
		if(dblock.dbuf.name[0] != '/' || Rflag)
			sprint(outname, "./%s", dblock.dbuf.name);
		else
			strcpy(outname, dblock.dbuf.name);
		if ((ofile = create(outname, OWRITE, stbuf->mode & 0777)) < 0) {
			fprint(2, "tar: %s - cannot create: %r\n", outname);
			passtar();
			continue;
		}

		blocks = ((bytes = stbuf->length) + TBLOCK-1)/TBLOCK;
		if (vflag)
			fprint(2, "x %s, %ld bytes\n",
				dblock.dbuf.name, bytes);
		while (blocks-- > 0) {
			readtar(buf);
			if (bytes > TBLOCK) {
				if (write(ofile, buf, TBLOCK) < 0) {
					fprint(2, "tar: %s: HELP - extract write error: %r\n", dblock.dbuf.name);
					exits("extract write");
				}
			} else
				if (write(ofile, buf, bytes) < 0) {
					fprint(2, "tar: %s: HELP - extract write error: %r\n", dblock.dbuf.name);
					exits("extract write");
				}
			bytes -= TBLOCK;
		}
		if(Tflag){
			nulldir(&null);
			null.mtime = stbuf->mtime;
			dirfwstat(ofile, &null);
		}
		close(ofile);
	}
}

void
dotable(void)
{
	for (;;) {
		getdir();
		if (endtar())
			break;
		if (vflag)
			longt(stbuf);
		Bprint(&bout, "%s", dblock.dbuf.name);
		if (dblock.dbuf.linkflag == '1')
			Bprint(&bout, " linked to %s", dblock.dbuf.linkname);
		if (dblock.dbuf.linkflag == 's')
			Bprint(&bout, " -> %s", dblock.dbuf.linkname);
		Bprint(&bout, "\n");
		passtar();
	}
}

void
putempty(void)
{
	char buf[TBLOCK];

	memset(buf, 0, TBLOCK);
	writetar(buf);
}

void
longt(Dir *st)
{
	char *cp;

	Bprint(&bout, "%M %4d/%1d ", st->mode, 0, 0);	/* 0/0 uid/gid */
	Bprint(&bout, "%8lld", st->length);
	cp = ctime(st->mtime);
	Bprint(&bout, " %-12.12s %-4.4s ", cp+4, cp+24);
}

int
checkdir(char *name, int mode, Qid *qid)
{
	char *cp;
	int f;
	Dir *d, null;

	if(Rflag && *name == '/')
		name++;
	cp = name;
	if(*cp == '/')
		cp++;
	for (; *cp; cp++) {
		if (*cp == '/') {
			*cp = '\0';
			if (access(name, 0) < 0) {
				f = create(name, OREAD, DMDIR + 0775L);
				if(f < 0)
					fprint(2, "tar: mkdir %s failed: %r\n", name);
				close(f);
			}
			*cp = '/';
		}
	}

	/* if this is a directory, chmod it to the mode in the tar plus 700 */
	if(cp[-1] == '/' || (qid->type&QTDIR)){
		if((d=dirstat(name)) != 0){
			nulldir(&null);
			null.mode = DMDIR | (mode & 0777) | 0700;
			dirwstat(name, &null);
			free(d);
		}
		return 1;
	} else
		return 0;
}

void
tomodes(Dir *sp)
{
	char *cp;

	for (cp = dblock.dummy; cp < &dblock.dummy[TBLOCK]; cp++)
		*cp = '\0';
	sprint(dblock.dbuf.mode, "%6lo ", sp->mode & 0777);
	sprint(dblock.dbuf.uid, "%6o ", uflag);
	sprint(dblock.dbuf.gid, "%6o ", gflag);
	sprint(dblock.dbuf.size, "%11llo ", sp->length);
	sprint(dblock.dbuf.mtime, "%11lo ", sp->mtime);
}

int
checksum(void)
{
	int i;
	char *cp;

	for (cp = dblock.dbuf.chksum; cp < &dblock.dbuf.chksum[sizeof(dblock.dbuf.chksum)]; cp++)
		*cp = ' ';
	i = 0;
	for (cp = dblock.dummy; cp < &dblock.dummy[TBLOCK]; cp++)
		i += *cp & 0xff;
	return(i);
}

int
prefix(char *s1, char *s2)
{
	while (*s1)
		if (*s1++ != *s2++)
			return(0);
	if (*s2)
		return(*s2 == '/');
	return(1);
}

int
readtar(char *buffer)
{
	int i;

	if (recno >= nblock || first == 0) {
		if ((i = readn(mt, tbuf, TBLOCK*nblock)) <= 0) {
			fprint(2, "tar: archive read error: %r\n");
			exits("archive read");
		}
		if (first == 0) {
			if ((i % TBLOCK) != 0) {
				fprint(2, "tar: archive blocksize error: %r\n");
				exits("blocksize");
			}
			i /= TBLOCK;
			if (i != nblock) {
				fprint(2, "tar: blocksize = %d\n", i);
				nblock = i;
			}
		}
		recno = 0;
	}
	first = 1;
	memmove(buffer, &tbuf[recno++], TBLOCK);
	return(TBLOCK);
}

int
writetar(char *buffer)
{
	first = 1;
	if (recno >= nblock) {
		if (write(mt, tbuf, TBLOCK*nblock) != TBLOCK*nblock) {
			fprint(2, "tar: archive write error: %r\n");
			exits("write");
		}
		recno = 0;
	}
	memmove(&tbuf[recno++], buffer, TBLOCK);
	if (recno >= nblock) {
		if (write(mt, tbuf, TBLOCK*nblock) != TBLOCK*nblock) {
			fprint(2, "tar: archive write error: %r\n");
			exits("write");
		}
		recno = 0;
	}
	return(TBLOCK);
}

/*
 * backup over last tar block
 */
void
backtar(void)
{
	seek(mt, -TBLOCK*nblock, 1);
	recno--;
}

void
flushtar(void)
{
	write(mt, tbuf, TBLOCK*nblock);
}
