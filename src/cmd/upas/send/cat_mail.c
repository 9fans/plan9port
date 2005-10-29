#include "common.h"
#include "send.h"


/* dispose of local addresses */
int
cat_mail(dest *dp, message *mp)
{
	Biobuf *fp;
	char *rcvr, *cp;
	Mlock *l;
	String *tmp, *s;
	int i, n;

	s = unescapespecial(s_clone(dp->repl1));
	if (nflg) {
		if(!xflg)
			print("cat >> %s\n", s_to_c(s));
		else
			print("%s\n", s_to_c(dp->addr));
		s_free(s);
		return 0;
	}
	for(i = 0;; i++){
		l = syslock(s_to_c(s));
		if(l == 0)
			return refuse(dp, mp, "can't lock mail file", 0, 0);

		fp = sysopen(s_to_c(s), "al", MBOXMODE);
		if(fp)
			break;
		tmp = s_append(0, s_to_c(s));
		s_append(tmp, ".tmp");
		fp = sysopen(s_to_c(tmp), "al", MBOXMODE);
		if(fp){
			syslog(0, "mail", "error: used %s", s_to_c(tmp));
			s_free(tmp);
			break;
		}
		s_free(tmp);
		sysunlock(l);
		if(i >= 5)
			return refuse(dp, mp, "mail file cannot be opened", 0, 0);
		sleep(1000);
	}
	s_free(s);
	n = m_print(mp, fp, (char *)0, 1);
	if (Bprint(fp, "\n") < 0 || Bflush(fp) < 0 || n < 0){
		sysclose(fp);
		sysunlock(l);
		return refuse(dp, mp, "error writing mail file", 0, 0);
	}
	sysclose(fp);
	sysunlock(l);
	rcvr = s_to_c(dp->addr);
	if(cp = strrchr(rcvr, '!'))
		rcvr = cp+1;
	logdelivery(dp, rcvr, mp);
	return 0;
}
