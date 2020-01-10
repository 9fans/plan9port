/* Copyright (c) 2004 Russ Cox */
#include <u.h>
#include <libc.h>
#include <venti.h>
#include <thread.h>
#include <libsec.h>

#ifndef _UNISTD_H_
#pragma varargck type "F" VtFcall*
#pragma varargck type "T" void
#endif

int verbose;

enum
{
	STACK = 8192
};

void
usage(void)
{
	fprint(2, "usage: venti/devnull [-v] [-a address]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	VtReq *r;
	VtSrv *srv;
	char *address;

	fmtinstall('V', vtscorefmt);
	fmtinstall('F', vtfcallfmt);

	address = "tcp!*!venti";

	ARGBEGIN{
	case 'v':
		verbose++;
		break;
	case 'a':
		address = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	srv = vtlisten(address);
	if(srv == nil)
		sysfatal("vtlisten %s: %r", address);

	while((r = vtgetreq(srv)) != nil){
		r->rx.msgtype = r->tx.msgtype+1;
		if(verbose)
			fprint(2, "<- %F\n", &r->tx);
		switch(r->tx.msgtype){
		case VtTping:
			break;
		case VtTgoodbye:
			break;
		case VtTread:
			r->rx.error = vtstrdup("no such block");
			r->rx.msgtype = VtRerror;
			break;
		case VtTwrite:
			packetsha1(r->tx.data, r->rx.score);
			break;
		case VtTsync:
			break;
		}
		if(verbose)
			fprint(2, "-> %F\n", &r->rx);
		vtrespond(r);
	}
	threadexitsall(nil);
}
