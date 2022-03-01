/*
TO DO

can get disposition info out of imap extended structure if needed
sizes in stat/ls ?
translate character sets in =? subjects

fetch headers, bodies on demand

cache headers, bodies on disk

cache message information on disk across runs

body.jpg

*/

#include "a.h"

Imap *imap;

void
usage(void)
{
	fprint(2, "usage: mailfs [-DVtx] [-m mtpt] [-s srvname] [-r root] [-u user] server\n");
	threadexitsall("usage");
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char **argv)
{
	char *server, *srvname, *root, *user;
	int mode;
	char *mtpt;

	srvname = "mail";
	root = "";
	mode = Unencrypted;
	mtpt = nil;
	user = nil;
	ARGBEGIN{
	default:
		usage();
	case 'D':
		chatty9p++;
		break;
	case 'V':
		chattyimap++;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		srvname = EARGF(usage());
		break;
	case 't':
		mode = Tls;
		break;
	case 'u':
		user = EARGF(usage());
		break;
	case 'x':
		mode = Cmd;
		break;
	case 'r':
		root = EARGF(usage());
		break;
	}ARGEND

	quotefmtinstall();
	fmtinstall('$', sxfmt);

	if(argc != 1)
		usage();
	server = argv[0];

	mailthreadinit();
	boxinit();
	fsinit0();

	if((imap = imapconnect(server, mode, root, user)) == nil)
		sysfatal("imapconnect: %r");
	threadpostmountsrv(&fs, srvname, mtpt, 0);
}
