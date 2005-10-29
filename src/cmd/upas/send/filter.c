#include "common.h"
#include "send.h"

Biobuf	bin;
int rmail, tflg;
char *subjectarg;

char *findbody(char*);

void
main(int argc, char *argv[])
{
	message *mp;
	dest *dp;
	Reprog *p;
	Resub match[10];
	char file[MAXPATHLEN];
	Biobuf *fp;
	char *rcvr, *cp;
	Mlock *l;
	String *tmp;
	int i;
	int header, body;

	header = body = 0;
	ARGBEGIN {
	case 'h':
		header = 1;
		break;
	case 'b':
		header = 1;
		body = 1;
		break;
	} ARGEND

	Binit(&bin, 0, OREAD);
	if(argc < 2){
		fprint(2, "usage: filter rcvr mailfile [regexp mailfile ...]\n");
		exits("usage");
	}
	mp = m_read(&bin, 1, 0);

	/* get rid of local system name */
	cp = strchr(s_to_c(mp->sender), '!');
	if(cp){
		cp++;
		mp->sender = s_copy(cp);
	}

	dp = d_new(s_copy(argv[0]));
	strecpy(file, file+sizeof file, argv[1]);
	cp = findbody(s_to_c(mp->body));
	for(i = 2; i < argc; i += 2){
		p = regcomp(argv[i]);
		if(p == 0)
			continue;
		if(regexec(p, s_to_c(mp->sender), match, 10)){
			regsub(argv[i+1], file, sizeof(file), match, 10);
			break;
		}
		if(header == 0 && body == 0)
			continue;
		if(regexec(p, s_to_c(mp->body), match, 10)){
			if(body == 0 && match[0].s.sp >= cp)
				continue;
			regsub(argv[i+1], file, sizeof(file), match, 10);
			break;
		}
	}

	/*
	 *  always lock the normal mail file to avoid too many lock files
	 *  lying about.  This isn't right but it's what the majority prefers.
	 */
	l = syslock(argv[1]);
	if(l == 0){
		fprint(2, "can't lock mail file %s\n", argv[1]);
		exit(1);
	}

	/*
	 *  open the destination mail file
	 */
	fp = sysopen(file, "ca", MBOXMODE);
	if (fp == 0){
		tmp = s_append(0, file);
		s_append(tmp, ".tmp");
		fp = sysopen(s_to_c(tmp), "cal", MBOXMODE);
		if(fp == 0){
			sysunlock(l);
			fprint(2, "can't open mail file %s\n", file);
			exit(1);
		}
		syslog(0, "mail", "error: used %s", s_to_c(tmp));
		s_free(tmp);
	}
	Bseek(fp, 0, 2);
	if(m_print(mp, fp, (char *)0, 1) < 0
	|| Bprint(fp, "\n") < 0
	|| Bflush(fp) < 0){
		sysclose(fp);
		sysunlock(l);
		fprint(2, "can't write mail file %s\n", file);
		exit(1);
	}
	sysclose(fp);

	sysunlock(l);
	rcvr = argv[0];
	if(cp = strrchr(rcvr, '!'))
		rcvr = cp+1;
	logdelivery(dp, rcvr, mp);
	exit(0);
}

char*
findbody(char *p)
{
	if(*p == '\n')
		return p;

	while(*p){
		if(*p == '\n' && *(p+1) == '\n')
			return p+1;
		p++;
	}
	return p;
}
