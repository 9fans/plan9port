#include "sam.h"
Header	h;
uchar	indata[DATASIZE];
uchar	outdata[2*DATASIZE+3];	/* room for overflow message */
uchar	*inp;
uchar	*outp;
uchar	*outmsg = outdata;
Posn	cmdpt;
Posn	cmdptadv;
Buffer	snarfbuf;
int	waitack;
int	outbuffered;
int	tversion;

int	inshort(void);
long	inlong(void);
vlong	invlong(void);
int	inmesg(Tmesg);

void	outshort(int);
void	outlong(long);
void	outvlong(vlong);
void	outcopy(int, void*);
void	outsend(void);
void	outstart(Hmesg);

void	setgenstr(File*, Posn, Posn);

#ifdef DEBUG
char *hname[] = {
	[Hversion]	"Hversion",
	[Hbindname]	"Hbindname",
	[Hcurrent]	"Hcurrent",
	[Hnewname]	"Hnewname",
	[Hmovname]	"Hmovname",
	[Hgrow]		"Hgrow",
	[Hcheck0]	"Hcheck0",
	[Hcheck]	"Hcheck",
	[Hunlock]	"Hunlock",
	[Hdata]		"Hdata",
	[Horigin]	"Horigin",
	[Hunlockfile]	"Hunlockfile",
	[Hsetdot]	"Hsetdot",
	[Hgrowdata]	"Hgrowdata",
	[Hmoveto]	"Hmoveto",
	[Hclean]	"Hclean",
	[Hdirty]	"Hdirty",
	[Hcut]		"Hcut",
	[Hsetpat]	"Hsetpat",
	[Hdelname]	"Hdelname",
	[Hclose]	"Hclose",
	[Hsetsnarf]	"Hsetsnarf",
	[Hsnarflen]	"Hsnarflen",
	[Hack]		"Hack",
	[Hexit]		"Hexit",
	[Hplumb]		"Hplumb"
};

char *tname[] = {
	[Tversion]	"Tversion",
	[Tstartcmdfile]	"Tstartcmdfile",
	[Tcheck]	"Tcheck",
	[Trequest]	"Trequest",
	[Torigin]	"Torigin",
	[Tstartfile]	"Tstartfile",
	[Tworkfile]	"Tworkfile",
	[Ttype]		"Ttype",
	[Tcut]		"Tcut",
	[Tpaste]	"Tpaste",
	[Tsnarf]	"Tsnarf",
	[Tstartnewfile]	"Tstartnewfile",
	[Twrite]	"Twrite",
	[Tclose]	"Tclose",
	[Tlook]		"Tlook",
	[Tsearch]	"Tsearch",
	[Tsend]		"Tsend",
	[Tdclick]	"Tdclick",
	[Tstartsnarf]	"Tstartsnarf",
	[Tsetsnarf]	"Tsetsnarf",
	[Tack]		"Tack",
	[Texit]		"Texit",
	[Tplumb]		"Tplumb"
};

void
journal(int out, char *s)
{
	static int fd = 0;

	if(fd <= 0)
		fd = create("/tmp/sam.out", 1, 0666L);
	fprint(fd, "%s%s\n", out? "out: " : "in:  ", s);
}

void
journaln(int out, long n)
{
	char buf[32];

	snprint(buf, sizeof buf, "%ld", n);
	journal(out, buf);
}

void
journalv(int out, vlong v)
{
	char buf[32];

	snprint(buf, sizeof buf, "%lld", v);
	journal(out, buf);
}

#else
#define	journal(a, b)
#define journaln(a, b)
#endif

int
rcvchar(void){
	static uchar buf[64];
	static int i, nleft = 0;

	if(nleft <= 0){
		nleft = read(0, (char *)buf, sizeof buf);
		if(nleft <= 0)
			return -1;
		i = 0;
	}
	--nleft;
	return buf[i++];
}

int
rcv(void){
	int c;
	static int state = 0;
	static int count = 0;
	static int i = 0;

	while((c=rcvchar()) != -1)
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
			if(count > DATASIZE)
				panic("count>DATASIZE");
			if(count == 0)
				goto zerocount;
			state++;
			break;

		case 3:
			indata[i++] = c;
			if(i == count){
		zerocount:
				indata[i] = 0;
				state = count = 0;
				return inmesg(h.type);
			}
			break;
		}
	return 0;
}

File *
whichfile(int tag)
{
	int i;

	for(i = 0; i<file.nused; i++)
		if(file.filepptr[i]->tag==tag)
			return file.filepptr[i];
	hiccough((char *)0);
	return 0;
}

int
inmesg(Tmesg type)
{
	Rune buf[1025];
	char cbuf[64];
	int i, m;
	short s;
	long l, l1;
	vlong v;
	File *f;
	Posn p0, p1, p;
	Range r;
	String *str;
	char *c, *wdir;
	Rune *rp;
	Plumbmsg *pm;

	if(type > TMAX)
		panic("inmesg");

	journal(0, tname[type]);

	inp = indata;
	switch(type){
	case -1:
		panic("rcv error");

	default:
		fprint(2, "unknown type %d\n", type);
		panic("rcv unknown");

	case Tversion:
		tversion = inshort();
		journaln(0, tversion);
		break;

	case Tstartcmdfile:
		v = invlong();		/* for 64-bit pointers */
		journaln(0, v);
		Strdupl(&genstr, samname);
		cmd = newfile();
		cmd->unread = 0;
		outTsv(Hbindname, cmd->tag, v);
		outTs(Hcurrent, cmd->tag);
		logsetname(cmd, &genstr);
		cmd->rasp = listalloc('P');
		cmd->mod = 0;
		if(cmdstr.n){
			loginsert(cmd, 0L, cmdstr.s, cmdstr.n);
			Strdelete(&cmdstr, 0L, (Posn)cmdstr.n);
		}
		fileupdate(cmd, FALSE, TRUE);
		outT0(Hunlock);
		break;

	case Tcheck:
		/* go through whichfile to check the tag */
		outTs(Hcheck, whichfile(inshort())->tag);
		break;

	case Trequest:
		f = whichfile(inshort());
		p0 = inlong();
		p1 = p0+inshort();
		journaln(0, p0);
		journaln(0, p1-p0);
		if(f->unread)
			panic("Trequest: unread");
		if(p1>f->b.nc)
			p1 = f->b.nc;
		if(p0>f->b.nc) /* can happen e.g. scrolling during command */
			p0 = f->b.nc;
		if(p0 == p1){
			i = 0;
			r.p1 = r.p2 = p0;
		}else{
			r = rdata(f->rasp, p0, p1-p0);
			i = r.p2-r.p1;
			bufread(&f->b, r.p1, buf, i);
		}
		buf[i]=0;
		outTslS(Hdata, f->tag, r.p1, tmprstr(buf, i+1));
		break;

	case Torigin:
		s = inshort();
		l = inlong();
		l1 = inlong();
		journaln(0, l1);
		lookorigin(whichfile(s), l, l1);
		break;

	case Tstartfile:
		termlocked++;
		f = whichfile(inshort());
		if(!f->rasp)	/* this might be a duplicate message */
			f->rasp = listalloc('P');
		current(f);
		outTsv(Hbindname, f->tag, invlong());	/* for 64-bit pointers */
		outTs(Hcurrent, f->tag);
		journaln(0, f->tag);
		if(f->unread)
			load(f);
		else{
			if(f->b.nc>0){
				rgrow(f->rasp, 0L, f->b.nc);
				outTsll(Hgrow, f->tag, 0L, f->b.nc);
			}
			outTs(Hcheck0, f->tag);
			moveto(f, f->dot.r);
		}
		break;

	case Tworkfile:
		i = inshort();
		f = whichfile(i);
		current(f);
		f->dot.r.p1 = inlong();
		f->dot.r.p2 = inlong();
		f->tdot = f->dot.r;
		journaln(0, i);
		journaln(0, f->dot.r.p1);
		journaln(0, f->dot.r.p2);
		break;

	case Ttype:
		f = whichfile(inshort());
		p0 = inlong();
		journaln(0, p0);
		journal(0, (char*)inp);
		str = tmpcstr((char*)inp);
		i = str->n;
		loginsert(f, p0, str->s, str->n);
		if(fileupdate(f, FALSE, FALSE))
			seq++;
		if(f==cmd && p0==f->b.nc-i && i>0 && str->s[i-1]=='\n'){
			freetmpstr(str);
			termlocked++;
			termcommand();
		}else
			freetmpstr(str);
		f->dot.r.p1 = f->dot.r.p2 = p0+i; /* terminal knows this already */
		f->tdot = f->dot.r;
		break;

	case Tcut:
		f = whichfile(inshort());
		p0 = inlong();
		p1 = inlong();
		journaln(0, p0);
		journaln(0, p1);
		logdelete(f, p0, p1);
		if(fileupdate(f, FALSE, FALSE))
			seq++;
		f->dot.r.p1 = f->dot.r.p2 = p0;
		f->tdot = f->dot.r;   /* terminal knows the value of dot already */
		break;

	case Tpaste:
		f = whichfile(inshort());
		p0 = inlong();
		journaln(0, p0);
		for(l=0; l<snarfbuf.nc; l+=m){
			m = snarfbuf.nc-l;
			if(m>BLOCKSIZE)
				m = BLOCKSIZE;
			bufread(&snarfbuf, l, genbuf, m);
			loginsert(f, p0, tmprstr(genbuf, m)->s, m);
		}
		if(fileupdate(f, FALSE, TRUE))
			seq++;
		f->dot.r.p1 = p0;
		f->dot.r.p2 = p0+snarfbuf.nc;
		f->tdot.p1 = -1; /* force telldot to tell (arguably a BUG) */
		telldot(f);
		outTs(Hunlockfile, f->tag);
		break;

	case Tsnarf:
		i = inshort();
		p0 = inlong();
		p1 = inlong();
		snarf(whichfile(i), p0, p1, &snarfbuf, 0);
		break;

	case Tstartnewfile:
		v = invlong();
		Strdupl(&genstr, empty);
		f = newfile();
		f->rasp = listalloc('P');
		outTsv(Hbindname, f->tag, v);
		logsetname(f, &genstr);
		outTs(Hcurrent, f->tag);
		current(f);
		load(f);
		break;

	case Twrite:
		termlocked++;
		i = inshort();
		journaln(0, i);
		f = whichfile(i);
		addr.r.p1 = 0;
		addr.r.p2 = f->b.nc;
		if(f->name.s[0] == 0)
			error(Enoname);
		Strduplstr(&genstr, &f->name);
		writef(f);
		break;

	case Tclose:
		termlocked++;
		i = inshort();
		journaln(0, i);
		f = whichfile(i);
		current(f);
		trytoclose(f);
		/* if trytoclose fails, will error out */
		delete(f);
		break;

	case Tlook:
		f = whichfile(inshort());
		termlocked++;
		p0 = inlong();
		p1 = inlong();
		journaln(0, p0);
		journaln(0, p1);
		setgenstr(f, p0, p1);
		for(l = 0; l<genstr.n; l++){
			i = genstr.s[l];
			if(utfrune(".*+?(|)\\[]^$", i)){
				str = tmpcstr("\\");
				Strinsert(&genstr, str, l++);
				freetmpstr(str);
			}
		}
		Straddc(&genstr, '\0');
		nextmatch(f, &genstr, p1, 1);
		moveto(f, sel.p[0]);
		break;

	case Tsearch:
		termlocked++;
		if(curfile == 0)
			error(Enofile);
		if(lastpat.s[0] == 0)
			panic("Tsearch");
		nextmatch(curfile, &lastpat, curfile->dot.r.p2, 1);
		moveto(curfile, sel.p[0]);
		break;

	case Tsend:
		termlocked++;
		inshort();	/* ignored */
		p0 = inlong();
		p1 = inlong();
		setgenstr(cmd, p0, p1);
		bufreset(&snarfbuf);
		bufinsert(&snarfbuf, (Posn)0, genstr.s, genstr.n);
		outTl(Hsnarflen, genstr.n);
		if(genstr.s[genstr.n-1] != '\n')
			Straddc(&genstr, '\n');
		loginsert(cmd, cmd->b.nc, genstr.s, genstr.n);
		fileupdate(cmd, FALSE, TRUE);
		cmd->dot.r.p1 = cmd->dot.r.p2 = cmd->b.nc;
		telldot(cmd);
		termcommand();
		break;

	case Tdclick:
		f = whichfile(inshort());
		p1 = inlong();
		doubleclick(f, p1);
		f->tdot.p1 = f->tdot.p2 = p1;
		telldot(f);
		outTs(Hunlockfile, f->tag);
		break;

	case Tstartsnarf:
		if (snarfbuf.nc <= 0) {	/* nothing to export */
			outTs(Hsetsnarf, 0);
			break;
		}
		c = 0;
		i = 0;
		m = snarfbuf.nc;
		if(m > SNARFSIZE) {
			m = SNARFSIZE;
			dprint("?warning: snarf buffer truncated\n");
		}
		rp = malloc(m*sizeof(Rune));
		if(rp){
			bufread(&snarfbuf, 0, rp, m);
			c = Strtoc(tmprstr(rp, m));
			free(rp);
			i = strlen(c);
		}
		outTs(Hsetsnarf, i);
		if(c){
			Write(1, c, i);
			free(c);
		} else
			dprint("snarf buffer too long\n");
		break;

	case Tsetsnarf:
		m = inshort();
		if(m > SNARFSIZE)
			error(Etoolong);
		c = malloc(m+1);
		if(c){
			for(i=0; i<m; i++)
				c[i] = rcvchar();
			c[m] = 0;
			str = tmpcstr(c);
			free(c);
			bufreset(&snarfbuf);
			bufinsert(&snarfbuf, (Posn)0, str->s, str->n);
			freetmpstr(str);
			outT0(Hunlock);
		}
		break;

	case Tack:
		waitack = 0;
		break;

	case Tplumb:
		f = whichfile(inshort());
		p0 = inlong();
		p1 = inlong();
		pm = emalloc(sizeof(Plumbmsg));
		pm->src = strdup("sam");
		pm->dst = 0;
		/* construct current directory */
		c = Strtoc(&f->name);
		if(c[0] == '/')
			pm->wdir = c;
		else{
			wdir = emalloc(1024);
			getwd(wdir, 1024);
			pm->wdir = emalloc(1024);
			snprint(pm->wdir, 1024, "%s/%s", wdir, c);
			cleanname(pm->wdir);
			free(wdir);
			free(c);
		}
		c = strrchr(pm->wdir, '/');
		if(c)
			*c = '\0';
		pm->type = strdup("text");
		if(p1 > p0)
			pm->attr = nil;
		else{
			p = p0;
			while(p0>0 && (i=filereadc(f, p0 - 1))!=' ' && i!='\t' && i!='\n')
				p0--;
			while(p1<f->b.nc && (i=filereadc(f, p1))!=' ' && i!='\t' && i!='\n')
				p1++;
			sprint(cbuf, "click=%ld", p-p0);
			pm->attr = plumbunpackattr(cbuf);
		}
		if(p0==p1 || p1-p0>=BLOCKSIZE){
			plumbfree(pm);
			break;
		}
		setgenstr(f, p0, p1);
		pm->data = Strtoc(&genstr);
		pm->ndata = strlen(pm->data);
		c = plumbpack(pm, &i);
		if(c != 0){
			outTs(Hplumb, i);
			Write(1, c, i);
			free(c);
		}
		plumbfree(pm);
		break;

	case Texit:
		exits(0);
	}
	return TRUE;
}

void
snarf(File *f, Posn p1, Posn p2, Buffer *buf, int emptyok)
{
	Posn l;
	int i;

	if(!emptyok && p1==p2)
		return;
	bufreset(buf);
	/* Stage through genbuf to avoid compaction problems (vestigial) */
	if(p2 > f->b.nc){
		fprint(2, "bad snarf addr p1=%ld p2=%ld f->b.nc=%d\n", p1, p2, f->b.nc); /*ZZZ should never happen, can remove */
		p2 = f->b.nc;
	}
	for(l=p1; l<p2; l+=i){
		i = p2-l>BLOCKSIZE? BLOCKSIZE : p2-l;
		bufread(&f->b, l, genbuf, i);
		bufinsert(buf, buf->nc, tmprstr(genbuf, i)->s, i);
	}
}

int
inshort(void)
{
	ushort n;

	n = inp[0] | (inp[1]<<8);
	inp += 2;
	return n;
}

long
inlong(void)
{
	ulong n;

	n = inp[0] | (inp[1]<<8) | (inp[2]<<16) | (inp[3]<<24);
	inp += 4;
	return n;
}

vlong
invlong(void)
{
	vlong v;

	v = (inp[7]<<24) | (inp[6]<<16) | (inp[5]<<8) | inp[4];
	v = (v<<16) | (inp[3]<<8) | inp[2];
	v = (v<<16) | (inp[1]<<8) | inp[0];
	inp += 8;
	return v;
}

void
setgenstr(File *f, Posn p0, Posn p1)
{
	if(p0 != p1){
		if(p1-p0 >= TBLOCKSIZE)
			error(Etoolong);
		Strinsure(&genstr, p1-p0);
		bufread(&f->b, p0, genbuf, p1-p0);
		memmove(genstr.s, genbuf, RUNESIZE*(p1-p0));
		genstr.n = p1-p0;
	}else{
		if(snarfbuf.nc == 0)
			error(Eempty);
		if(snarfbuf.nc > TBLOCKSIZE)
			error(Etoolong);
		bufread(&snarfbuf, (Posn)0, genbuf, snarfbuf.nc);
		Strinsure(&genstr, snarfbuf.nc);
		memmove(genstr.s, genbuf, RUNESIZE*snarfbuf.nc);
		genstr.n = snarfbuf.nc;
	}
}

void
outT0(Hmesg type)
{
	outstart(type);
	outsend();
}

void
outTl(Hmesg type, long l)
{
	outstart(type);
	outlong(l);
	outsend();
}

void
outTs(Hmesg type, int s)
{
	outstart(type);
	journaln(1, s);
	outshort(s);
	outsend();
}

void
outS(String *s)
{
	char *c;
	int i;

	c = Strtoc(s);
	i = strlen(c);
	outcopy(i, c);
	if(i > 99)
		c[99] = 0;
	journaln(1, i);
	journal(1, c);
	free(c);
}

void
outTsS(Hmesg type, int s1, String *s)
{
	outstart(type);
	outshort(s1);
	outS(s);
	outsend();
}

void
outTslS(Hmesg type, int s1, Posn l1, String *s)
{
	outstart(type);
	outshort(s1);
	journaln(1, s1);
	outlong(l1);
	journaln(1, l1);
	outS(s);
	outsend();
}

void
outTS(Hmesg type, String *s)
{
	outstart(type);
	outS(s);
	outsend();
}

void
outTsllS(Hmesg type, int s1, Posn l1, Posn l2, String *s)
{
	outstart(type);
	outshort(s1);
	outlong(l1);
	outlong(l2);
	journaln(1, l1);
	journaln(1, l2);
	outS(s);
	outsend();
}

void
outTsll(Hmesg type, int s, Posn l1, Posn l2)
{
	outstart(type);
	outshort(s);
	outlong(l1);
	outlong(l2);
	journaln(1, l1);
	journaln(1, l2);
	outsend();
}

void
outTsl(Hmesg type, int s, Posn l)
{
	outstart(type);
	outshort(s);
	outlong(l);
	journaln(1, l);
	outsend();
}

void
outTsv(Hmesg type, int s, vlong v)
{
	outstart(type);
	outshort(s);
	outvlong(v);
	journaln(1, v);
	outsend();
}

void
outstart(Hmesg type)
{
	journal(1, hname[type]);
	outmsg[0] = type;
	outp = outmsg+3;
}

void
outcopy(int count, void *data)
{
	memmove(outp, data, count);
	outp += count;
}

void
outshort(int s)
{
	*outp++ = s;
	*outp++ = s>>8;
}

void
outlong(long l)
{
	*outp++ = l;
	*outp++ = l>>8;
	*outp++ = l>>16;
	*outp++ = l>>24;
}

void
outvlong(vlong v)
{
	int i;

	for(i = 0; i < 8; i++){
		*outp++ = v;
		v >>= 8;
	}
}

void
outsend(void)
{
	int outcount;

	if(outp >= outdata+nelem(outdata))
		panic("outsend");
	outcount = outp-outmsg;
	outcount -= 3;
	outmsg[1] = outcount;
	outmsg[2] = outcount>>8;
	outmsg = outp;
	if(!outbuffered){
		outcount = outmsg-outdata;
		if (write(1, (char*) outdata, outcount) != outcount)
			rescue();
		outmsg = outdata;
		return;
	}
}

int
needoutflush(void)
{
	return outmsg >= outdata+DATASIZE;
}

void
outflush(void)
{
	if(outmsg == outdata)
		return;
	outbuffered = 0;
	/* flow control */
	outT0(Hack);
	waitack = 1;
	do
		if(rcv() == 0){
			rescue();
			exits("eof");
		}
	while(waitack);
	outmsg = outdata;
	outbuffered = 1;
}
