#include "stdinc.h"

#include "9.h"

typedef struct {
	char*	argv0;
	int	(*cmd)(int, char*[]);
} Cmd;

static struct {
	VtLock*	lock;
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
	vtSetError("%s", p);
	free(p);
	va_end(arg);

	return 0;
}

int
cliExec(char* buf)
{
	int argc, i, r;
	char *argv[20], *p;

	p = vtStrDup(buf);
	if((argc = tokenize(p, argv, nelem(argv)-1)) == 0){
		vtMemFree(p);
		return 1;
	}
	argv[argc] = 0;

	if(argv[0][0] == '#'){
		vtMemFree(p);
		return 1;
	}

	vtLock(cbox.lock);
	for(i = 0; i < cbox.hi; i++){
		if(strcmp(cbox.cmd[i].argv0, argv[0]) == 0){
			vtUnlock(cbox.lock);
			if(!(r = cbox.cmd[i].cmd(argc, argv)))
				consPrint("%s\n", vtGetError());
			vtMemFree(p);
			return r;
		}
	}
	vtUnlock(cbox.lock);

	consPrint("%s: - eh?\n", argv[0]);
	vtMemFree(p);

	return 0;
}

int
cliAddCmd(char* argv0, int (*cmd)(int, char*[]))
{
	int i;
	Cmd *opt;

	vtLock(cbox.lock);
	for(i = 0; i < cbox.hi; i++){
		if(strcmp(argv0, cbox.cmd[i].argv0) == 0){
			vtUnlock(cbox.lock);
			return 0;
		}
	}
	if(i >= cbox.hi){
		if(cbox.hi >= cbox.ncmd){
			cbox.cmd = vtMemRealloc(cbox.cmd,
					(cbox.ncmd+NCmdIncr)*sizeof(Cmd));
			memset(&cbox.cmd[cbox.ncmd], 0, NCmdIncr*sizeof(Cmd));
			cbox.ncmd += NCmdIncr;
		}
	}

	opt = &cbox.cmd[cbox.hi];
	opt->argv0 = argv0;
	opt->cmd = cmd;
	cbox.hi++;
	vtUnlock(cbox.lock);

	return 1;
}

int
cliInit(void)
{
	cbox.lock = vtLockAlloc();
	cbox.cmd = vtMemAllocZ(NCmdIncr*sizeof(Cmd));
	cbox.ncmd = NCmdIncr;
	cbox.hi = 0;

	return 1;
}
