/*
 * RFC822 message tokenizer (really feature generator) for spam filter.
 *
 * See Paul Graham's musings on spam filtering for theory.
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include <ctype.h>
#include "dfa.h"

void buildre(Dreprog*[3]);
int debug;
char *refile = "#9/mail/lib/classify.re";
int maxtoklen = 20;
int trim(char*);

void
usage(void)
{
	fprint(2, "usage: msgtok [-D] [-r /mail/lib/classify.re] [file]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, hdr, n, eof, off;
	Dreprog *re[3];
	int m[3];
	char *p, *ep, *tag;
	Biobuf bout, bin;
	char msg[1024+1];
	char buf[1024];

	refile = unsharp(refile);
	buildre(re);
	ARGBEGIN{
	case 'D':
		debug = 1;
		break;
	case 'n':
		maxtoklen = atoi(EARGF(usage()));
		break;
	case 'r':
		refile = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	if(argc > 1)
		usage();
	if(argc == 1){
		close(0);
		if(open(argv[0], OREAD) < 0)
			sysfatal("open %s: %r", argv[0]);
	}

	tag = nil;
	Binit(&bin, 0, OREAD);
	Binit(&bout, 1, OWRITE);
	ep = msg;
	p = msg;
	eof = 0;
	off = 0;
	hdr = 1;
	for(;;){
		/* replenish buffer */
		if(ep - p < 512 && !eof){
			if(p > msg + 1){
				n = ep - p;
				memmove(msg, p-1, ep-(p-1));
				off += (p-1) - msg;
				p = msg+1;
				ep = p + n;
			}
			n = Bread(&bin, ep, msg+(sizeof msg - 1)- ep);
			if(n < 0)
				sysfatal("read error: %r");
			if(n == 0)
				eof = 1;
			ep += n;
			*ep = 0;
		}
		if(p >= ep)
			break;

		if(*p == 0){
			p++;
			continue;
		}

		if(hdr && p[-1]=='\n'){
			if(p[0]=='\n')
				hdr = 0;
			else if(cistrncmp(p-1, "\nfrom:", 6) == 0)
				tag = "From*";
			else if(cistrncmp(p-1, "\nto:", 4) == 0)
				tag = "To*";
			else if(cistrncmp(p-1, "\nsubject:", 9) == 0)
				tag = "Subject*";
			else if(cistrncmp(p-1, "\nreturn-path:", 13) == 0)
				tag = "Return-Path*";
			else
				tag = nil;
		}
		m[0] = dregexec(re[0], p, p==msg || p[-1]=='\n');
		m[1] = dregexec(re[1], p, p==msg || p[-1]=='\n');
		m[2] = dregexec(re[2], p, p==msg || p[-1]=='\n');

		n = m[0];
		if(n < m[1])
			n = m[1];
		if(n < m[2])
			n = m[2];
		if(n <= 0){
fprint(2, "«%s» %.2ux", p, p[0]);
			sysfatal("no regexps matched at %ld", off + (p-msg));
		}

		if(m[0] >= m[1] && m[0] >= m[2]){
			/* "From " marks start of new message */
			Bprint(&bout, "*From*\n");
			n = m[0];
			hdr = 1;
		}else if(m[2] > 1){
			/* ignore */
			n = m[2];
		}else if(m[1] >= m[0] && m[1] >= m[2] && m[1] > 2 && m[1] <= maxtoklen){
			/* keyword */
			/* should do UTF-aware lowercasing, too much bother */
/*
			for(i=0; i<n; i++)
				if('A' <= p[i] && p[i] <= 'Z')
					p[i] += 'a' - 'A';
*/
			if(tag){
				i = strlen(tag);
				memmove(buf, tag, i);
				memmove(buf+i, p, m[1]);
				buf[i+m[1]] = 0;
			}else{
				memmove(buf, p, m[1]);
				buf[m[1]] = 0;
			}
			Bprint(&bout, "%s\n", buf);
			while(trim(buf) >= 0)
				Bprint(&bout, "stem*%s\n", buf);
			n = m[1];
		}else
			n = m[2];
		if(debug)
			fprint(2, "%.*s¦", utfnlen(p, n), p);
		p += n;
	}
	Bterm(&bout);
	exits(0);
}

void
buildre(Dreprog *re[3])
{
	Biobuf *b;

	if((b = Bopen(refile, OREAD)) == nil)
		sysfatal("open %s: %r", refile);

	re[0] = Breaddfa(b);
	re[1] = Breaddfa(b);
	re[2] = Breaddfa(b);

	if(re[0]==nil || re[1]==nil || re[2]==nil)
		sysfatal("Breaddfa: %r");
	Bterm(b);
}

/* perhaps this belongs in the tokenizer */
int
trim(char *s)
{
	char *p, *op;
	int mix, mix1;

	if(*s == '*')
		return -1;

	/* strip leading punctuation */
	p = strchr(s, '*');
	if(p == nil)
		p = s;
	while(*p && !isalpha(*p))
		p++;
	if(strlen(p) < 2)
{
		return -1;
}
	memmove(s, p, strlen(p)+1);

	/* strip suffix of punctuation */
	p = s+strlen(s);
	op = p;
	while(p > s && (uchar)p[-1]<0x80 && !isalpha(p[-1]))
		p--;

	/* chop punctuation */
	if(p > s){
		/* free!!! -> free! */
		if(p+1 < op){
			p[1] = 0;
			return 0;
		}
		/* free! -> free */
		if(p < op){
			p[0] = 0;
			return 0;
		}
	}

	mix = mix1 = 0;
	if(isupper(s[0]))
		mix = 1;
	for(p=s+1; *p; p++)
		if(isupper(*p)){
			mix1 = 1;
			break;
		}

	/* turn FREE into Free */
	if(mix1){
		for(p=s+1; *p; p++)
			if(isupper(*p))
				*p += 'a'-'A';
		return 0;
	}

	/* turn Free into free */
	if(mix){
		*s += 'a'-'A';
		return 0;
	}
	return -1;
}
