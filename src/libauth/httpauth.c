#include <u.h>
#include <libc.h>
#include <auth.h>
#include <authsrv.h>

/* deprecated.
	This is the mechanism that put entries in /sys/lib/httpd.rewrite
	and passwords on the authserver in /sys/lib/httppasswords, which
	was awkward to administer.  Instead, use local .httplogin files,
	which are implemented in sys/src/cmd/ip/httpd/authorize.c */

int
httpauth(char *name, char *password)
{
	int afd;
	Ticketreq tr;
	Ticket	t;
	char key[DESKEYLEN];
	char buf[512];

	afd = authdial(nil, nil);
	if(afd < 0)
		return -1;

	/* send ticket request to AS */
	memset(&tr, 0, sizeof(tr));
	strcpy(tr.uid, name);
	tr.type = AuthHttp;
	convTR2M(&tr, buf);
	if(write(afd, buf, TICKREQLEN) != TICKREQLEN){
		close(afd);
		return -1;
	}
	if(_asrdresp(afd, buf, TICKETLEN) < 0){
		close(afd);
		return -1;
	}
	close(afd);

	/*
	 *  use password and try to decrypt the
	 *  ticket.  If it doesn't work we've got a bad password,
	 *  give up.
	 */
	passtokey(key, password);
	convM2T(buf, &t, key);
	if(t.num != AuthHr || strcmp(t.cuid, tr.uid))
		return -1;

	return 0;
}
