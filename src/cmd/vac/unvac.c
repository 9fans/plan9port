#include "stdinc.h"
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include "vac.h"

VacFs *fs;
int tostdout;
int nwant;
char **want;
int *found;
int chatty;
VtConn *conn;
int errors;
int settimes;
int table;

int mtimefmt(Fmt*);
void unvac(VacFile*, char*, VacDir*);

void
usage(void)
{
	fprint(2, "usage: unvac [-TVctv] [-h host] file.vac [file ...]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	int i;
	char *host;
	VacFile *f;

	fmtinstall('H', encodefmt);
	fmtinstall('V', vtscorefmt);
	fmtinstall('F', vtfcallfmt);
	fmtinstall('T', mtimefmt);
	fmtinstall('M', dirmodefmt);
	
	host = nil;
	ARGBEGIN{
	case 'T':
		settimes = 1;
		break;
	case 'V':
		chattyventi = 1;
		break;
	case 'c':
		tostdout++;
		break;
	case 'h':
		host = EARGF(usage());
		break;
	case 't':
		table++;
		break;
	case 'v':
		chatty++;
		break;
	default:
		usage();
	}ARGEND

	if(argc < 1)
		usage();

	conn = vtdial(host);
	if(conn == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(conn) < 0)
		sysfatal("vtconnect: %r");

	fs = vacfsopen(conn, argv[0], VtOREAD, 128);
	if(fs == nil)
		sysfatal("vacfsopen: %r");

	nwant = argc-1;
	want = argv+1;
	found = vtmallocz(nwant*sizeof found[0]);

	if((f = vacfsgetroot(fs)) == nil)
		sysfatal("vacfsgetroot: %r");
	
	unvac(f, nil, nil);
	for(i=0; i<nwant; i++){
		if(want[i] && !found[i]){
			fprint(2, "warning: didn't find %s\n", want[i]);
			errors++;
		}
	}
	if(errors)
		threadexitsall("errors");
	threadexitsall(0);
}

int
writen(int fd, char *buf, int n)
{
	int m;
	int oldn;
	
	oldn = n;
	while(n > 0){
		m = write(fd, buf, n);
		if(m <= 0)
			return -1;
		buf += m;
		n -= m;
	}
	return oldn;
}

int
wantfile(char *name)
{
	int i, namelen, n;
	
	if(nwant == 0)
		return 1;

	namelen = strlen(name);
	for(i=0; i<nwant; i++){
		if(want[i] == nil)
			continue;
		n = strlen(want[i]);
		if(n < namelen && name[n] == '/' && memcmp(name, want[i], n) == 0)
			return 1;
		if(namelen < n && want[i][namelen] == '/' && memcmp(want[i], name, n) == 0)
			return 1;
		if(n == namelen && memcmp(name, want[i], n) == 0){
			found[i] = 1;
			return 1;
		}
	}
	return 0;
}

void
unvac(VacFile *f, char *name, VacDir *vdir)
{
	static char buf[65536];
	int fd, mode, n, mode9;
	char *newname;
	char *what;
	vlong off;
	Dir d, *dp;
	VacDirEnum *vde;
	VacDir newvdir;
	VacFile *newf;

	if(vdir)
		mode = vdir->mode;
	else
		mode = vacfilegetmode(f);

	if(vdir){
		if(table){
			if(chatty){
				mode9 = vdir->mode&0777;
				if(mode&ModeDir)
					mode9 |= DMDIR;
				if(mode&ModeLink)
					mode9 |= DMSYMLINK;
				if(mode&ModeAppend)
					mode9 |= DMAPPEND;
				if(mode&ModeExclusive)
					mode9 |= DMEXCL;
				if(mode&ModeNamedPipe)
					mode9 |= DMNAMEDPIPE;
				if(mode&ModeSetUid)
					mode9 |= DMSETUID;
				if(mode&ModeSetGid)
					mode9 |= DMSETGID;
				if(mode&ModeDevice)
					mode9 |= DMDEVICE;
				print("%M %-10s %-10s %11lld %T %s\n",
					mode9, vdir->uid, vdir->gid, vdir->size,
					vdir->mtime, name);
			}else
				print("%s%s\n", name, (mode&ModeDir) ? "/" : "");
		}
		else if(chatty)
			fprint(2, "%s%s\n", name, (mode&ModeDir) ? "/" : "");
	}

	if(mode&(ModeDevice|ModeLink|ModeNamedPipe|ModeExclusive)){
		if(table)
			return;
		if(mode&ModeDevice)
			what = "device";
		else if(mode&ModeLink)
			what = "link";
		else if(mode&ModeNamedPipe)
			what = "named pipe";
		else if(mode&ModeExclusive)
			what = "lock";
		else
			what = "unknown type of file";
		fprint(2, "warning: ignoring %s %s\n", what, name);
		return;
	}
	
	if(mode&ModeDir){
		if((vde = vdeopen(f)) == nil){
			fprint(2, "vdeopen %s: %r", name);
			errors++;
			return;
		}
		if(!table && !tostdout && vdir){
			// create directory
			if((dp = dirstat(name)) == nil){
				if((fd = create(name, OREAD, DMDIR|(mode&0777))) < 0){
					fprint(2, "mkdir %s: %r\n", name);
					vdeclose(vde);
				}
				close(fd);
			}else{
				if(!(dp->mode&DMDIR)){
					fprint(2, "%s already exists and is not a directory\n", name);
					errors++;
					free(dp);
					vdeclose(vde);
					return;
				}
				free(dp);
			}
		}
		while(vderead(vde, &newvdir) > 0){
			if(name == nil)
				newname = newvdir.elem;
			else
				newname = smprint("%s/%s", name, newvdir.elem);
			if(wantfile(newname)){
				if((newf = vacfilewalk(f, newvdir.elem)) == nil){
					fprint(2, "walk %s: %r\n", name);
					errors++;
				}else if(newf == f){
					fprint(2, "walk loop: %s\n", newname);
					vacfiledecref(newf);
				}else{
					unvac(newf, newname, &newvdir);
					vacfiledecref(newf);
				}
			}
			if(newname != newvdir.elem)
				free(newname);
			vdcleanup(&newvdir);
		}
		vdeclose(vde);
	}else{
		if(!table){
			if(tostdout)
				fd = dup(1, -1);
			else if((fd = create(name, OWRITE, mode&0777)) < 0){
				fprint(2, "create %s: %r\n", name);
				errors++;
				return;
			}
			off = 0;
			while((n = vacfileread(f, buf, sizeof buf, off)) > 0){
				if(writen(fd, buf, n) != n){
					fprint(2, "write %s: %r\n", name);
					errors++;
					close(fd);
					remove(name);
					return;
				}
				off += n;
			}
			close(fd);
		}
	}
	if(vdir && settimes && !tostdout){
		nulldir(&d);
		d.mtime = vdir->mtime;
		if(dirwstat(name, &d) < 0)
			fprint(2, "warning: setting mtime on %s: %r", name);
	}
}

int
mtimefmt(Fmt *f)
{
	Tm *tm;
	
	tm = localtime(va_arg(f->args, ulong));
	fmtprint(f, "%04d-%02d-%02d %02d:%02d",
		tm->year+1900, tm->mon+1, tm->mday,
		tm->hour, tm->min);
	return 0;
}
