#include "stdinc.h"
#include "dat.h"
#include "fns.h"

Index			*mainindex;
int			paranoid = 1;		/* should verify hashes on disk read */

static ArenaPart	*configarenas(char *file);
static ISect		*configisect(char *file);

int
initventi(char *file)
{
	Config conf;

	fmtinstall('V', vtscorefmt);

	statsinit();

	if(file == nil){
		seterr(EOk, "no configuration file");
		return -1;
	}
	if(runconfig(file, &conf) < 0){
		seterr(EOk, "can't initialize venti: %r");
		return -1;
	}
	mainindex = initindex(conf.index, conf.sects, conf.nsects);
	if(mainindex == nil)
		return -1;
	return 0;
}

/*
 * configs	:
 *		| configs config
 * config	: "isect" filename
 *		| "arenas" filename
 *		| "index" name
 *
 * '#' and \n are comments
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
	config->index = nil;
	config->naparts = 0;
	config->aparts = nil;
	config->nsects = 0;
	config->sects = nil;
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
		if(i == 2 && strcmp(flds[0], "isect") == 0){
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
		}else if(i == 2 && strcmp(flds[0], "index") == 0){
			if(nameok(flds[1]) < 0){
				seterr(EAdmin, "illegal index name %s in config file %s", flds[1], config);
				break;
			}
			if(config->index != nil){
				seterr(EAdmin, "duplicate indices in config file %s", config);
				break;
			}
			config->index = estrdup(flds[1]);
		}else{
			seterr(EAdmin, "illegal line '%s' in configuration file %s", line, config);
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

	fprint(2, "configure index section in %s\n", file);

	part = initpart(file, 0);
	if(part == nil)
		return nil;
	return initisect(part);
}

static ArenaPart*
configarenas(char *file)
{
	Part *part;

	fprint(2, "configure arenas in %s\n", file);
	part = initpart(file, 0);
	if(part == nil)
		return nil;
	return initarenapart(part);
}
