#include "common.h"

char *_MAILROOT =	"#9/mail";
char *_UPASLOG =		"#9/sys/log";
char *_UPASLIB = 	"#9/mail/lib";
char *_UPASBIN=		"#9/bin/upas";
char *_UPASTMP = 	"/var/tmp";
char *_SHELL = 		"#9/bin/rc";
char *_POST =		"#9/sys/lib/post/dispatch";

int MBOXMODE = 0662;

void
upasconfig(void)
{
	static int did;

	if(did)
		return;
	did = 1;
	_MAILROOT = unsharp(_MAILROOT);
	_UPASLOG = unsharp(_UPASLOG);
	_UPASLIB = unsharp(_UPASLIB);
	_UPASBIN = unsharp(_UPASBIN);
	_SHELL = unsharp(_SHELL);
	_POST = unsharp(_POST);
}
