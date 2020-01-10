#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <thread.h>
#include <9pclient.h>
#include "dns.h"
#include "ip.h"

void
usage(void)
{
	fprint(2, "usage: dnsquery [-x service]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	CFid *fd;
	int n, len;
	Biobuf in;
	char line[1024], *lp, *p, *np, *dns;
	char buf[1024];

	dns = "dns";
	ARGBEGIN{
	case 'x':
		dns = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	if(argc)
		usage();

	fd = nsopen(dns, nil, "dns", ORDWR);
	if(fd == nil)
		sysfatal("open %s!dns: %r", dns);

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

		fsseek(fd, 0, 0);
		if(fswrite(fd, line, n) < 0) {
			print("!%r\n");
			continue;
		}
		fsseek(fd, 0, 0);
		while((n = fsread(fd, buf, sizeof(buf))) > 0){
			buf[n] = 0;
			print("%s\n", buf);
		}
	}
	threadexitsall(0);
}
