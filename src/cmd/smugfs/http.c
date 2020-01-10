#include "a.h"

static char*
haveheader(char *buf, int n)
{
	int i;

	for(i=0; i<n; i++){
		if(buf[i] == '\n'){
			if(i+2 < n && buf[i+1] == '\r' && buf[i+2] == '\n')
				return buf+i+3;
			if(i+1 < n && buf[i+1] == '\n')
				return buf+i+2;
		}
	}
	return 0;
}

static int
parseheader(char *buf, int n, HTTPHeader *hdr)
{
	int nline;
	char *data, *ebuf, *p, *q, *next;

	memset(hdr, 0, sizeof *hdr);
	ebuf = buf+n;
	data = haveheader(buf, n);
	if(data == nil)
		return -1;

	data[-1] = 0;
	if(data[-2] == '\r')
		data[-2] = 0;
	if(chattyhttp > 1){
		fprint(2, "--HTTP Response Header:\n");
		fprint(2, "%s\n", buf);
		fprint(2, "--\n");
	}
	nline = 0;
	for(p=buf; *p; p=next, nline++){
		q = strchr(p, '\n');
		if(q){
			next = q+1;
			*q = 0;
			if(q > p && q[-1] == '\r')
				q[-1] = 0;
		}else
			next = p+strlen(p);
		if(nline == 0){
			if(memcmp(p, "HTTP/", 5) != 0){
				werrstr("invalid HTTP version: %.10s", p);
				return -1;
			}
			q = strchr(p, ' ');
			if(q == nil){
				werrstr("invalid HTTP version");
				return -1;
			}
			*q++ = 0;
			strncpy(hdr->proto, p, sizeof hdr->proto);
			hdr->proto[sizeof hdr->proto-1] = 0;
			while(*q == ' ')
				q++;
			if(*q < '0' || '9' < *q){
				werrstr("invalid HTTP response code");
				return -1;
			}
			p = q;
			q = strchr(p, ' ');
			if(q == nil)
				q = p+strlen(p);
			else
				*q++ = 0;
			hdr->code = strtol(p, &p, 10);
			if(*p != 0)
				return -1;
			while(*q == ' ')
				q++;
			strncpy(hdr->codedesc, q, sizeof hdr->codedesc);
			hdr->codedesc[sizeof hdr->codedesc-1] = 0;
			continue;
		}
		q = strchr(p, ':');
		if(q == nil)
			continue;
		*q++ = 0;
		while(*q != 0 && (*q == ' ' || *q == '\t'))
			q++;
		if(cistrcmp(p, "Content-Type") == 0){
			strncpy(hdr->contenttype, q, sizeof hdr->contenttype);
			hdr->contenttype[sizeof hdr->contenttype-1] = 0;
			continue;
		}
		if(cistrcmp(p, "Content-Length") == 0 && '0' <= *q && *q <= '9'){
			hdr->contentlength = strtoll(q, 0, 10);
			continue;
		}
	}
	if(nline < 1){
		werrstr("no header");
		return -1;
	}

	memmove(buf, data, ebuf - data);
	return ebuf - data;
}

static char*
genhttp(Protocol *proto, char *host, char *req, HTTPHeader *hdr, int wfd, int rfd, vlong rtotal)
{
	int n, m, total, want;
	char buf[8192], *data;
	Pfd *fd;

	if(chattyhttp > 1){
		fprint(2, "--HTTP Request:\n");
		fprint(2, "%s", req);
		fprint(2, "--\n");
	}
	fd = proto->connect(host);
	if(fd == nil){
		if(chattyhttp > 0)
			fprint(2, "connect %s: %r\n", host);
		return nil;
	}

	n = strlen(req);
	if(proto->write(fd, req, n) != n){
		if(chattyhttp > 0)
			fprint(2, "write %s: %r\n", host);
		proto->close(fd);
		return nil;
	}

	if(rfd >= 0){
		while(rtotal > 0){
			m = sizeof buf;
			if(m > rtotal)
				m = rtotal;
			if((n = read(rfd, buf, m)) <= 0){
				fprint(2, "read: missing data\n");
				proto->close(fd);
				return nil;
			}
			if(proto->write(fd, buf, n) != n){
				fprint(2, "write data: %r\n");
				proto->close(fd);
				return nil;
			}
			rtotal -= n;
		}
	}

	total = 0;
	while(!haveheader(buf, total)){
		n = proto->read(fd, buf+total, sizeof buf-total);
		if(n <= 0){
			if(chattyhttp > 0)
				fprint(2, "read missing header\n");
			proto->close(fd);
			return nil;
		}
		total += n;
	}

	n = parseheader(buf, total, hdr);
	if(n < 0){
		fprint(2, "failed response parse: %r\n");
		proto->close(fd);
		return nil;
	}
	if(hdr->contentlength >= MaxResponse){
		werrstr("response too long");
		proto->close(fd);
		return nil;
	}
	if(hdr->contentlength >= 0 && n > hdr->contentlength)
		n = hdr->contentlength;
	want = sizeof buf;
	data = nil;
	total = 0;
	goto didread;

	while(want > 0 && (n = proto->read(fd, buf, want)) > 0){
	didread:
		if(wfd >= 0){
			if(writen(wfd, buf, n) < 0){
				proto->close(fd);
				werrstr("write error");
				return nil;
			}
		}else{
			data = erealloc(data, total+n);
			memmove(data+total, buf, n);
		}
		total += n;
		if(total > MaxResponse){
			proto->close(fd);
			werrstr("response too long");
			return nil;
		}
		if(hdr->contentlength >= 0 && total + want > hdr->contentlength)
			want = hdr->contentlength - total;
	}
	proto->close(fd);

	if(hdr->contentlength >= 0 && total != hdr->contentlength){
		werrstr("got wrong content size %d %d", total, hdr->contentlength);
		return nil;
	}
	hdr->contentlength = total;
	if(wfd >= 0)
		return (void*)1;
	else{
		data = erealloc(data, total+1);
		data[total] = 0;
	}
	return data;
}

char*
httpreq(Protocol *proto, char *host, char *req, HTTPHeader *hdr, int rfd, vlong rlength)
{
	return genhttp(proto, host, req, hdr, -1, rfd, rlength);
}

int
httptofile(Protocol *proto, char *host, char *req, HTTPHeader *hdr, int fd)
{
	if(fd < 0){
		werrstr("bad fd");
		return -1;
	}
	if(genhttp(proto, host, req, hdr, fd, -1, 0) == nil)
		return -1;
	return 0;
}
