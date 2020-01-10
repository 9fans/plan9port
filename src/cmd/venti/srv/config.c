#include "stdinc.h"
#include "dat.h"
#include "fns.h"

Index			*mainindex;
int			paranoid = 1;		/* should verify hashes on disk read */

static ArenaPart	*configarenas(char *file);
static ISect		*configisect(char *file);
static Bloom		*configbloom(char *file);

int
initventi(char *file, Config *conf)
{
	statsinit();

	if(file == nil){
		seterr(EOk, "no configuration file");
		return -1;
	}
	if(runconfig(file, conf) < 0){
		seterr(EOk, "can't initialize venti: %r");
		return -1;
	}
	mainindex = initindex(conf->index, conf->sects, conf->nsects);
	if(mainindex == nil)
		return -1;
	mainindex->bloom = conf->bloom;
	return 0;
}

static int
numok(char *s)
{
	char *p;

	strtoull(s, &p, 0);
	if(p == s)
		return -1;
	if(*p == 0)
		return 0;
	if(p[1] == 0 && strchr("MmGgKk", *p))
		return 0;
	return 0;
}

/*
 * configs	:
 *		| configs config
 * config	: "isect" filename
 *		| "arenas" filename
 *		| "index" name
 *		| "bcmem" num
 *		| "mem" num
 *		| "icmem" num
 *		| "queuewrites"
 *		| "httpaddr" address
 *		| "addr" address
 *
 * '#' and \n delimit comments
 */
enum
{
	MaxArgs	= 2
};
int
runconfig(char *file, Config *config)
{
	ArenaPart **av;
	ISect **sv;
	IFile f;
	char *s, *line, *flds[MaxArgs + 1];
	int i, ok;

	if(readifile(&f, file) < 0)
		return -1;
	memset(config, 0, sizeof *config);
	config->mem = Unspecified;
	ok = -1;
	line = nil;
	for(;;){
		s = ifileline(&f);
		if(s == nil){
			ok = 0;
			break;
		}
		line = estrdup(s);
		i = getfields(s, flds, MaxArgs + 1, 1, " \t\r");
		if(i > 0 && strcmp(flds[0], "mgr") == 0){
			/* do nothing */
		}else if(i == 2 && strcmp(flds[0], "isect") == 0){
			sv = MKN(ISect*, config->nsects + 1);
			for(i = 0; i < config->nsects; i++)
				sv[i] = config->sects[i];
			free(config->sects);
			config->sects = sv;
			config->sects[config->nsects] = configisect(flds[1]);
			if(config->sects[config->nsects] == nil)
				break;
			config->nsects++;
		}else if(i == 2 && strcmp(flds[0], "arenas") == 0){
			av = MKN(ArenaPart*, config->naparts + 1);
			for(i = 0; i < config->naparts; i++)
				av[i] = config->aparts[i];
			free(config->aparts);
			config->aparts = av;
			config->aparts[config->naparts] = configarenas(flds[1]);
			if(config->aparts[config->naparts] == nil)
				break;
			config->naparts++;
		}else if(i == 2 && strcmp(flds[0], "bloom") == 0){
			if(config->bloom){
				seterr(EAdmin, "duplicate bloom lines in configuration file %s", file);
				break;
			}
			if((config->bloom = configbloom(flds[1])) == nil)
				break;
		}else if(i == 2 && strcmp(flds[0], "index") == 0){
			if(nameok(flds[1]) < 0){
				seterr(EAdmin, "illegal index name %s in config file %s", flds[1], file);
				break;
			}
			if(config->index != nil){
				seterr(EAdmin, "duplicate indices in config file %s", file);
				break;
			}
			config->index = estrdup(flds[1]);
		}else if(i == 2 && strcmp(flds[0], "bcmem") == 0){
			if(numok(flds[1]) < 0){
				seterr(EAdmin, "illegal size %s in config file %s",
					flds[1], file);
				break;
			}
			if(config->bcmem != 0){
				seterr(EAdmin, "duplicate bcmem lines in config file %s", file);
				break;
			}
			config->bcmem = unittoull(flds[1]);
		}else if(i == 2 && strcmp(flds[0], "mem") == 0){
			if(numok(flds[1]) < 0){
				seterr(EAdmin, "illegal size %s in config file %s",
					flds[1], file);
				break;
			}
			if(config->mem != Unspecified){
				seterr(EAdmin, "duplicate mem lines in config file %s", file);
				break;
			}
			config->mem = unittoull(flds[1]);
		}else if(i == 2 && strcmp(flds[0], "icmem") == 0){
			if(numok(flds[1]) < 0){
				seterr(EAdmin, "illegal size %s in config file %s",
					flds[1], file);
				break;
			}
			if(config->icmem != 0){
				seterr(EAdmin, "duplicate icmem lines in config file %s", file);
				break;
			}
			config->icmem = unittoull(flds[1]);
		}else if(i == 1 && strcmp(flds[0], "queuewrites") == 0){
			config->queuewrites = 1;
		}else if(i == 2 && strcmp(flds[0], "httpaddr") == 0){
			if(config->haddr){
				seterr(EAdmin, "duplicate httpaddr lines in configuration file %s", file);
				break;
			}
			config->haddr = estrdup(flds[1]);
		}else if(i == 2 && strcmp(flds[0], "webroot") == 0){
			if(config->webroot){
				seterr(EAdmin, "duplicate webroot lines in configuration file %s", file);
				break;
			}
			config->webroot = estrdup(flds[1]);
		}else if(i == 2 && strcmp(flds[0], "addr") == 0){
			if(config->vaddr){
				seterr(EAdmin, "duplicate addr lines in configuration file %s", file);
				break;
			}
			config->vaddr = estrdup(flds[1]);
		}else{
			seterr(EAdmin, "illegal line '%s' in configuration file %s", line, file);
			break;
		}
		free(line);
		line = nil;
	}
	free(line);
	freeifile(&f);
	if(ok < 0){
		free(config->sects);
		config->sects = nil;
		free(config->aparts);
		config->aparts = nil;
	}
	return ok;
}

static ISect*
configisect(char *file)
{
	Part *part;
	ISect *is;

	if(0) fprint(2, "configure index section in %s\n", file);

	part = initpart(file, ORDWR|ODIRECT);
	if(part == nil)
		return nil;
	is = initisect(part);
	if(is == nil)
		werrstr("%s: %r", file);
	return is;
}

static ArenaPart*
configarenas(char *file)
{
	ArenaPart *ap;
	Part *part;

	if(0) fprint(2, "configure arenas in %s\n", file);
	part = initpart(file, ORDWR|ODIRECT);
	if(part == nil)
		return nil;
	ap = initarenapart(part);
	if(ap == nil)
		werrstr("%s: %r", file);
	return ap;
}

static Bloom*
configbloom(char *file)
{
	Bloom *b;
	Part *part;

	if(0) fprint(2, "configure bloom in %s\n", file);
	part = initpart(file, ORDWR|ODIRECT);
	if(part == nil)
		return nil;
	b = readbloom(part);
	if(b == nil){
		werrstr("%s: %r", file);
		freepart(part);
	}
	return b;
}

/* for OS X linker, which only resolves functions, not data */
void
needmainindex(void)
{
}
