#include <u.h>
#include <libc.h>
#include <mach.h>

Fhdr *symhdr;
Fhdr *corhdr;
char *symfil;
char *corfil;
int corpid;
Regs *correg;
Map *symmap;
Map *cormap;

int
alldigs(char *s)
{
	while(*s){
		if(*s<'0' || '9'<*s)
			return 0;
		s++;
	}
	return 1;
}

/*
 * attach to arguments in argc, argv
 */
int
attachargs(int argc, char **argv, int omode)
{
	int i;
	Fhdr *hdr;
	char *s;

	symhdr = nil;
	corhdr = nil;
	symfil = nil;
	corfil = nil;
	corpid = 0;
	correg = nil;

	for(i=0; i<argc; i++){
		if(alldigs(argv[i])){
			if(corpid){
				fprint(2, "already have corpid %d; ignoring corpid %d\n", corpid, argv[i]);
				continue;
			}
			if(corhdr){
				fprint(2, "already have core %s; ignoring corpid %d\n", corfil, corpid);
				continue;
			}
			corpid = atoi(argv[i]);
			continue;
		}
		if((hdr = crackhdr(argv[i], omode)) == nil){
			fprint(2, "crackhdr %s: %r\n", argv[i]);
			continue;
		}
		fprint(2, "%s: %s %s %s\n", argv[i], hdr->aname, hdr->mname, hdr->fname);
		if(hdr->ftype == FCORE){
			if(corpid){
				fprint(2, "already have corpid %d; ignoring core %s\n", corpid, argv[i]);
				uncrackhdr(hdr);
				continue;
			}
			if(corhdr){
				fprint(2, "already have core %s; ignoring core %s\n", corfil, argv[i]);
				uncrackhdr(hdr);
				continue;
			}
			corhdr = hdr;
			corfil = argv[i];
		}else{
			if(symhdr){
				fprint(2, "already have text %s; ignoring text %s\n", symfil, argv[i]);
				uncrackhdr(hdr);
				continue;
			}
			symhdr = hdr;
			symfil = argv[i];
		}
	}

	if(symhdr == nil){
		symfil = "a.out";	/* default */
		if(corpid){	/* try from corpid */
			if((s = proctextfile(corpid)) != nil){
				fprint(2, "corpid %d: text %s\n", corpid, s);
				symfil = s;
			}
		}
		if(corhdr){	/* try from core */
			if(corhdr->txtfil != nil){
				fprint(2, "core %s: text %s\n", corfil, corhdr->txtfil);
				symfil = corhdr->txtfil;
			}
		}
		if((symhdr = crackhdr(symfil, omode)) == nil){
			fprint(2, "crackhdr %s: %r\n", symfil);
			symfil = nil;
		}
	}

	if(symhdr)
		syminit(symhdr);

	if(!mach)
		mach = machcpu;

	/*
	 * Set up maps
	 */
	symmap = allocmap();
	cormap = allocmap();
	if(symmap == nil || cormap == nil)
		sysfatal("allocating maps: %r");

	if(symhdr){
		if(mapfile(symhdr, 0, symmap, nil) < 0)
			fprint(2, "mapfile %s: %r\n", symfil);
		mapfile(symhdr, 0, cormap, nil);
	}

	if(corpid)	
		attachproc(corpid);

	if(corhdr)
		attachcore(corhdr);

	return 0;
}

static int thecorpid;
static Fhdr *thecorhdr;

static void
unattach(void)
{
	unmapproc(cormap);
	unmapfile(corhdr, cormap);
	free(correg);
	correg = nil;
	thecorpid = 0;
	thecorhdr = nil;
	corpid = 0;
	corhdr = nil;
	corfil = nil;
}

int
attachproc(int pid)
{
	unattach();
	if(pid == 0)
		return 0;
	if(mapproc(pid, cormap, &correg) < 0){
		fprint(2, "attachproc %d: %r\n", pid);
		return -1;
	}
	thecorpid = pid;
	corpid = pid;
	return 0;
}

int
attachcore(Fhdr *hdr)
{
	unattach();
	if(corhdr == nil)
		return 0;
	if(mapfile(hdr, 0, cormap, &correg) < 0){
		fprint(2, "attachcore %s: %r\n", hdr->filename);
		return -1;
	}
	thecorhdr = hdr;
	corhdr = hdr;
	corfil = hdr->filename;
	return 0;
}

int
attachdynamic(void)
{
	elfdl386mapdl();
	return 0;
}

