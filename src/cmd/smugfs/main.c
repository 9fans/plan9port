#include "a.h"

char *keypattern = "";
char *sessid;
Json *userinfo;
int printerrors;

void
usage(void)
{
	fprint(2, "usage: smugfs [-k keypattern] [-m mtpt] [-s srv]\n");
	threadexitsall("usage");
}

void
smuglogin(void)
{
	Json *v;
	char *s;
	UserPasswd *up;

	printerrors = 1;
	up = auth_getuserpasswd(auth_getkey,
		"proto=pass role=client server=smugmug.com "
		"user? !password? %s", keypattern);
	if(up == nil)
		sysfatal("cannot get username/password: %r");

	v = ncsmug("smugmug.login.withPassword",
		"EmailAddress", up->user,
		"Password", up->passwd,
		nil);
	if(v == nil)
		sysfatal("login failed: %r");

	memset(up->user, 'X', strlen(up->user));
	memset(up->passwd, 'X', strlen(up->passwd));
	free(up);

	sessid = jstring(jwalk(v, "Login/Session/id"));
	if(sessid == nil)
		sysfatal("no session id");
	sessid = estrdup(sessid);
	s = jstring(jwalk(v, "Login/User/NickName"));
	if(s == nil)
		sysfatal("no nick name");
	if(nickindex(s) != 0)
		sysfatal("bad nick name");
	userinfo = jincref(jwalk(v, "Login"));
	jclose(v);
	printerrors = 0;
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char **argv)
{
	char *mtpt, *name;

	mtpt = nil;
	name = nil;
	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 'F':
		chattyfuse++;
		break;
	case 'H':
		chattyhttp++;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		name = EARGF(usage());
		break;
	case 'k':
		keypattern = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 0)
		usage();

	if(name == nil && mtpt == nil)
		mtpt = "/n/smug";

	/*
	 * Check twice -- if there is an exited smugfs instance
	 * mounted there, the first access will fail but unmount it.
	 */
	if(mtpt && access(mtpt, AEXIST) < 0 && access(mtpt, AEXIST) < 0)
		sysfatal("mountpoint %s does not exist", mtpt);

	fmtinstall('H', encodefmt);
	fmtinstall('[', encodefmt);  // base-64
	fmtinstall('J', jsonfmt);
	fmtinstall('M', dirmodefmt);
	fmtinstall('T', timefmt);
	fmtinstall('U', urlencodefmt);

	xinit();
	smuglogin();
	threadpostmountsrv(&xsrv, name, mtpt, 0);
	threadexits(nil);
}
