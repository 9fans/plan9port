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
	fprint(2, "usage: mailfs [-t] server\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	char *server;
	int mode;

	mode = Unencrypted;
	ARGBEGIN{
	default:
		usage();
	case 'D':
		chatty9p++;
		break;
	case 'V':
		chattyimap++;
		break;
	case 't':
		mode = Tls;
		break;
	case 'x':
		mode = Cmd;
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

	if((imap = imapconnect(server, mode)) == nil)
		sysfatal("imapconnect: %r");
	threadpostmountsrv(&fs, "mail", nil, 0);
}

