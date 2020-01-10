#include <u.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <libc.h>

#define	REDIALTIMEOUT	15
#define TIMEOUT 600

char tmpfilename[L_tmpnam+1];
int alarmstate = 0;
int debugflag = 0;
int killflag = 0;
int statflag = 0;

void
cleanup(void) {
	unlink(tmpfilename);
}

#define SBSIZE 8192
unsigned char sendbuf[SBSIZE];

void
debug(char *str) {
	if (debugflag)
		fprintf(stderr, "%s", str);
}

void
alarmhandler(int sig) {
	fprintf(stderr, "timeout occurred, check printer.\n");
	exit(2);
}

/* send a message after each WARNPC percent of data sent */
#define WARNPC	5

int
copyfile(int in, int out, long tosend) {
	int n;
	int sent = 0;
	int percent = 0;

	if (debugflag)
		fprintf(stderr, "lpdsend: copyfile(%d,%d,%ld)\n",
			in, out, tosend);
	while ((n=read(in, sendbuf, SBSIZE)) > 0) {
		if (debugflag)
			fprintf(stderr, "lpdsend: copyfile read %d bytes from %d\n",
				n, in);
		alarm(TIMEOUT); alarmstate = 1;
		if (write(out, sendbuf, n) != n) {
			alarm(0);
			fprintf(stderr, "write to fd %d failed\n", out);
			return(0);
		}
		alarm(0);
		if (debugflag)
			fprintf(stderr, "lpdsend: copyfile wrote %d bytes to %d\n",
				n, out);
		sent += n;
		if (tosend && ((sent*100/tosend)>=(percent+WARNPC))) {
			percent += WARNPC;
			fprintf(stderr, ": %5.2f%% sent\n", sent*100.0/tosend);
		}
	}
	if (debugflag)
		fprintf(stderr, "lpdsend: copyfile read %d bytes from %d\n",
			n, in);
	return(!n);
}

char  strbuf[120];
char hostname[MAXHOSTNAMELEN], *username, *printername, *killarg;
char *inputname;
char filetype = 'o';	/* 'o' is for PostScript */
int seqno = 0;
char *seqfilename;

void
killjob(int printerfd) {
	int strlength;
	if (printername==0) {
		fprintf(stderr, "no printer name\n");
		exit(1);
	}
	if (username==0) {
		fprintf(stderr, "no user name given\n");
		exit(1);
	}
	if (killarg==0) {
		fprintf(stderr, "no job to kill\n");
		exit(1);
	}
	sprintf(strbuf, "%c%s %s %s\n", '\5', printername, username, killarg);
	strlength = strlen(strbuf);
	if (write(printerfd, strbuf, strlength) != strlength) {
		fprintf(stderr, "write(printer) error\n");
		exit(1);
	}
	copyfile(printerfd, 2, 0L);
}

void
checkqueue(int printerfd) {
	int strlength;

	sprintf(strbuf, "%c%s\n", '\4', printername);
	strlength = strlen(strbuf);
	if (write(printerfd, strbuf, strlength) != strlength) {
		fprintf(stderr, "write(printer) error\n");
		exit(1);
	}
	copyfile(printerfd, 2, 0L);
/*
{	int n;
	unsigned char sendbuf[1];
	while ((n=read(printerfd, sendbuf, 1)) > 0) {
		write(2, sendbuf, n);
	}
}
*/
}

void
getack(int printerfd, int as) {
	char resp;
	int rv;

	alarm(TIMEOUT); alarmstate = as;
	if ((rv=read(printerfd, &resp, 1)) != 1 || resp != '\0') {
		fprintf(stderr, "getack failed: read returned %d, read value (if any) %d, alarmstate=%d\n",
			rv, resp, alarmstate);
		exit(1);
	}
	alarm(0);
}

/* send control file */
void
sendctrl(int printerfd) {
	char cntrlstrbuf[256];
	int strlength, cntrlen;

	sprintf(cntrlstrbuf, "H%s\nP%s\n%cdfA%3.3d%s\n", hostname, username, filetype, seqno, hostname);
	cntrlen = strlen(cntrlstrbuf);
	sprintf(strbuf, "%c%d cfA%3.3d%s\n", '\2', cntrlen, seqno, hostname);
	strlength = strlen(strbuf);
	if (write(printerfd, strbuf, strlength) != strlength) {
		fprintf(stderr, "write(printer) error\n");
		exit(1);
	}
	getack(printerfd, 3);
	if (write(printerfd, cntrlstrbuf, cntrlen) != cntrlen) {
		fprintf(stderr, "write(printer) error\n");
		exit(1);
	}
	if (write(printerfd, "\0", 1) != 1) {
		fprintf(stderr, "write(printer) error\n");
		exit(1);
	}
	getack(printerfd, 4);
}

/* send data file */
void
senddata(int inputfd, int printerfd, long size) {
	int strlength;

	sprintf(strbuf, "%c%ld dfA%3.3d%s\n", '\3', size, seqno, hostname);
	strlength = strlen(strbuf);
	if (write(printerfd, strbuf, strlength) != strlength) {
		fprintf(stderr, "write(printer) error\n");
		exit(1);
	}
	getack(printerfd, 5);
	if (!copyfile(inputfd, printerfd, size)) {
		fprintf(stderr, "failed to send file to printer\n");
		exit(1);
	}
	if (write(printerfd, "\0", 1) != 1) {
		fprintf(stderr, "write(printer) error\n");
		exit(1);
	}
	fprintf(stderr, "%ld bytes sent, status: waiting for end of job\n", size);
	getack(printerfd, 6);
}

void
sendjob(int inputfd, int printerfd) {
	struct stat statbuf;
	int strlength;

	if (fstat(inputfd, &statbuf) < 0) {
		fprintf(stderr, "fstat(%s) failed\n", inputname);
		exit(1);
	}
	sprintf(strbuf, "%c%s\n", '\2', printername);
	strlength = strlen(strbuf);
	if (write(printerfd, strbuf, strlength) != strlength) {
		fprintf(stderr, "write(printer) error\n");
		exit(1);
	}
	getack(printerfd, 2);
	debug("send data\n");
	senddata(inputfd, printerfd, statbuf.st_size);
	debug("send control info\n");
	sendctrl(printerfd);
	fprintf(stderr, "%ld bytes sent, status: end of job\n", (long)statbuf.st_size);
}

/*
 *  make an address, add the defaults
 */
char *
netmkaddr(char *linear, char *defnet, char *defsrv)
{
	static char addr[512];
	char *cp;

	/*
	 *  dump network name
	 */
	cp = strchr(linear, '!');
	if(cp == 0){
		if(defnet==0){
			if(defsrv)
				sprintf(addr, "net!%s!%s", linear, defsrv);
			else
				sprintf(addr, "net!%s", linear);
		}
		else {
			if(defsrv)
				sprintf(addr, "%s!%s!%s", defnet, linear, defsrv);
			else
				sprintf(addr, "%s!%s", defnet, linear);
		}
		return addr;
	}

	/*
	 *  if there is already a service, use it
	 */
	cp = strchr(cp+1, '!');
	if(cp)
		return linear;

	/*
	 *  add default service
	 */
	if(defsrv == 0)
		return linear;
	sprintf(addr, "%s!%s", linear, defsrv);

	return addr;
}

void
main(int argc, char *argv[]) {
	int c, usgflg = 0;
	char *desthostname;
	int printerfd;
	int inputfd;
	int sendport;
	char portstr[4];

	desthostname = nil;
	if (signal(SIGALRM, alarmhandler) == SIG_ERR) {
		fprintf(stderr, "failed to set alarm handler\n");
		exit(1);
	}
	while ((c = getopt(argc, argv, "Dd:k:qs:t:H:P:")) != -1)
		switch (c) {
		case 'D':
			debugflag = 1;
			debug("debugging on\n");
			break;
		case 'd':
			printername = optarg;
			break;
		case 'k':
			if (statflag) {
				fprintf(stderr, "cannot have both -k and -q flags\n");
				exit(1);
			}
			killflag = 1;
			killarg = optarg;
			break;
		case 'q':
			if (killflag) {
				fprintf(stderr, "cannot have both -q and -k flags\n");
				exit(1);
			}
			statflag = 1;
			break;
		case 's':
			seqno = strtol(optarg, NULL, 10);
			if (seqno < 0 || seqno > 999)
				seqno = 0;
			break;
		case 't':
			switch (filetype) {
			case 'c':
			case 'd':
			case 'f':
			case 'g':
			case 'l':
			case 'n':
			case 'o':
			case 'p':
			case 'r':
			case 't':
			case 'v':
			case 'z':
				filetype = optarg[0];
				break;
			default:
				usgflg++;
				break;
			}
			break;
		case 'H':
			strncpy(hostname, optarg, MAXHOSTNAMELEN);
			break;
		case 'P':
			username = optarg;
			break;
		default:
		case '?':
			fprintf(stderr, "unknown option %c\n", c);
			usgflg++;
		}
	if (argc < 2) usgflg++;
	if (optind < argc) {
		desthostname = argv[optind++];
	} else
		usgflg++;
	if (usgflg) {
		fprintf(stderr, "usage: to send a job - %s -d printer -H hostname -P username [-s seqno] [-t[cdfgklnoprtvz]] desthost [filename]\n", argv[0]);
		fprintf(stderr, "     to check status - %s -d printer -q desthost\n", argv[0]);
		fprintf(stderr, "       to kill a job - %s -d printer -P username -k jobname desthost\n", argv[0]);
		exit(1);
	}

/* make sure the file to send is here and ready
 * otherwise the TCP connection times out.
 */
 	inputfd = -1;
	if (!statflag && !killflag) {
		if (optind < argc) {
			inputname = argv[optind++];
			debug("open("); debug(inputname); debug(")\n");
			inputfd = open(inputname, O_RDONLY);
			if (inputfd < 0) {
				fprintf(stderr, "open(%s) failed\n", inputname);
				exit(1);
			}
		} else {
			inputname = "stdin";
			tmpnam(tmpfilename);
			debug("using stdin\n");
			if ((inputfd = create(tmpfilename, ORDWR, 0600)) < 0) {
				fprintf(stderr, "open(%s) failed\n", tmpfilename);
				exit(1);
			}
			atexit(cleanup);
			debug("copy input to temp file ");
			debug(tmpfilename);
			debug("\n");
			if (!copyfile(0, inputfd, 0L)) {
				fprintf(stderr, "failed to copy file to temporary file\n");
				exit(1);
			}
			if (lseek(inputfd, 0L, 0) < 0) {
				fprintf(stderr, "failed to seek back to the beginning of the temporary file\n");
				exit(1);
			}
		}
	}

	sprintf(strbuf, "%s", netmkaddr(desthostname, "tcp", "printer"));
	fprintf(stderr, "connecting to %s\n", strbuf);
	for (sendport=721; sendport<=731; sendport++) {
		sprintf(portstr, "%3.3d", sendport);
		fprintf(stderr, " trying from port %s...", portstr);
		debug(" dial("); debug(strbuf); debug(", "); debug(portstr); debug(", 0, 0) ...");
		printerfd = dial(strbuf, portstr, 0, 0);
		if (printerfd >= 0) {
			fprintf(stderr, "connected\n");
			break;
		}
		fprintf(stderr, "failed\n");
		sleep(REDIALTIMEOUT);
	}
	if (printerfd < 0) {
		fprintf(stderr, "Cannot open a valid port!\n");
		fprintf(stderr, "-  All source ports [721-731] may be busy.\n");
		fprintf(stderr, "-  Is recipient ready and online?\n");
		fprintf(stderr, "-  If all else fails, cycle the power!\n");
		exit(1);
	}
/*	hostname[8] = '\0'; */
#ifndef PLAN9
	if (gethostname(hostname, sizeof(hostname)) < 0) {
		perror("gethostname");
		exit(1);
	}
#endif
/*	char *hnend;
	if ((hnend = strchr(hostname, '.')) != NULL)
		*hnend = '\0';
 */
	if (statflag) {
		checkqueue(printerfd);
	} else if (killflag) {
		killjob(printerfd);
	} else {
		sendjob(inputfd, printerfd);
	}
	exit(0);
}
