#include <u.h>
#include <libc.h>
#include <bin.h>
#include <httpd.h>

int
hunallowed(HConnect *c, char *allowed)
{
	Hio *hout;
	int n;

	n = snprint(c->xferbuf, HBufSize, "<head><title>Method Not Allowed</title></head>\r\n"
		"<body><h1>Method Not Allowed</h1>\r\n"
		"You can't %s on <a href=\"%U\"> here</a>.<p></body>\r\n", c->req.meth, c->req.uri);

	hout = &c->hout;
	hprint(hout, "%s 405 Method Not Allowed\r\n", hversion);
	hprint(hout, "Date: %D\r\n", time(nil));
	hprint(hout, "Server: Plan9\r\n");
	hprint(hout, "Content-Type: text/html\r\n");
	hprint(hout, "Allow: %s\r\n", allowed);
	hprint(hout, "Content-Length: %d\r\n", n);
	if(c->head.closeit)
		hprint(hout, "Connection: close\r\n");
	else if(!http11(c))
		hprint(hout, "Connection: Keep-Alive\r\n");
	hprint(hout, "\r\n");

	if(strcmp(c->req.meth, "HEAD") != 0)
		hwrite(hout, c->xferbuf, n);

	if(c->replog)
		c->replog(c, "Reply: 405 Method Not Allowed\nReason: Method Not Allowed\n");
	return hflush(hout);
}
