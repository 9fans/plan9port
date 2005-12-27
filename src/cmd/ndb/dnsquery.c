#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include "dns.h"
#include "ip.h"

void
main(int argc, char *argv[])
{
	int fd, n, len, domount;
	Biobuf in;
	char line[1024], *lp, *p, *np, *mtpt, *srv, *dns;
	char buf[1024];

	dns = "/net/dns";
	mtpt = "/net";
	srv = "/srv/dns";
	domount = 1;
	ARGBEGIN {
	case 'x':
		dns = "/net.alt/dns";
		mtpt = "/net.alt";
		srv = "/srv/dns_net.alt";
		break;
	default:
		fprint(2, "usage: %s -x [dns-mount-point]\n", argv0);
		exits("usage");
	} ARGEND;

	if(argc == 1){
		domount = 0;
		mtpt = argv[0];
	}

	fd = open(dns, ORDWR);
	if(fd < 0){
		if(domount == 0){
			fprint(2, "can't open %s: %r\n", mtpt);
			exits(0);
		}
		fd = open(srv, ORDWR);
		if(fd < 0){
			print("can't open %s: %r\n", srv);
			exits(0);
		}
		if(mount(fd, -1, mtpt, MBEFORE, "") < 0){
			print("can't mount(%s, %s): %r\n", srv, mtpt);
			exits(0);
		}
		fd = open(mtpt, ORDWR);
		if(fd < 0){
			print("can't open %s: %r\n", mtpt);
			exits(0);
		}
	}
	Binit(&in, 0, OREAD);
	for(print("> "); lp = Brdline(&in, '\n'); print("> ")){
		n = Blinelen(&in)-1;
		strncpy(line, lp, n);
		line[n] = 0;
		if (n<=1)
			continue;
		/* default to an "ip" request if alpha, "ptr" if numeric */
		if (strchr(line, ' ')==0) {
			if(strcmp(ipattr(line), "ip") == 0) {
				strcat(line, " ptr");
				n += 4;
			} else {
				strcat(line, " ip");
				n += 3;
			}
		}

		/* inverse queries may need to be permuted */
		if(n > 4 && strcmp("ptr", &line[n-3]) == 0
		&& strstr(line, "IN-ADDR") == 0 && strstr(line, "in-addr") == 0){
			for(p = line; *p; p++)
				if(*p == ' '){
					*p = '.';
					break;
				}
			np = buf;
			len = 0;
			while(p >= line){
				len++;
				p--;
				if(*p == '.'){
					memmove(np, p+1, len);
					np += len;
					len = 0;
				}
			}
			memmove(np, p+1, len);
			np += len;
			strcpy(np, "in-addr.arpa ptr");
			strcpy(line, buf);
			n = strlen(line);
		}

		seek(fd, 0, 0);
		if(write(fd, line, n) < 0) {
			print("!%r\n");
			continue;
		}
		seek(fd, 0, 0);
		while((n = read(fd, buf, sizeof(buf))) > 0){
			buf[n] = 0;
			print("%s\n", buf);
		}
	}
	exits(0);
}
