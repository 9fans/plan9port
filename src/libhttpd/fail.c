#include <u.h>
#include <libc.h>
#include <bin.h>
#include <httpd.h>

typedef struct Error	Error;

struct Error
{
	char	*num;
	char	*concise;
	char	*verbose;
};

Error errormsg[] =
{
	[HInternal]	{"500 Internal Error", "Internal Error",
		"This server could not process your request due to an internal error."},
	[HTempFail]	{"500 Internal Error", "Temporary Failure",
		"The object %s is currently inaccessible.<p>Please try again later."},
	[HUnimp]	{"501 Not implemented", "Command not implemented",
		"This server does not implement the %s command."},
	[HUnkVers]	{"501 Not Implemented", "Unknown http version",
		"This server does not know how to respond to http version %s."},
	[HBadCont]	{"501 Not Implemented", "Impossible format",
		"This server cannot produce %s in any of the formats your client accepts."},
	[HBadReq]	{"400 Bad Request", "Strange Request",
		"Your client sent a query that this server could not understand."},
	[HSyntax]	{"400 Bad Request", "Garbled Syntax",
		"Your client sent a query with incoherent syntax."},
	[HBadSearch]	{"400 Bad Request", "Inapplicable Search",
		"Your client sent a search that cannot be applied to %s."},
	[HNotFound]	{"404 Not Found", "Object not found",
		"The object %s does not exist on this server."},
	[HNoSearch]	{"403 Forbidden", "Search not supported",
		"The object %s does not support the search command."},
	[HNoData]	{"403 Forbidden", "No data supplied",
		"Search or forms data must be supplied to %s."},
	[HExpectFail]	{"403 Expectation Failed", "Expectation Failed",
		"This server does not support some of your request's expectations."},
	[HUnauth]	{"403 Forbidden", "Forbidden",
		"You are not allowed to see the object %s."},
	[HOK]		{"200 OK", "everything is fine"},
};

/*
 * write a failure message to the net and exit
 */
int
hfail(HConnect *c, int reason, ...)
{
	Hio *hout;
	char makeup[HBufSize];
	va_list arg;
	int n;

	hout = &c->hout;
	va_start(arg, reason);
	vseprint(makeup, makeup+HBufSize, errormsg[reason].verbose, arg);
	va_end(arg);
	n = snprint(c->xferbuf, HBufSize, "<head><title>%s</title></head>\n<body><h1>%s</h1>\n%s</body>\n",
		errormsg[reason].concise, errormsg[reason].concise, makeup);

	hprint(hout, "%s %s\r\n", hversion, errormsg[reason].num);
	hprint(hout, "Date: %D\r\n", time(nil));
	hprint(hout, "Server: Plan9\r\n");
	hprint(hout, "Content-Type: text/html\r\n");
	hprint(hout, "Content-Length: %d\r\n", n);
	if(c->head.closeit)
		hprint(hout, "Connection: close\r\n");
	else if(!http11(c))
		hprint(hout, "Connection: Keep-Alive\r\n");
	hprint(hout, "\r\n");

	if(c->req.meth == nil || strcmp(c->req.meth, "HEAD") != 0)
		hwrite(hout, c->xferbuf, n);

	if(c->replog)
		c->replog(c, "Reply: %s\nReason: %s\n", errormsg[reason].num, errormsg[reason].concise);
	return hflush(hout);
}
