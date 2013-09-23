#include "stdinc.h"

#include "9.h"

typedef struct {
	char*	argv0;
	int	(*cmd)(int, char*[]);
} Cmd;

static struct {
	QLock	lock;
	Cmd*	cmd;
	int	ncmd;
	int	hi;
} cbox;

enum {
	NCmdIncr	= 20,
};

int
cliError(char* fmt, ...)
{
	char *p;
	va_list arg;

	va_start(arg, fmt);
	p = vsmprint(fmt, arg);
	werrstr("%s", p);
	free(p);
	va_end(arg);

	return 0;
}

int
cliExec(char* buf)
{
	int argc, i, r;
	char *argv[20], *p;

	p = vtstrdup(buf);
	if((argc = tokenize(p, argv, nelem(argv)-1)) == 0){
		vtfree(p);
		return 1;
	}
	argv[argc] = 0;

	if(argv[0][0] == '#'){
		vtfree(p);
		return 1;
	}

	qlock(&cbox.lock);
	for(i = 0; i < cbox.hi; i++){
		if(strcmp(cbox.cmd[i].argv0, argv[0]) == 0){
			qunlock(&cbox.lock);
			if(!(r = cbox.cmd[i].cmd(argc, argv)))
				consPrint("%r\n");
			vtfree(p);
			return r;
		}
	}
	qunlock(&cbox.lock);

	consPrint("%s: - eh?\n", argv[0]);
	vtfree(p);

	return 0;
}

int
cliAddCmd(char* argv0, int (*cmd)(int, char*[]))
{
	int i;
	Cmd *opt;

	qlock(&cbox.lock);
	for(i = 0; i < cbox.hi; i++){
		if(strcmp(argv0, cbox.cmd[i].argv0) == 0){
			qunlock(&cbox.lock);
			return 0;
		}
	}
	if(i >= cbox.hi){
		if(cbox.hi >= cbox.ncmd){
			cbox.cmd = vtrealloc(cbox.cmd,
					(cbox.ncmd+NCmdIncr)*sizeof(Cmd));
			memset(&cbox.cmd[cbox.ncmd], 0, NCmdIncr*sizeof(Cmd));
			cbox.ncmd += NCmdIncr;
		}
	}

	opt = &cbox.cmd[cbox.hi];
	opt->argv0 = argv0;
	opt->cmd = cmd;
	cbox.hi++;
	qunlock(&cbox.lock);

	return 1;
}

int
cliInit(void)
{
	cbox.cmd = vtmallocz(NCmdIncr*sizeof(Cmd));
	cbox.ncmd = NCmdIncr;
	cbox.hi = 0;

	return 1;
}
