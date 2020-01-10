#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include <plumb.h>
#include "flayer.h"
#include "samterm.h"

#define	HSIZE	3	/* Type + short count */
Header	h;
uchar	indata[DATASIZE+1];	/* room for NUL */
uchar	outdata[DATASIZE];
short	outcount;
int	hversion;
int	hostfd[2];
int	exiting;

void	inmesg(Hmesg, int);
int	inshort(int);
long	inlong(int);
vlong	invlong(int);
void	hsetdot(int, long, long);
void	hmoveto(int, long);
void	hsetsnarf(int);
void	hplumb(int);
void	clrlock(void);
int	snarfswap(char*, int, char**);


void
rcv(void)
{
	int c;
	static int state = 0;
	static int count = 0;
	static int i = 0;
	static int errs = 0;

	if(protodebug) print("rcv in\n");
	while((c=rcvchar()) != -1){
		if(protodebug) print(".");
		switch(state){
		case 0:
			h.type = c;
			state++;
			break;

		case 1:
			h.count0 = c;
			state++;
			break;

		case 2:
			h.count1 = c;
			count = h.count0|(h.count1<<8);
			i = 0;
			if(count > DATASIZE){
				if(++errs < 5){
					dumperrmsg(count, h.type, h.count0, c);
					state = 0;
					continue;
				}
				fprint(2, "type %d count %d\n", h.type, count);
				panic("count>DATASIZE");
			}
			if(count == 0)
				goto zerocount;
			state++;
			break;

		case 3:
			indata[i++] = c;
			if(i == count){
		zerocount:
				indata[i] = 0;
				inmesg(h.type, count);
				state = count = 0;
				continue;
			}
			break;
		}
		if(protodebug) print(":");
	}

	if(protodebug) print("rcv out\n");
}

Text *
whichtext(int tg)
{
	int i;

	for(i=0; i<nname; i++)
		if(tag[i] == tg)
			return text[i];
	panic("whichtext");
	return 0;
}

void
inmesg(Hmesg type, int count)
{
	Text *t;
	int i, m;
	long l;
	Flayer *lp;

	m = inshort(0);
	l = inlong(2);
	switch(type){
	case -1:
		panic("rcv error");
	default:
		fprint(2, "type %d\n", type);
		panic("rcv unknown");

	case Hversion:
		hversion = m;
		break;

	case Hbindname:
		l = invlong(2);		/* for 64-bit pointers */
		if((i=whichmenu(m)) < 0)
			break;
		/* in case of a race, a bindname may already have occurred */
		if((t=whichtext(m)) == 0)
			t=(Text *)l;
		else	/* let the old one win; clean up the new one */
			while(((Text *)l)->nwin>0)
				closeup(&((Text *)l)->l[((Text *)l)->front]);
		text[i] = t;
		text[i]->tag = m;
		break;

	case Hcurrent:
		if(whichmenu(m)<0)
			break;
		t = whichtext(m);
		i = which && ((Text *)which->user1)==&cmd && m!=cmd.tag;
		if(t==0 && (t = sweeptext(0, m))==0)
			break;
		if(t->l[t->front].textfn==0)
			panic("Hcurrent");
		lp = &t->l[t->front];
		if(i){
			flupfront(lp);
			flborder(lp, 0);
			work = lp;
		}else
			current(lp);
		break;

	case Hmovname:
		if((m=whichmenu(m)) < 0)
			break;
		t = text[m];
		l = tag[m];
		i = name[m][0];
		text[m] = 0;	/* suppress panic in menudel */
		menudel(m);
		if(t == &cmd)
			m = 0;
		else{
			if (nname>0 && text[0]==&cmd)
				m = 1;
			else m = 0;
			for(; m<nname; m++)
				if(strcmp((char*)indata+2, (char*)name[m]+1)<0)
					break;
		}
		menuins(m, indata+2, t, i, (int)l);
		break;

	case Hgrow:
		if(whichmenu(m) >= 0)
			hgrow(m, l, inlong(6), 1);
		break;

	case Hnewname:
		menuins(0, (uchar *)"", (Text *)0, ' ', m);
		break;

	case Hcheck0:
		i = whichmenu(m);
		if(i>=0) {
			t = text[i];
			if(t)
				t->lock++;
			outTs(Tcheck, m);
		}
		break;

	case Hcheck:
		i = whichmenu(m);
		if(i>=0) {
			t = text[i];
			if(t && t->lock)
				t->lock--;
			hcheck(m);
		}
		break;

	case Hunlock:
		clrlock();
		break;

	case Hdata:
		if(whichmenu(m) >= 0)
			l += hdata(m, l, indata+6, count-6);
	Checkscroll:
		if(m == cmd.tag){
			for(i=0; i<NL; i++){
				lp = &cmd.l[i];
				if(lp->textfn)
					center(lp, l>=0? l : lp->p1);
			}
		}
		break;

	case Horigin:
		if(whichmenu(m) >= 0)
			horigin(m, l);
		break;

	case Hunlockfile:
		if(whichmenu(m)>=0 && (t = whichtext(m))->lock){
			--t->lock;
			l = -1;
			goto Checkscroll;
		}
		break;

	case Hsetdot:
		if(whichmenu(m) >= 0)
			hsetdot(m, l, inlong(6));
		break;

	case Hgrowdata:
		if(whichmenu(m)<0)
			break;
		hgrow(m, l, inlong(6), 0);
		whichtext(m)->lock++;	/* fake the request */
		l += hdata(m, l, indata+10, count-10);
		goto Checkscroll;

	case Hmoveto:
		if(whichmenu(m)>=0)
			hmoveto(m, l);
		break;

	case Hclean:
		if((m = whichmenu(m)) >= 0)
			name[m][0] = ' ';
		break;

	case Hdirty:
		if((m = whichmenu(m))>=0)
			name[m][0] = '\'';
		break;

	case Hdelname:
		if((m=whichmenu(m)) >= 0)
			menudel(m);
		break;

	case Hcut:
		if(whichmenu(m) >= 0)
			hcut(m, l, inlong(6));
		break;

	case Hclose:
		if(whichmenu(m)<0 || (t = whichtext(m))==0)
			break;
		l = t->nwin;
		for(i = 0,lp = t->l; l>0 && i<NL; i++,lp++)
			if(lp->textfn){
				closeup(lp);
				--l;
			}
		break;

	case Hsetpat:
		setpat((char *)indata);
		break;

	case Hsetsnarf:
		hsetsnarf(m);
		break;

	case Hsnarflen:
		snarflen = inlong(0);
		break;

	case Hack:
		outT0(Tack);
		break;

	case Hexit:
		exiting = 1;
		outT0(Texit);
		threadexitsall(nil);
		break;

	case Hplumb:
		hplumb(m);
		break;
	}
}

void
setlock(void)
{
	hostlock++;
	setcursor(mousectl, cursor = &lockarrow);
}

void
clrlock(void)
{
	hasunlocked = 1;
	if(hostlock > 0)
		hostlock--;
	if(hostlock == 0)
		setcursor(mousectl, cursor=(Cursor *)0);
}

void
startfile(Text *t)
{
	outTsv(Tstartfile, t->tag, (vlong)(uintptr)t);		/* for 64-bit pointers */
	setlock();
}

void
startnewfile(int type, Text *t)
{
	t->tag = Untagged;
	outTv(type, (vlong)(uintptr)t);				/* for 64-bit pointers */
}

int
inshort(int n)
{
	return indata[n]|(indata[n+1]<<8);
}

long
inlong(int n)
{
	return indata[n]|(indata[n+1]<<8)|
		((long)indata[n+2]<<16)|((long)indata[n+3]<<24);
}

vlong
invlong(int n)
{
	vlong v;

	v = (indata[n+7]<<24) | (indata[n+6]<<16) | (indata[n+5]<<8) | indata[n+4];
	v = (v<<16) | (indata[n+3]<<8) | indata[n+2];
	v = (v<<16) | (indata[n+1]<<8) | indata[n];
	return v;
}

void
outT0(Tmesg type)
{
	outstart(type);
	outsend();
}

void
outTl(Tmesg type, long l)
{
	outstart(type);
	outlong(l);
	outsend();
}

void
outTs(Tmesg type, int s)
{
	outstart(type);
	outshort(s);
	outsend();
}

void
outTss(Tmesg type, int s1, int s2)
{
	outstart(type);
	outshort(s1);
	outshort(s2);
	outsend();
}

void
outTsll(Tmesg type, int s1, long l1, long l2)
{
	outstart(type);
	outshort(s1);
	outlong(l1);
	outlong(l2);
	outsend();
}

void
outTsl(Tmesg type, int s1, long l1)
{
	outstart(type);
	outshort(s1);
	outlong(l1);
	outsend();
}

void
outTsv(Tmesg type, int s1, vlong v1)
{
	outstart(type);
	outshort(s1);
	outvlong(v1);
	outsend();
}

void
outTv(Tmesg type, vlong v1)
{
	outstart(type);
	outvlong(v1);
	outsend();
}

void
outTslS(Tmesg type, int s1, long l1, Rune *s)
{
	char buf[DATASIZE*3+1];
	char *c;

	outstart(type);
	outshort(s1);
	outlong(l1);
	c = buf;
	while(*s)
		c += runetochar(c, s++);
	*c++ = 0;
	outcopy(c-buf, (uchar *)buf);
	outsend();
}

void
outTsls(Tmesg type, int s1, long l1, int s2)
{
	outstart(type);
	outshort(s1);
	outlong(l1);
	outshort(s2);
	outsend();
}

void
outstart(Tmesg type)
{
	outdata[0] = type;
	outcount = 0;
}

void
outcopy(int count, uchar *data)
{
	while(count--)
		outdata[HSIZE+outcount++] = *data++;
}

void
outshort(int s)
{
	uchar buf[2];

	buf[0]=s;
	buf[1]=s>>8;
	outcopy(2, buf);
}

void
outlong(long l)
{
	uchar buf[4];

	buf[0]=l;
	buf[1]=l>>8;
	buf[2]=l>>16;
	buf[3]=l>>24;
	outcopy(4, buf);
}

void
outvlong(vlong v)
{
	int i;
	uchar buf[8];

	for(i = 0; i < sizeof(buf); i++){
		buf[i] = v;
		v >>= 8;
	}

	outcopy(8, buf);
}

void
outsend(void)
{
	if(outcount>DATASIZE-HSIZE)
		panic("outcount>sizeof outdata");
	outdata[1]=outcount;
	outdata[2]=outcount>>8;
	if(write(hostfd[1], (char *)outdata, outcount+HSIZE)!=outcount+HSIZE)
		panic("write error");
}


void
hsetdot(int m, long p0, long p1)
{
	Text *t = whichtext(m);
	Flayer *l = &t->l[t->front];

	flushtyping(1);
	flsetselect(l, p0, p1);
}

void
horigin(int m, long p0)
{
	Text *t = whichtext(m);
	Flayer *l = &t->l[t->front];
	long a;
	ulong n;
	Rune *r;

	if(!flprepare(l)){
		l->origin = p0;
		return;
	}
	a = p0-l->origin;
	if(a>=0 && a<l->f.nchars)
		frdelete(&l->f, 0, a);
	else if(a<0 && -a<l->f.nchars){
		r = rload(&t->rasp, p0, l->origin, &n);
		frinsert(&l->f, r, r+n, 0);
	}else
		frdelete(&l->f, 0, l->f.nchars);
	l->origin = p0;
	scrdraw(l, t->rasp.nrunes);
	if(l->visible==Some)
		flrefresh(l, l->entire, 0);
	hcheck(m);
}

void
hmoveto(int m, long p0)
{
	Text *t = whichtext(m);
	Flayer *l = &t->l[t->front];

	if(p0<l->origin || p0-l->origin>l->f.nchars*9/10)
		outTsll(Torigin, m, p0, 2L);
}

void
hcheck(int m)
{
	Flayer *l;
	Text *t;
	int reqd = 0, i;
	long n, nl, a;
	Rune *r;

	if(m == Untagged)
		return;
	t = whichtext(m);
	if(t == 0)		/* possible in a half-built window */
		return;
	for(l = &t->l[0], i = 0; i<NL; i++, l++){
		if(l->textfn==0 || !flprepare(l))	/* BUG: don't
							   need this if BUG below
							   is fixed */
			continue;
		a = t->l[i].origin;
		n = rcontig(&t->rasp, a, a+l->f.nchars, 1);
		if(n<l->f.nchars)	/* text missing in middle of screen */
			a+=n;
		else{			/* text missing at end of screen? */
        Again:
		 	if(l->f.lastlinefull)
				goto Checksel;	/* all's well */
			a = t->l[i].origin+l->f.nchars;
			n = t->rasp.nrunes-a;
			if(n==0)
				goto Checksel;
			if(n>TBLOCKSIZE)
				n = TBLOCKSIZE;
			n = rcontig(&t->rasp, a, a+n, 1);
			if(n>0){
				rload(&t->rasp, a, a+n, 0);
				nl = l->f.nchars;
				r = scratch;
				flinsert(l, r, r+n, l->origin+nl);
				if(nl == l->f.nchars)	/* made no progress */
					goto Checksel;
				goto Again;
			}
		}
		if(!reqd){
			n = rcontig(&t->rasp, a, a+TBLOCKSIZE, 0);
			if(n <= 0)
				panic("hcheck request==0");
			outTsls(Trequest, m, a, (int)n);
			outTs(Tcheck, m);
			t->lock++;	/* for the Trequest */
			t->lock++;	/* for the Tcheck */
			reqd++;
		}
	    Checksel:
		flsetselect(l, l->p0, l->p1);
	}
}

void
flnewlyvisible(Flayer *l)
{
	hcheck(((Text *)l->user1)->tag);
}

void
hsetsnarf(int nc)
{
	char *s2;
	char *s1;
	int i;
	int n;

	setcursor(mousectl, &deadmouse);
	s2 = alloc(nc+1);
	for(i=0; i<nc; i++)
		s2[i] = getch();
	s2[nc] = 0;
	n = snarfswap(s2, nc, &s1);
	if(n >= 0){
		if(!s1)
			n = 0;
		if(n > SNARFSIZE){
			s1 = strdup("<snarf too long>");
			if (!s1)
				panic("strdup");
			n = strlen(s1);
		}else{
			s1 = realloc(s1, n+1);
			if (!s1)
				panic("realloc");
			s1[n] = 0;
		}
		snarflen = n;
		outTs(Tsetsnarf, n);
		if(n>0 && write(hostfd[1], s1, n)!=n)
			panic("snarf write error");
		free(s1);
	}else
		outTs(Tsetsnarf, 0);
	free(s2);
	setcursor(mousectl, cursor);
}

void
hplumb(int nc)
{
	int i;
	char *s;
	Plumbmsg *m;

	s = alloc(nc);
	for(i=0; i<nc; i++)
		s[i] = getch();
	if(plumbfd > 0){
		m = plumbunpack(s, nc);
		if(m != 0)
			plumbsend(plumbfd, m);
		plumbfree(m);
	}
	free(s);
}

void
hgrow(int m, long a, long new, int req)
{
	int i;
	Flayer *l;
	Text *t = whichtext(m);
	long o, b;

	if(new <= 0)
		panic("hgrow");
	rresize(&t->rasp, a, 0L, new);
	for(l = &t->l[0], i = 0; i<NL; i++, l++){
		if(l->textfn == 0)
			continue;
		o = l->origin;
		b = a-o-rmissing(&t->rasp, o, a);
		if(a < o)
			l->origin+=new;
		if(a < l->p0)
			l->p0+=new;
		if(a < l->p1)
			l->p1+=new;
		/* must prevent b temporarily becoming unsigned */
		if(!req || a<o || (b>0 && b>l->f.nchars) ||
		    (l->f.nchars==0 && a-o>0))
			continue;
		if(new>TBLOCKSIZE)
			new = TBLOCKSIZE;
		outTsls(Trequest, m, a, (int)new);
		t->lock++;
		req = 0;
	}
}

int
hdata1(Text *t, long a, Rune *r, int len)
{
	int i;
	Flayer *l;
	long o, b;

	for(l = &t->l[0], i=0; i<NL; i++, l++){
		if(l->textfn==0)
			continue;
		o = l->origin;
		b = a-o-rmissing(&t->rasp, o, a);
		/* must prevent b temporarily becoming unsigned */
		if(a<o || (b>0 && b>l->f.nchars))
			continue;
		flinsert(l, r, r+len, o+b);
	}
	rdata(&t->rasp, a, a+len, r);
	rclean(&t->rasp);
	return len;
}

int
hdata(int m, long a, uchar *s, int len)
{
	int i, w;
	Text *t = whichtext(m);
	Rune buf[DATASIZE], *r;

	if(t->lock)
		--t->lock;
	if(len == 0)
		return 0;
	r = buf;
	for(i=0; i<len; i+=w,s+=w)
		w = chartorune(r++, (char*)s);
	return hdata1(t, a, buf, r-buf);
}

int
hdatarune(int m, long a, Rune *r, int len)
{
	Text *t = whichtext(m);

	if(t->lock)
		--t->lock;
	if(len == 0)
		return 0;
	return hdata1(t, a, r, len);
}

void
hcut(int m, long a, long old)
{
	Flayer *l;
	Text *t = whichtext(m);
	int i;
	long o, b;

	if(t->lock)
		--t->lock;
	for(l = &t->l[0], i = 0; i<NL; i++, l++){
		if(l->textfn == 0)
			continue;
		o = l->origin;
		b = a-o-rmissing(&t->rasp, o, a);
		/* must prevent b temporarily becoming unsigned */
		if((b<0 || b<l->f.nchars) && a+old>=o){
			fldelete(l, b<0? o : o+b,
			    a+old-rmissing(&t->rasp, o, a+old));
		}
		if(a+old<o)
			l->origin-=old;
		else if(a<=o)
			l->origin = a;
		if(a+old<l->p0)
			l->p0-=old;
		else if(a<=l->p0)
			l->p0 = a;
		if(a+old<l->p1)
			l->p1-=old;
		else if(a<=l->p1)
			l->p1 = a;
	}
	rresize(&t->rasp, a, old, 0L);
	rclean(&t->rasp);
}
