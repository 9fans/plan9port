#include "common.h"
#include "send.h"

/* configuration */
#define LOGBiobuf "log/status"

/* log mail delivery */
extern void
logdelivery(dest *list, char *rcvr, message *mp)
{
	dest *parent;
	String *srcvr, *sender;

	srcvr = unescapespecial(s_copy(rcvr));
	sender = unescapespecial(s_clone(mp->sender));

	for(parent=list; parent->parent!=0; parent=parent->parent)
		;
	if(parent!=list && strcmp(s_to_c(parent->addr), s_to_c(srcvr))!=0)
		syslog(0, "mail", "delivered %s From %.256s %.256s (%.256s) %d",
			rcvr,
			s_to_c(sender), s_to_c(mp->date),
			s_to_c(parent->addr), mp->size);
	else
		syslog(0, "mail", "delivered %s From %.256s %.256s %d", s_to_c(srcvr),
			s_to_c(sender), s_to_c(mp->date), mp->size);
	s_free(srcvr);
	s_free(sender);
}

/* log mail forwarding */
extern void
loglist(dest *list, message *mp, char *tag)
{
	dest *next;
	dest *parent;
	String *srcvr, *sender;

	sender = unescapespecial(s_clone(mp->sender));

	for(next=d_rm(&list); next != 0; next = d_rm(&list)) {
		for(parent=next; parent->parent!=0; parent=parent->parent)
			;
		srcvr = unescapespecial(s_clone(next->addr));
		if(parent!=next)
			syslog(0, "mail", "%s %.256s From %.256s %.256s (%.256s) %d",
				tag,
				s_to_c(srcvr), s_to_c(sender),
				s_to_c(mp->date), s_to_c(parent->addr), mp->size);
		else
			syslog(0, "mail", "%s %.256s From %.256s %.256s %d", tag,
				s_to_c(srcvr), s_to_c(sender),
				s_to_c(mp->date), mp->size);
		s_free(srcvr);
	}
	s_free(sender);
}

/* log a mail refusal */
extern void
logrefusal(dest *dp, message *mp, char *msg)
{
	char buf[2048];
	char *cp, *ep;
	String *sender, *srcvr;

	srcvr = unescapespecial(s_clone(dp->addr));
	sender = unescapespecial(s_clone(mp->sender));

	sprint(buf, "error %.256s From %.256s %.256s\nerror+ ", s_to_c(srcvr),
		s_to_c(sender), s_to_c(mp->date));
	s_free(srcvr);
	s_free(sender);
	cp = buf + strlen(buf);
	ep = buf + sizeof(buf) - sizeof("error + ");
	while(*msg && cp<ep) {
		*cp++ = *msg;
		if (*msg++ == '\n') {
			strcpy(cp, "error+ ");
			cp += sizeof("error+ ") - 1;
		}
	}
	*cp = 0;
	syslog(0, "mail", "%s", buf);
}
