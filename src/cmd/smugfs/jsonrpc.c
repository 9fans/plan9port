#include "a.h"

// JSON request/reply cache.

int chattyhttp;

typedef struct JEntry JEntry;
struct JEntry
{
	CEntry ce;
	Json *reply;
};

static Cache *jsoncache;

static void
jfree(CEntry *ce)
{
	JEntry *j;

	j = (JEntry*)ce;
	jclose(j->reply);
}

static JEntry*
jcachelookup(char *request)
{
	if(jsoncache == nil)
		jsoncache = newcache(sizeof(JEntry), 1000, jfree);
	return (JEntry*)cachelookup(jsoncache, request, 1);
}

void
jcacheflush(char *substr)
{
	if(jsoncache == nil)
		return;
	cacheflush(jsoncache, substr);
}


// JSON RPC over HTTP

static char*
makehttprequest(char *host, char *path, char *postdata)
{
	Fmt fmt;

	fmtstrinit(&fmt);
	fmtprint(&fmt, "POST %s HTTP/1.0\r\n", path);
	fmtprint(&fmt, "Host: %s\r\n", host);
	fmtprint(&fmt, "User-Agent: " USER_AGENT "\r\n");
	fmtprint(&fmt, "Content-Type: application/x-www-form-urlencoded\r\n");
	fmtprint(&fmt, "Content-Length: %d\r\n", strlen(postdata));
	fmtprint(&fmt, "\r\n");
	fmtprint(&fmt, "%s", postdata);
	return fmtstrflush(&fmt);
}

static char*
makerequest(char *method, char *name1, va_list arg)
{
	char *p, *key, *val;
	Fmt fmt;

	fmtstrinit(&fmt);
	fmtprint(&fmt, "&");
	p = name1;
	while(p != nil){
		key = p;
		val = va_arg(arg, char*);
		if(val == nil)
			sysfatal("jsonrpc: nil value");
		fmtprint(&fmt, "%U=%U&", key, val);
		p = va_arg(arg, char*);
	}
	// TODO: These are SmugMug-specific, probably.
	fmtprint(&fmt, "method=%s&", method);
	if(sessid)
		fmtprint(&fmt, "SessionID=%s&", sessid);
	fmtprint(&fmt, "APIKey=%s", APIKEY);
	return fmtstrflush(&fmt);
}

static char*
dojsonhttp(Protocol *proto, char *host, char *request, int rfd, vlong rlength)
{
	char *data;
	HTTPHeader hdr;

	data = httpreq(proto, host, request, &hdr, rfd, rlength);
	if(data == nil){
		fprint(2, "httpreq: %r\n");
		return nil;
	}
	if(strcmp(hdr.contenttype, "application/json") != 0 &&
	   (strcmp(hdr.contenttype, "text/html; charset=utf-8") != 0 || data[0] != '{')){  // upload.smugmug.com, sigh
		werrstr("bad content type: %s", hdr.contenttype);
		fprint(2, "Content-Type: %s\n", hdr.contenttype);
		write(2, data, hdr.contentlength);
		return nil;
	}
	if(hdr.contentlength == 0){
		werrstr("no content");
		return nil;
	}
	return data;
}

Json*
jsonrpc(Protocol *proto, char *host, char *path, char *method, char *name1, va_list arg, int usecache)
{
	char *httpreq, *request, *reply;
	JEntry *je;
	Json *jv, *jstat, *jmsg;

	request = makerequest(method, name1, arg);

	je = nil;
	if(usecache){
		je = jcachelookup(request);
		if(je->reply){
			free(request);
			return jincref(je->reply);
		}
	}

	rpclog("%T %s", request);
	httpreq = makehttprequest(host, path, request);
	free(request);

	if((reply = dojsonhttp(proto, host, httpreq, -1, 0)) == nil){
		free(httpreq);
		return nil;
	}
	free(httpreq);

	jv = parsejson(reply);
	free(reply);
	if(jv == nil){
		rpclog("%s: error parsing JSON reply: %r", method);
		return nil;
	}

	if(jstrcmp((jstat = jlookup(jv, "stat")), "ok") == 0){
		if(je)
			je->reply = jincref(jv);
		return jv;
	}

	if(jstrcmp(jstat, "fail") == 0){
		jmsg = jlookup(jv, "message");
		if(jmsg){
			// If there are no images, that's not an error!
			// (But SmugMug says it is.)
			if(strcmp(method, "smugmug.images.get") == 0 &&
			   jstrcmp(jmsg, "empty set - no images found") == 0){
				jclose(jv);
				jv = parsejson("{\"stat\":\"ok\", \"Images\":[]}");
				if(jv == nil)
					sysfatal("parsejson: %r");
				je->reply = jincref(jv);
				return jv;
			}
			if(printerrors)
				fprint(2, "%s: %J\n", method, jv);
			rpclog("%s: %J", method, jmsg);
			werrstr("%J", jmsg);
			jclose(jv);
			return nil;
		}
		rpclog("%s: json status: %J", method, jstat);
		jclose(jv);
		return nil;
	}

	rpclog("%s: json stat=%J", method, jstat);
	jclose(jv);
	return nil;
}

Json*
ncsmug(char *method, char *name1, ...)
{
	Json *jv;
	va_list arg;

	va_start(arg, name1);
	// TODO: Could use https only for login.
	jv = jsonrpc(&https, HOST, PATH, method, name1, arg, 0);
	va_end(arg);
	rpclog("reply: %J", jv);
	return jv;
}

Json*
smug(char *method, char *name1, ...)
{
	Json *jv;
	va_list arg;

	va_start(arg, name1);
	jv = jsonrpc(&http, HOST, PATH, method, name1, arg, 1);
	va_end(arg);
	return jv;
}

Json*
jsonupload(Protocol *proto, char *host, char *req, int rfd, vlong rlength)
{
	Json *jv, *jstat, *jmsg;
	char *reply;

	if((reply = dojsonhttp(proto, host, req, rfd, rlength)) == nil)
		return nil;

	jv = parsejson(reply);
	free(reply);
	if(jv == nil){
		fprint(2, "upload: error parsing JSON reply\n");
		return nil;
	}

	if(jstrcmp((jstat = jlookup(jv, "stat")), "ok") == 0)
		return jv;

	if(jstrcmp(jstat, "fail") == 0){
		jmsg = jlookup(jv, "message");
		if(jmsg){
			fprint(2, "upload: %J\n", jmsg);
			werrstr("%J", jmsg);
			jclose(jv);
			return nil;
		}
		fprint(2, "upload: json status: %J\n", jstat);
		jclose(jv);
		return nil;
	}

	fprint(2, "upload: %J\n", jv);
	jclose(jv);
	return nil;
}
