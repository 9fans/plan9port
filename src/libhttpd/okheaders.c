#include <u.h>
#include <libc.h>
#include <bin.h>
#include <httpd.h>

/*
 * write initial part of successful header
 */
void
hokheaders(HConnect *c)
{
	Hio *hout;

	hout = &c->hout;
	hprint(hout, "%s 200 OK\r\n", hversion);
	hprint(hout, "Server: Plan9\r\n");
	hprint(hout, "Date: %D\r\n", time(nil));
	if(c->head.closeit)
		hprint(hout, "Connection: close\r\n");
	else if(!http11(c))
		hprint(hout, "Connection: Keep-Alive\r\n");
}
