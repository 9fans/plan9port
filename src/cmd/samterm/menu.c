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

uchar	**name;	/* first byte is ' ' or '\'': modified state */
Text	**text;	/* pointer to Text associated with file */
ushort	*tag;		/* text[i].tag, even if text[i] not defined */
int	nname;
int	mname;
int	mw;

char	*genmenu3(int);
char	*genmenu2(int);
char	*genmenu2c(int);

enum Menu2
{
	Cut,
	Paste,
	Snarf,
	Plumb,
	Look,
	Exch,
	Search,
	NMENU2 = Search,
	Send = Search,
	NMENU2C
};

enum Menu3
{
	New,
	Zerox,
	Resize,
	Close,
	Write,
	NMENU3
};

char	*menu2str[] = {
	"cut",
	"paste",
	"snarf",
	"plumb",
	"look",
	"<rio>",
	0,		/* storage for last pattern */
};

char	*menu3str[] = {
	"new",
	"zerox",
	"resize",
	"close",
	"write"
};

Menu	menu2 =	{0, genmenu2};
Menu	menu2c ={0, genmenu2c};
Menu	menu3 =	{0, genmenu3};

void
menu2hit(void)
{
	Text *t=(Text *)which->user1;
	int w = which-t->l;
	int m;

	if(hversion==0 || plumbfd<0)
		menu2str[Plumb] = "(plumb)";
	m = menuhit(2, mousectl, t==&cmd? &menu2c : &menu2, nil);
	if(hostlock || t->lock)
		return;

	switch(m){
	case Cut:
		cut(t, w, 1, 1);
		break;

	case Paste:
		paste(t, w);
		break;

	case Snarf:
		snarf(t, w);
		break;

	case Plumb:
		if(hversion > 0)
			outTsll(Tplumb, t->tag, which->p0, which->p1);
		break;

	case Exch:
		snarf(t, w);
		outT0(Tstartsnarf);
		setlock();
		break;

	case Look:
		outTsll(Tlook, t->tag, which->p0, which->p1);
		setlock();
		break;

	case Search:
		outcmd();
		if(t==&cmd)
			outTsll(Tsend, 0 /*ignored*/, which->p0, which->p1);
		else
			outT0(Tsearch);
		setlock();
		break;
	}
}

void
menu3hit(void)
{
	Rectangle r;
	Flayer *l;
	int m, i;
	Text *t;

	mw = -1;
	m = menuhit(3, mousectl, &menu3, nil);
	switch(m){
	case -1:
		break;

	case New:
		if(!hostlock)
			sweeptext(1, 0);
		break;

	case Zerox:
	case Resize:
		if(!hostlock){
			setcursor(mousectl, &bullseye);
			buttons(Down);
			if((mousep->buttons&4) && (l = flwhich(mousep->xy)) && getr(&r))
				duplicate(l, r, l->f.font, m==Resize);
			else
				setcursor(mousectl, cursor);
			buttons(Up);
		}
		break;

	case Close:
		if(!hostlock){
			setcursor(mousectl, &bullseye);
			buttons(Down);
			if((mousep->buttons&4) && (l = flwhich(mousep->xy)) && !hostlock){
				t=(Text *)l->user1;
				if (t->nwin>1)
					closeup(l);
				else if(t!=&cmd) {
					outTs(Tclose, t->tag);
					setlock();
				}
			}
			setcursor(mousectl, cursor);
			buttons(Up);
		}
		break;

	case Write:
		if(!hostlock){
			setcursor(mousectl, &bullseye);
			buttons(Down);
			if((mousep->buttons&4) && (l = flwhich(mousep->xy))){
				outTs(Twrite, ((Text *)l->user1)->tag);
				setlock();
			}else
				setcursor(mousectl, cursor);
			buttons(Up);
		}
		break;

	default:
		if(t = text[m-NMENU3]){
			i = t->front;
			if(t->nwin==0 || t->l[i].textfn==0)
				return;	/* not ready yet; try again later */
			if(t->nwin>1 && which==&t->l[i])
				do
					if(++i==NL)
						i = 0;
				while(i!=t->front && t->l[i].textfn==0);
			current(&t->l[i]);
		}else if(!hostlock)
			sweeptext(0, tag[m-NMENU3]);
		break;
	}
}


Text *
sweeptext(int new, int tag)
{
	Rectangle r;
	Text *t;

	if(getr(&r) && (t = malloc(sizeof(Text)))){
		memset((void*)t, 0, sizeof(Text));
		current((Flayer *)0);
		flnew(&t->l[0], gettext, 0, (char *)t);
		flinit(&t->l[0], r, font, maincols);	/*bnl*/
		t->nwin = 1;
		rinit(&t->rasp);
		if(new)
			startnewfile(Tstartnewfile, t);
		else{
			rinit(&t->rasp);
			t->tag = tag;
			startfile(t);
		}
		return t;
	}
	return 0;
}

int
whichmenu(int tg)
{
	int i;

	for(i=0; i<nname; i++)
		if(tag[i] == tg)
			return i;
	return -1;
}

void
menuins(int n, uchar *s, Text *t, int m, int tg)
{
	int i;

	if(nname == mname){
		if(mname == 0)
			mname = 32;
		else
			mname *= 2;
		name = realloc(name, sizeof(name[0])*mname);
		text = realloc(text, sizeof(text[0])*mname);
		tag = realloc(tag, sizeof(tag[0])*mname);
		if(name==nil || text==nil || tag==nil)
			panic("realloc");
	}
	for(i=nname; i>n; --i)
		name[i]=name[i-1], text[i]=text[i-1], tag[i]=tag[i-1];
	text[n] = t;
	tag[n] = tg;
	name[n] = alloc(strlen((char*)s)+2);
	name[n][0] = m;
	strcpy((char*)name[n]+1, (char*)s);
	nname++;
	menu3.lasthit = n+NMENU3;
}

void
menudel(int n)
{
	int i;

	if(nname==0 || n>=nname || text[n])
		panic("menudel");
	free(name[n]);
	--nname;
	for(i = n; i<nname; i++)
		name[i]=name[i+1], text[i]=text[i+1], tag[i]=tag[i+1];
}

void
setpat(char *s)
{
	static char pat[17];

	pat[0] = '/';
	strncpy(pat+1, s, 15);
	menu2str[Search] = pat;
}

#define	NBUF	64
static uchar buf[NBUF*UTFmax]={' ', ' ', ' ', ' '};

char *
paren(char *s)
{
	uchar *t = buf;

	*t++ = '(';
	do; while(*t++ = *s++);
	t[-1] = ')';
	*t = 0;
	return (char *)buf;
}
char*
genmenu2(int n)
{
	Text *t=(Text *)which->user1;
	char *p;
	if(n>=NMENU2+(menu2str[Search]!=0))
		return 0;
	p = menu2str[n];
	if(!hostlock && !t->lock || n==Search || n==Look)
		return p;
	return paren(p);
}
char*
genmenu2c(int n)
{
	Text *t=(Text *)which->user1;
	char *p;
	if(n >= NMENU2C)
		return 0;
	if(n == Send)
		p="send";
	else
		p = menu2str[n];
	if(!hostlock && !t->lock)
		return p;
	return paren(p);
}
char *
genmenu3(int n)
{
	Text *t;
	int c, i, k, l, w;
	Rune r;
	char *p;

	if(n >= NMENU3+nname)
		return 0;
	if(n < NMENU3){
		p = menu3str[n];
		if(hostlock)
			p = paren(p);
		return p;
	}
	n -= NMENU3;
	if(n == 0)	/* unless we've been fooled, this is cmd */
		return (char *)&name[n][1];
	if(mw == -1){
		mw = 7;	/* strlen("~~sam~~"); */
		for(i=1; i<nname; i++){
			w = utflen((char*)name[i]+1)+4;	/* include "'+. " */
			if(w > mw)
				mw = w;
		}
	}
	if(mw > NBUF)
		mw = NBUF;
	t = text[n];
	buf[0] = name[n][0];
	buf[1] = '-';
	buf[2] = ' ';
	buf[3] = ' ';
	if(t){
		if(t->nwin == 1)
			buf[1] = '+';
		else if(t->nwin > 1)
			buf[1] = '*';
		if(work && t==(Text *)work->user1) {
			buf[2]= '.';
			if(modified)
				buf[0] = '\'';
		}
	}
	l = utflen((char*)name[n]+1);
	if(l > NBUF-4-2){
		i = 4;
		k = 1;
		while(i < NBUF/2){
			k += chartorune(&r, (char*)name[n]+k);
			i++;
		}
		c = name[n][k];
		name[n][k] = 0;
		strcpy((char*)buf+4, (char*)name[n]+1);
		name[n][k] = c;
		strcat((char*)buf, "...");
		while((l-i) >= NBUF/2-4){
			k += chartorune(&r, (char*)name[n]+k);
			i++;
		}
		strcat((char*)buf, (char*)name[n]+k);
	}else
		strcpy((char*)buf+4, (char*)name[n]+1);
	i = utflen((char*)buf);
	k = strlen((char*)buf);
	while(i<mw && k<sizeof buf-1){
		buf[k++] = ' ';
		i++;
	}
	buf[k] = 0;
	return (char *)buf;
}
