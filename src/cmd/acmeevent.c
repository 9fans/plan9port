#include <u.h>
#include <libc.h>
#include <bio.h>


int
getn(Biobuf *b)
{
	int c, n;

	n = 0;
	while((c = Bgetc(b)) != -1 && '0'<=c && c<='9')
		n = n*10+c-'0';
	if(c != ' ')
		sysfatal("bad number syntax");
	return n;
}

char*
getrune(Biobuf *b, char *p)
{
	int c;
	char *q;

	c = Bgetc(b);
	if(c == -1)
		sysfatal("eof");
	q = p;
	*q++ = c;
	if(c >= Runeself){
		while(!fullrune(p, q-p)){
			c = Bgetc(b);
			if(c == -1)
				sysfatal("eof");
			*q++ = c;
		}
	}
	return q;
}

void
getevent(Biobuf *b, int *c1, int *c2, int *q0, int *q1, int *flag, int *nr, char *buf)
{
	int i;
	char *p;

	*c1 = Bgetc(b);
	if(*c1 == -1)
		exits(0);
	*c2 = Bgetc(b);
	*q0 = getn(b);
	*q1 = getn(b);
	*flag = getn(b);
	*nr = getn(b);
	if(*nr >= 256)
		sysfatal("event string too long");
	p = buf;
	for(i=0; i<*nr; i++)
		p = getrune(b, p);
	*p = 0;
	if(Bgetc(b) != '\n')
		sysfatal("expected newline");
}

void
main(void)
{
	int c1, c2, q0, q1, eq0, eq1, flag, nr, x;
	Biobuf b;
	char buf[2000], buf2[2000], buf3[2000];

	doquote = needsrcquote;
	quotefmtinstall();
	Binit(&b, 0, OREAD);
	for(;;){
		getevent(&b, &c1, &c2, &q0, &q1, &flag, &nr, buf);
		eq0 = q0;
		eq1 = q1;
		buf2[0] = 0;
		buf3[0] = 0;
		if(flag & 2){
			/* null string with non-null expansion */
			getevent(&b, &x, &x, &eq0, &eq1, &x, &nr, buf);
		}
		if(flag & 8){
			/* chorded argument */
			getevent(&b, &x, &x, &x, &x, &x, &x, buf2);
			getevent(&b, &x, &x, &x, &x, &x, &x, buf3);
		}
		print("event %c %c %d %d %d %d %d %d %q %q %q\n",
			c1, c2, q0, q1, eq0, eq1, flag, nr, buf, buf2, buf3);
	}
}
