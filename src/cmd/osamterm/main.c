#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include "flayer.h"
#include "samterm.h"

Text	cmd;
Rune	*scratch;
long	nscralloc;
Cursor	*cursor;
Flayer	*which = 0;
Flayer	*work = 0;
long	snarflen;
long	typestart = -1;
long	typeend = -1;
long	typeesc = -1;
long	modified = 0;		/* strange lookahead for menus */
char	hostlock = 1;
char	hasunlocked = 0;
int	maxtab = 8;
int	chord;
int	autoindent;

#define chording 0	/* code here for reference but it causes deadlocks */

void
notifyf(void *a, char *msg)
{
	if(strcmp(msg, "interrupt") == 0)
		noted(NCONT);
	noted(NDFLT);
}

void
threadmain(int argc, char *argv[])
{
	int i, got, scr, w;
	Text *t;
	Rectangle r;
	Flayer *nwhich;

	/*
	 * sam is talking to us on fd 0 and 1.
	 * move these elsewhere so that if we accidentally
	 * use 0 and 1 in other code, nothing bad happens.
	 */
	dup(0, 3);
	dup(1, 4);
	hostfd[0] = 3;
	hostfd[1] = 4;
	close(0);
	close(1);
	open("/dev/null", OREAD);
	if(open("/dev/tty", OWRITE) < 0)
		open("/dev/null", OWRITE);

	notify(notifyf);

	if(protodebug) print("getscreen\n");
	getscreen(argc, argv);
	if(protodebug) print("iconinit\n");
	iconinit();
	if(protodebug) print("initio\n");
	initio();
	if(protodebug) print("scratch\n");
	scratch = alloc(100*RUNESIZE);
	nscralloc = 100;
	r = screen->r;
	r.max.y = r.min.y+Dy(r)/5;
	if(protodebug) print("flstart\n");
	flstart(screen->clipr);
	rinit(&cmd.rasp);
	flnew(&cmd.l[0], gettext, 1, &cmd);
	flinit(&cmd.l[0], r, font, cmdcols);
	cmd.nwin = 1;
	which = &cmd.l[0];
	cmd.tag = Untagged;
	outTs(Tversion, VERSION);
	startnewfile(Tstartcmdfile, &cmd);

	got = 0;
	if(protodebug) print("loop\n");
	for(;;got = waitforio()){
		if(hasunlocked && RESIZED())
			resize();
		if(got&(1<<RHost))
			rcv();
		if(got&(1<<RPlumb)){
			for(i=0; cmd.l[i].textfn==0; i++)
				;
			current(&cmd.l[i]);
			flsetselect(which, cmd.rasp.nrunes, cmd.rasp.nrunes);
			type(which, RPlumb);
		}
		if(got&(1<<RKeyboard))
			if(which)
				type(which, RKeyboard);
			else
				kbdblock();
		if(got&(1<<RMouse)){
			if(hostlock==2 || !ptinrect(mousep->xy, screen->r)){
				mouseunblock();
				continue;
			}
			nwhich = flwhich(mousep->xy);
			scr = which && ptinrect(mousep->xy, which->scroll);
			if(mousep->buttons)
				flushtyping(1);
			if(chording && chord==1 && !mousep->buttons)
				chord = 0;
			if(chording && chord)
				chord |= mousep->buttons;
			else if(mousep->buttons&1){
				if(nwhich){
					if(nwhich!=which)
						current(nwhich);
					else if(scr)
						scroll(which, 1);
					else{
						t=(Text *)which->user1;
						if(flselect(which)){
							outTsl(Tdclick, t->tag, which->p0);
							t->lock++;
						}else if(t!=&cmd)
							outcmd();
						if(mousep->buttons&1)
							chord = mousep->buttons;
					}
				}
			}else if((mousep->buttons&2) && which){
				if(scr)
					scroll(which, 2);
				else
					menu2hit();
			}else if((mousep->buttons&4)){
				if(scr)
					scroll(which, 3);
				else
					menu3hit();
			}
			mouseunblock();
		}
		if(chording && chord){
			t = (Text*)which->user1;
			if(!t->lock && !hostlock){
				w = which-t->l;
				if(chord&2){
					cut(t, w, 1, 1);
					chord &= ~2;
				}else if(chord&4){
					paste(t, w);
					chord &= ~4;
				}
			}
		}
	}
}

void
resize(void)
{
	int i;

	flresize(screen->clipr);
	for(i = 0; i<nname; i++)
		if(text[i])
			hcheck(text[i]->tag);
}

void
current(Flayer *nw)
{
	Text *t;

	if(which)
		flborder(which, 0);
	if(nw){
		flushtyping(1);
		flupfront(nw);
		flborder(nw, 1);
		buttons(Up);
		t = (Text *)nw->user1;
		t->front = nw-&t->l[0];
		if(t != &cmd)
			work = nw;
	}
	which = nw;
}

void
closeup(Flayer *l)
{
	Text *t=(Text *)l->user1;
	int m;

	m = whichmenu(t->tag);
	if(m < 0)
		return;
	flclose(l);
	if(l == which){
		which = 0;
		current(flwhich(Pt(0, 0)));
	}
	if(l == work)
		work = 0;
	if(--t->nwin == 0){
		rclear(&t->rasp);
		free((uchar *)t);
		text[m] = 0;
	}else if(l == &t->l[t->front]){
		for(m=0; m<NL; m++)	/* find one; any one will do */
			if(t->l[m].textfn){
				t->front = m;
				return;
			}
		panic("close");
	}
}

Flayer *
findl(Text *t)
{
	int i;
	for(i = 0; i<NL; i++)
		if(t->l[i].textfn==0)
			return &t->l[i];
	return 0;
}

void
duplicate(Flayer *l, Rectangle r, Font *f, int close)
{
	Text *t=(Text *)l->user1;
	Flayer *nl = findl(t);
	Rune *rp;
	ulong n;

	if(nl){
		flnew(nl, gettext, l->user0, (char *)t);
		flinit(nl, r, f, l->f.cols);
		nl->origin = l->origin;
		rp = (*l->textfn)(l, l->f.nchars, &n);
		flinsert(nl, rp, rp+n, l->origin);
		flsetselect(nl, l->p0, l->p1);
		if(close){
			flclose(l);
			if(l==which)
				which = 0;
		}else
			t->nwin++;
		current(nl);
		hcheck(t->tag);
	}
	setcursor(mousectl, cursor);
}

void
buttons(int updown)
{
	while(((mousep->buttons&7)!=0) != updown)
		getmouse();
}

int
getr(Rectangle *rp)
{
	Point p;
	Rectangle r;

	*rp = getrect(3, mousectl);
	if(rp->max.x && rp->max.x-rp->min.x<=5 && rp->max.y-rp->min.y<=5){
		p = rp->min;
		r = cmd.l[cmd.front].entire;
		*rp = screen->r;
		if(cmd.nwin==1){
			if (p.y <= r.min.y)
				rp->max.y = r.min.y;
			else if (p.y >= r.max.y)
				rp->min.y = r.max.y;
			if (p.x <= r.min.x)
				rp->max.x = r.min.x;
			else if (p.x >= r.max.x)
				rp->min.x = r.max.x;
		}
	}
	return rectclip(rp, screen->r) &&
	   rp->max.x-rp->min.x>100 && rp->max.y-rp->min.y>40;
}

void
snarf(Text *t, int w)
{
	Flayer *l = &t->l[w];

	if(l->p1>l->p0){
		snarflen = l->p1-l->p0;
		outTsll(Tsnarf, t->tag, l->p0, l->p1);
	}
}

void
cut(Text *t, int w, int save, int check)
{
	long p0, p1;
	Flayer *l;

	l = &t->l[w];
	p0 = l->p0;
	p1 = l->p1;
	if(p0 == p1)
		return;
	if(p0 < 0)
		panic("cut");
	if(save)
		snarf(t, w);
	outTsll(Tcut, t->tag, p0, p1);
	flsetselect(l, p0, p0);
	t->lock++;
	hcut(t->tag, p0, p1-p0);
	if(check)
		hcheck(t->tag);
}

void
paste(Text *t, int w)
{
	if(snarflen){
		cut(t, w, 0, 0);
		t->lock++;
		outTsl(Tpaste, t->tag, t->l[w].p0);
	}
}

void
scrorigin(Flayer *l, int but, long p0)
{
	Text *t=(Text *)l->user1;

	switch(but){
	case 1:
		outTsll(Torigin, t->tag, l->origin, p0);
		break;
	case 2:
		outTsll(Torigin, t->tag, p0, 1L);
		break;
	case 3:
		horigin(t->tag,p0);
	}
}

int
alnum(int c)
{
	/*
	 * Hard to get absolutely right.  Use what we know about ASCII
	 * and assume anything above the Latin control characters is
	 * potentially an alphanumeric.
	 */
	if(c<=' ')
		return 0;
	if(0x7F<=c && c<=0xA0)
		return 0;
	if(utfrune("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", c))
		return 0;
	return 1;
}

int
raspc(Rasp *r, long p)
{
	ulong n;
	rload(r, p, p+1, &n);
	if(n)
		return scratch[0];
	return 0;
}

long
ctlw(Rasp *r, long o, long p)
{
	int c;

	if(--p < o)
		return o;
	if(raspc(r, p)=='\n')
		return p;
	for(; p>=o && !alnum(c=raspc(r, p)); --p)
		if(c=='\n')
			return p+1;
	for(; p>o && alnum(raspc(r, p-1)); --p)
		;
	return p>=o? p : o;
}

long
ctlu(Rasp *r, long o, long p)
{
	if(--p < o)
		return o;
	if(raspc(r, p)=='\n')
		return p;
	for(; p-1>=o && raspc(r, p-1)!='\n'; --p)
		;
	return p>=o? p : o;
}

int
center(Flayer *l, long a)
{
	Text *t;

	t = l->user1;
	if(!t->lock && (a<l->origin || l->origin+l->f.nchars<a)){
		if(a > t->rasp.nrunes)
			a = t->rasp.nrunes;
		outTsll(Torigin, t->tag, a, 2L);
		return 1;
	}
	return 0;
}

int
thirds(Flayer *l, long a, int n)
{
	Text *t;
	Rectangle s;
	long lines;

	t = l->user1;
	if(!t->lock && (a<l->origin || l->origin+l->f.nchars<a)){
		if(a > t->rasp.nrunes)
			a = t->rasp.nrunes;
		s = insetrect(l->scroll, 1);
		lines = (n*(s.max.y-s.min.y)/l->f.font->height+1)/3;
		if (lines < 2)
			lines = 2;
		outTsll(Torigin, t->tag, a, lines);
		return 1;
	}
	return 0;
}

int
onethird(Flayer *l, long a)
{
	return thirds(l, a, 1);
}

int
twothirds(Flayer *l, long a)
{
	return thirds(l, a, 2);
}

void
flushtyping(int clearesc)
{
	Text *t;
	ulong n;

	if(clearesc)
		typeesc = -1;
	if(typestart == typeend) {
		modified = 0;
		return;
	}
	t = which->user1;
	if(t != &cmd)
		modified = 1;
	rload(&t->rasp, typestart, typeend, &n);
	scratch[n] = 0;
	if(t==&cmd && typeend==t->rasp.nrunes && scratch[typeend-typestart-1]=='\n'){
		setlock();
		outcmd();
	}
	outTslS(Ttype, t->tag, typestart, scratch);
	typestart = -1;
	typeend = -1;
}

#define	BACKSCROLLKEY	Kup
#define	ENDKEY	Kend
#define	ESC		0x1B
#define	HOMEKEY	Khome
#define	LEFTARROW	Kleft
#define	LINEEND	0x05
#define	LINESTART	0x01
#define	PAGEDOWN	Kpgdown
#define	PAGEUP	Kpgup
#define	RIGHTARROW	Kright
#define	SCROLLKEY	Kdown
#define	CUT	(Kcmd+'x')
#define	COPY	(Kcmd+'c')
#define	PASTE	(Kcmd+'v')

int
nontypingkey(int c)
{
	switch(c){
	case BACKSCROLLKEY:
	case ENDKEY:
	case HOMEKEY:
	case LEFTARROW:
	case LINEEND:
	case LINESTART:
	case PAGEDOWN:
	case PAGEUP:
	case RIGHTARROW:
	case SCROLLKEY:
	case CUT:
	case COPY:
	case PASTE:
		return 1;
	}
	return 0;
}

void
type(Flayer *l, int res)	/* what a bloody mess this is */
{
	Text *t = (Text *)l->user1;
	Rune buf[100];
	Rune *p = buf;
	int c, backspacing;
	long a, a0;
	int scrollkey;

	scrollkey = 0;
	if(res == RKeyboard)
		scrollkey = nontypingkey(qpeekc());	/* ICK */

	if(hostlock || t->lock){
		kbdblock();
		return;
	}
	a = l->p0;
	if(a!=l->p1 && !scrollkey){
		flushtyping(1);
		cut(t, t->front, 1, 1);
		return;	/* it may now be locked */
	}
	backspacing = 0;
	while((c = kbdchar())>0){
		if(res == RKeyboard){
			if(nontypingkey(c) || c==ESC)
				break;
			/* backspace, ctrl-u, ctrl-w, del */
			if(c=='\b' || c==0x15 || c==0x17 || c==0x7F){
				backspacing = 1;
				break;
			}
		}
		*p++ = c;
		if(autoindent)
		if(c == '\n'){
			/* autoindent */
			int cursor, ch;
			cursor = ctlu(&t->rasp, 0, a+(p-buf)-1);
			while(p < buf+nelem(buf)){
				ch = raspc(&t->rasp, cursor++);
				if(ch == ' ' || ch == '\t')
					*p++ = ch;
				else
					break;
			}
		}
		if(c == '\n' || p >= buf+nelem(buf))
			break;
	}
	if(p > buf){
		if(typestart < 0)
			typestart = a;
		if(typeesc < 0)
			typeesc = a;
		hgrow(t->tag, a, p-buf, 0);
		t->lock++;	/* pretend we Trequest'ed for hdatarune*/
		hdatarune(t->tag, a, buf, p-buf);
		a += p-buf;
		l->p0 = a;
		l->p1 = a;
		typeend = a;
		if(c=='\n' || typeend-typestart>100)
			flushtyping(0);
		onethird(l, a);
	}
	if(c==SCROLLKEY || c==PAGEDOWN){
		flushtyping(0);
		center(l, l->origin+l->f.nchars+1);
	}else if(c==BACKSCROLLKEY || c==PAGEUP){
		flushtyping(0);
		a0 = l->origin-l->f.nchars;
		if(a0 < 0)
			a0 = 0;
		center(l, a0);
	}else if(c == RIGHTARROW){
		flushtyping(0);
		a0 = l->p0;
		if(a0 < t->rasp.nrunes)
			a0++;
		flsetselect(l, a0, a0);
		center(l, a0);
	}else if(c == LEFTARROW){
		flushtyping(0);
		a0 = l->p0;
		if(a0 > 0)
			a0--;
		flsetselect(l, a0, a0);
		center(l, a0);
	}else if(c == HOMEKEY){
		flushtyping(0);
		center(l, 0);
	}else if(c == ENDKEY){
		flushtyping(0);
		center(l, t->rasp.nrunes);
	}else if(c == LINESTART || c == LINEEND){
		flushtyping(1);
		if(c == LINESTART)
			while(a > 0 && raspc(&t->rasp, a-1)!='\n')
				a--;
		else
			while(a < t->rasp.nrunes && raspc(&t->rasp, a)!='\n')
				a++;
		l->p0 = l->p1 = a;
		for(l=t->l; l<&t->l[NL]; l++)
			if(l->textfn)
				flsetselect(l, l->p0, l->p1);
	}else if(backspacing && !hostlock){
		/* backspacing immediately after outcmd(): sorry */
		if(l->f.p0>0 && a>0){
			switch(c){
			case '\b':
			case 0x7F:	/* del */
				l->p0 = a-1;
				break;
			case 0x15:	/* ctrl-u */
				l->p0 = ctlu(&t->rasp, l->origin, a);
				break;
			case 0x17:	/* ctrl-w */
				l->p0 = ctlw(&t->rasp, l->origin, a);
				break;
			}
			l->p1 = a;
			if(l->p1 != l->p0){
				/* cut locally if possible */
				if(typestart<=l->p0 && l->p1<=typeend){
					t->lock++;	/* to call hcut */
					hcut(t->tag, l->p0, l->p1-l->p0);
					/* hcheck is local because we know rasp is contiguous */
					hcheck(t->tag);
				}else{
					flushtyping(0);
					cut(t, t->front, 0, 1);
				}
			}
			if(typeesc >= l->p0)
				typeesc = l->p0;
			if(typestart >= 0){
				if(typestart >= l->p0)
					typestart = l->p0;
				typeend = l->p0;
				if(typestart == typeend){
					typestart = -1;
					typeend = -1;
					modified = 0;
				}
			}
		}
	}else{
		if(c==ESC && typeesc>=0){
			l->p0 = typeesc;
			l->p1 = a;
			flushtyping(1);
		}
		for(l=t->l; l<&t->l[NL]; l++)
			if(l->textfn)
				flsetselect(l, l->p0, l->p1);
		switch(c) {
		case CUT:
			flushtyping(0);
			cut(t, t->front, 1, 1);
			break;
		case COPY:
			flushtyping(0);
			snarf(t, t->front);
			break;
		case PASTE:
			flushtyping(0);
			paste(t, t->front);
			break;
		}
	}
}


void
outcmd(void){
	if(work)
		outTsll(Tworkfile, ((Text *)work->user1)->tag, work->p0, work->p1);
}

void
panic(char *s)
{
	panic1(display, s);
}

void
panic1(Display *d, char *s)
{
	fprint(2, "samterm:panic: ");
	perror(s);
	abort();
}

Rune*
gettext(Flayer *l, long n, ulong *np)
{
	Text *t;

	t = l->user1;
	rload(&t->rasp, l->origin, l->origin+n, np);
	return scratch;
}

long
scrtotal(Flayer *l)
{
	return ((Text *)l->user1)->rasp.nrunes;
}

void*
alloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == 0)
		panic("alloc");
	memset(p, 0, n);
	return p;
}
