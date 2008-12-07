#include <u.h>
#include <libc.h>
#include <bio.h>

typedef	void*	pointer;

#define div	dcdiv

#define FATAL 0
#define NFATAL 1
#define BLK sizeof(Blk)
#define PTRSZ sizeof(int*)
#define HEADSZ 1024
#define STKSZ 100
#define RDSKSZ 100
#define TBLSZ 256
#define ARRAYST 221
#define MAXIND 2048
#define NL 1
#define NG 2
#define NE 3
#define length(p)	((p)->wt-(p)->beg)
#define rewind(p)	(p)->rd=(p)->beg
#undef create
#define create(p)	(p)->rd = (p)->wt = (p)->beg
#define fsfile(p)	(p)->rd = (p)->wt
#define truncate(p)	(p)->wt = (p)->rd
#define sfeof(p)	(((p)->rd==(p)->wt)?1:0)
#define sfbeg(p)	(((p)->rd==(p)->beg)?1:0)
#define sungetc(p,c)	*(--(p)->rd)=c
#define sgetc(p)	(((p)->rd==(p)->wt)?-1:*(p)->rd++)
#define skipc(p)	{if((p)->rd<(p)->wt)(p)->rd++;}
#define slookc(p)	(((p)->rd==(p)->wt)?-1:*(p)->rd)
#define sbackc(p)	(((p)->rd==(p)->beg)?-1:*(--(p)->rd))
#define backc(p)	{if((p)->rd>(p)->beg) --(p)->rd;}
#define sputc(p,c)	{if((p)->wt==(p)->last)more(p);\
				*(p)->wt++ = c; }
#define salterc(p,c)	{if((p)->rd==(p)->last)more(p);\
				*(p)->rd++ = c;\
				if((p)->rd>(p)->wt)(p)->wt=(p)->rd;}
#define sunputc(p)	(*((p)->rd = --(p)->wt))
#define sclobber(p)	((p)->rd = --(p)->wt)
#define zero(p)		for(pp=(p)->beg;pp<(p)->last;)\
				*pp++='\0'
#define OUTC(x)		{Bputc(&bout,x); if(--count == 0){Bprint(&bout,"\\\n"); count=ll;} }
#define TEST2		{if((count -= 2) <=0){Bprint(&bout,"\\\n");count=ll;}}
#define EMPTY		if(stkerr != 0){Bprint(&bout,"stack empty\n"); continue; }
#define EMPTYR(x)	if(stkerr!=0){pushp(x);Bprint(&bout,"stack empty\n");continue;}
#define EMPTYS		if(stkerr != 0){Bprint(&bout,"stack empty\n"); return(1);}
#define EMPTYSR(x)	if(stkerr !=0){Bprint(&bout,"stack empty\n");pushp(x);return(1);}
#define error(p)	{Bprint(&bout,p); continue; }
#define errorrt(p)	{Bprint(&bout,p); return(1); }
#define LASTFUN 026

typedef	struct	Blk	Blk;
struct	Blk
{
	char	*rd;
	char	*wt;
	char	*beg;
	char	*last;
};
typedef	struct	Sym	Sym;
struct	Sym
{
	Sym	*next;
	Blk	*val;
};
typedef	struct	Wblk	Wblk;
struct	Wblk
{
	Blk	**rdw;
	Blk	**wtw;
	Blk	**begw;
	Blk	**lastw;
};

Biobuf	*curfile, *fsave;
Blk	*arg1, *arg2;
uchar	savk;
int	dbg;
int	ifile;
Blk	*scalptr, *basptr, *tenptr, *inbas;
Blk	*sqtemp, *chptr, *strptr, *divxyz;
Blk	*stack[STKSZ];
Blk	**stkptr,**stkbeg;
Blk	**stkend;
Blk	*hfree;
int	stkerr;
int	lastchar;
Blk	*readstk[RDSKSZ];
Blk	**readptr;
Blk	*rem;
int	k;
Blk	*irem;
int	skd,skr;
int	neg;
Sym	symlst[TBLSZ];
Sym	*stable[TBLSZ];
Sym	*sptr, *sfree;
long	rel;
long	nbytes;
long	all;
long	headmor;
long	obase;
int	fw,fw1,ll;
void	(*outdit)(Blk *p, int flg);
int	logo;
int	logten;
int	count;
char	*pp;
char	*dummy;
long	longest, maxsize, active;
int	lall, lrel, lcopy, lmore, lbytes;
int	inside;
Biobuf	bin;
Biobuf	bout;

void	main(int argc, char *argv[]);
void	commnds(void);
Blk*	readin(void);
Blk*	div(Blk *ddivd, Blk *ddivr);
int	dscale(void);
Blk*	removr(Blk *p, int n);
Blk*	dcsqrt(Blk *p);
void	init(int argc, char *argv[]);
void	onintr(void);
void	pushp(Blk *p);
Blk*	pop(void);
Blk*	readin(void);
Blk*	add0(Blk *p, int ct);
Blk*	mult(Blk *p, Blk *q);
void	chsign(Blk *p);
int	readc(void);
void	unreadc(char c);
void	binop(char c);
void	dcprint(Blk *hptr);
Blk*	dcexp(Blk *base, Blk *ex);
Blk*	getdec(Blk *p, int sc);
void	tenot(Blk *p, int sc);
void	oneot(Blk *p, int sc, char ch);
void	hexot(Blk *p, int flg);
void	bigot(Blk *p, int flg);
Blk*	add(Blk *a1, Blk *a2);
int	eqk(void);
Blk*	removc(Blk *p, int n);
Blk*	scalint(Blk *p);
Blk*	scale(Blk *p, int n);
int	subt(void);
int	command(void);
int	cond(char c);
void	load(void);
#define log2 dclog2
int	log2(long n);
Blk*	salloc(int size);
Blk*	morehd(void);
Blk*	copy(Blk *hptr, int size);
void	sdump(char *s1, Blk *hptr);
void	seekc(Blk *hptr, int n);
void	salterwd(Blk *hptr, Blk *n);
void	more(Blk *hptr);
void	ospace(char *s);
void	garbage(char *s);
void	release(Blk *p);
Blk*	dcgetwd(Blk *p);
void	putwd(Blk *p, Blk *c);
Blk*	lookwd(Blk *p);
int	getstk(void);

/********debug only**/
void
tpr(char *cp, Blk *bp)
{
	print("%s-> ", cp);
	print("beg: %lx rd: %lx wt: %lx last: %lx\n", bp->beg, bp->rd,
		bp->wt, bp->last);
	for (cp = bp->beg; cp != bp->wt; cp++) {
		print("%d", *cp);
		if (cp != bp->wt-1)
			print("/");
	}
	print("\n");
}
/************/

void
main(int argc, char *argv[])
{
	Binit(&bin, 0, OREAD);
	Binit(&bout, 1, OWRITE);
	init(argc,argv);
	commnds();
	exits(0);
}

void
commnds(void)
{
	Blk *p, *q, **ptr, *s, *t;
	long l;
	Sym *sp;
	int sk, sk1, sk2, c, sign, n, d;

	while(1) {
		Bflush(&bout);
		if(((c = readc())>='0' && c <= '9') ||
		    (c>='A' && c <='F') || c == '.') {
			unreadc(c);
			p = readin();
			pushp(p);
			continue;
		}
		switch(c) {
		case ' ':
		case '\n':
		case -1:
			continue;
		case 'Y':
			sdump("stk",*stkptr);
			Bprint(&bout, "all %ld rel %ld headmor %ld\n",all,rel,headmor);
			Bprint(&bout, "nbytes %ld\n",nbytes);
			Bprint(&bout, "longest %ld active %ld maxsize %ld\n", longest,
				active, maxsize);
			Bprint(&bout, "new all %d rel %d copy %d more %d lbytes %d\n",
				lall, lrel, lcopy, lmore, lbytes);
			lall = lrel = lcopy = lmore = lbytes = 0;
			continue;
		case '_':
			p = readin();
			savk = sunputc(p);
			chsign(p);
			sputc(p,savk);
			pushp(p);
			continue;
		case '-':
			subt();
			continue;
		case '+':
			if(eqk() != 0)
				continue;
			binop('+');
			continue;
		case '*':
			arg1 = pop();
			EMPTY;
			arg2 = pop();
			EMPTYR(arg1);
			sk1 = sunputc(arg1);
			sk2 = sunputc(arg2);
			savk = sk1+sk2;
			binop('*');
			p = pop();
			if(savk>k && savk>sk1 && savk>sk2) {
				sclobber(p);
				sk = sk1;
				if(sk<sk2)
					sk = sk2;
				if(sk<k)
					sk = k;
				p = removc(p,savk-sk);
				savk = sk;
				sputc(p,savk);
			}
			pushp(p);
			continue;
		case '/':
		casediv:
			if(dscale() != 0)
				continue;
			binop('/');
			if(irem != 0)
				release(irem);
			release(rem);
			continue;
		case '%':
			if(dscale() != 0)
				continue;
			binop('/');
			p = pop();
			release(p);
			if(irem == 0) {
				sputc(rem,skr+k);
				pushp(rem);
				continue;
			}
			p = add0(rem,skd-(skr+k));
			q = add(p,irem);
			release(p);
			release(irem);
			sputc(q,skd);
			pushp(q);
			continue;
		case 'v':
			p = pop();
			EMPTY;
			savk = sunputc(p);
			if(length(p) == 0) {
				sputc(p,savk);
				pushp(p);
				continue;
			}
			if(sbackc(p)<0) {
				error("sqrt of neg number\n");
			}
			if(k<savk)
				n = savk;
			else {
				n = k*2-savk;
				savk = k;
			}
			arg1 = add0(p,n);
			arg2 = dcsqrt(arg1);
			sputc(arg2,savk);
			pushp(arg2);
			continue;

		case '^':
			neg = 0;
			arg1 = pop();
			EMPTY;
			if(sunputc(arg1) != 0)
				error("exp not an integer\n");
			arg2 = pop();
			EMPTYR(arg1);
			if(sfbeg(arg1) == 0 && sbackc(arg1)<0) {
				neg++;
				chsign(arg1);
			}
			if(length(arg1)>=3) {
				error("exp too big\n");
			}
			savk = sunputc(arg2);
			p = dcexp(arg2,arg1);
			release(arg2);
			rewind(arg1);
			c = sgetc(arg1);
			if(c == -1)
				c = 0;
			else
			if(sfeof(arg1) == 0)
				c = sgetc(arg1)*100 + c;
			d = c*savk;
			release(arg1);
		/*	if(neg == 0) {		removed to fix -exp bug*/
				if(k>=savk)
					n = k;
				else
					n = savk;
				if(n<d) {
					q = removc(p,d-n);
					sputc(q,n);
					pushp(q);
				} else {
					sputc(p,d);
					pushp(p);
				}
		/*	} else { this is disaster for exp <-127 */
		/*		sputc(p,d);		*/
		/*		pushp(p);		*/
		/*	}				*/
			if(neg == 0)
				continue;
			p = pop();
			q = salloc(2);
			sputc(q,1);
			sputc(q,0);
			pushp(q);
			pushp(p);
			goto casediv;
		case 'z':
			p = salloc(2);
			n = stkptr - stkbeg;
			if(n >= 100) {
				sputc(p,n/100);
				n %= 100;
			}
			sputc(p,n);
			sputc(p,0);
			pushp(p);
			continue;
		case 'Z':
			p = pop();
			EMPTY;
			n = (length(p)-1)<<1;
			fsfile(p);
			backc(p);
			if(sfbeg(p) == 0) {
				if((c = sbackc(p))<0) {
					n -= 2;
					if(sfbeg(p) == 1)
						n++;
					else {
						if((c = sbackc(p)) == 0)
							n++;
						else
						if(c > 90)
							n--;
					}
				} else
				if(c < 10)
					n--;
			}
			release(p);
			q = salloc(1);
			if(n >= 100) {
				sputc(q,n%100);
				n /= 100;
			}
			sputc(q,n);
			sputc(q,0);
			pushp(q);
			continue;
		case 'i':
			p = pop();
			EMPTY;
			p = scalint(p);
			release(inbas);
			inbas = p;
			continue;
		case 'I':
			p = copy(inbas,length(inbas)+1);
			sputc(p,0);
			pushp(p);
			continue;
		case 'o':
			p = pop();
			EMPTY;
			p = scalint(p);
			sign = 0;
			n = length(p);
			q = copy(p,n);
			fsfile(q);
			l = c = sbackc(q);
			if(n != 1) {
				if(c<0) {
					sign = 1;
					chsign(q);
					n = length(q);
					fsfile(q);
					l = c = sbackc(q);
				}
				if(n != 1) {
					while(sfbeg(q) == 0)
						l = l*100+sbackc(q);
				}
			}
			logo = log2(l);
			obase = l;
			release(basptr);
			if(sign == 1)
				obase = -l;
			basptr = p;
			outdit = bigot;
			if(n == 1 && sign == 0) {
				if(c <= 16) {
					outdit = hexot;
					fw = 1;
					fw1 = 0;
					ll = 70;
					release(q);
					continue;
				}
			}
			n = 0;
			if(sign == 1)
				n++;
			p = salloc(1);
			sputc(p,-1);
			t = add(p,q);
			n += length(t)*2;
			fsfile(t);
			if(sbackc(t)>9)
				n++;
			release(t);
			release(q);
			release(p);
			fw = n;
			fw1 = n-1;
			ll = 70;
			if(fw>=ll)
				continue;
			ll = (70/fw)*fw;
			continue;
		case 'O':
			p = copy(basptr,length(basptr)+1);
			sputc(p,0);
			pushp(p);
			continue;
		case '[':
			n = 0;
			p = salloc(0);
			for(;;) {
				if((c = readc()) == ']') {
					if(n == 0)
						break;
					n--;
				}
				sputc(p,c);
				if(c == '[')
					n++;
			}
			pushp(p);
			continue;
		case 'k':
			p = pop();
			EMPTY;
			p = scalint(p);
			if(length(p)>1) {
				error("scale too big\n");
			}
			rewind(p);
			k = 0;
			if(!sfeof(p))
				k = sgetc(p);
			release(scalptr);
			scalptr = p;
			continue;
		case 'K':
			p = copy(scalptr,length(scalptr)+1);
			sputc(p,0);
			pushp(p);
			continue;
		case 'X':
			p = pop();
			EMPTY;
			fsfile(p);
			n = sbackc(p);
			release(p);
			p = salloc(2);
			sputc(p,n);
			sputc(p,0);
			pushp(p);
			continue;
		case 'Q':
			p = pop();
			EMPTY;
			if(length(p)>2) {
				error("Q?\n");
			}
			rewind(p);
			if((c =  sgetc(p))<0) {
				error("neg Q\n");
			}
			release(p);
			while(c-- > 0) {
				if(readptr == &readstk[0]) {
					error("readstk?\n");
				}
				if(*readptr != 0)
					release(*readptr);
				readptr--;
			}
			continue;
		case 'q':
			if(readptr <= &readstk[1])
				exits(0);
			if(*readptr != 0)
				release(*readptr);
			readptr--;
			if(*readptr != 0)
				release(*readptr);
			readptr--;
			continue;
		case 'f':
			if(stkptr == &stack[0])
				Bprint(&bout,"empty stack\n");
			else {
				for(ptr = stkptr; ptr > &stack[0];) {
					dcprint(*ptr--);
				}
			}
			continue;
		case 'p':
			if(stkptr == &stack[0])
				Bprint(&bout,"empty stack\n");
			else {
				dcprint(*stkptr);
			}
			continue;
		case 'P':
			p = pop();
			EMPTY;
			sputc(p,0);
			Bprint(&bout,"%s",p->beg);
			release(p);
			continue;
		case 'd':
			if(stkptr == &stack[0]) {
				Bprint(&bout,"empty stack\n");
				continue;
			}
			q = *stkptr;
			n = length(q);
			p = copy(*stkptr,n);
			pushp(p);
			continue;
		case 'c':
			while(stkerr == 0) {
				p = pop();
				if(stkerr == 0)
					release(p);
			}
			continue;
		case 'S':
			if(stkptr == &stack[0]) {
				error("save: args\n");
			}
			c = getstk() & 0377;
			sptr = stable[c];
			sp = stable[c] = sfree;
			sfree = sfree->next;
			if(sfree == 0)
				goto sempty;
			sp->next = sptr;
			p = pop();
			EMPTY;
			if(c >= ARRAYST) {
				q = copy(p,length(p)+PTRSZ);
				for(n = 0;n < PTRSZ;n++) {
					sputc(q,0);
				}
				release(p);
				p = q;
			}
			sp->val = p;
			continue;
		sempty:
			error("symbol table overflow\n");
		case 's':
			if(stkptr == &stack[0]) {
				error("save:args\n");
			}
			c = getstk() & 0377;
			sptr = stable[c];
			if(sptr != 0) {
				p = sptr->val;
				if(c >= ARRAYST) {
					rewind(p);
					while(sfeof(p) == 0)
						release(dcgetwd(p));
				}
				release(p);
			} else {
				sptr = stable[c] = sfree;
				sfree = sfree->next;
				if(sfree == 0)
					goto sempty;
				sptr->next = 0;
			}
			p = pop();
			sptr->val = p;
			continue;
		case 'l':
			load();
			continue;
		case 'L':
			c = getstk() & 0377;
			sptr = stable[c];
			if(sptr == 0) {
				error("L?\n");
			}
			stable[c] = sptr->next;
			sptr->next = sfree;
			sfree = sptr;
			p = sptr->val;
			if(c >= ARRAYST) {
				rewind(p);
				while(sfeof(p) == 0) {
					q = dcgetwd(p);
					if(q != 0)
						release(q);
				}
			}
			pushp(p);
			continue;
		case ':':
			p = pop();
			EMPTY;
			q = scalint(p);
			fsfile(q);
			c = 0;
			if((sfbeg(q) == 0) && ((c = sbackc(q))<0)) {
				error("neg index\n");
			}
			if(length(q)>2) {
				error("index too big\n");
			}
			if(sfbeg(q) == 0)
				c = c*100+sbackc(q);
			if(c >= MAXIND) {
				error("index too big\n");
			}
			release(q);
			n = getstk() & 0377;
			sptr = stable[n];
			if(sptr == 0) {
				sptr = stable[n] = sfree;
				sfree = sfree->next;
				if(sfree == 0)
					goto sempty;
				sptr->next = 0;
				p = salloc((c+PTRSZ)*PTRSZ);
				zero(p);
			} else {
				p = sptr->val;
				if(length(p)-PTRSZ < c*PTRSZ) {
					q = copy(p,(c+PTRSZ)*PTRSZ);
					release(p);
					p = q;
				}
			}
			seekc(p,c*PTRSZ);
			q = lookwd(p);
			if(q!=0)
				release(q);
			s = pop();
			EMPTY;
			salterwd(p, s);
			sptr->val = p;
			continue;
		case ';':
			p = pop();
			EMPTY;
			q = scalint(p);
			fsfile(q);
			c = 0;
			if((sfbeg(q) == 0) && ((c = sbackc(q))<0)) {
				error("neg index\n");
			}
			if(length(q)>2) {
				error("index too big\n");
			}
			if(sfbeg(q) == 0)
				c = c*100+sbackc(q);
			if(c >= MAXIND) {
				error("index too big\n");
			}
			release(q);
			n = getstk() & 0377;
			sptr = stable[n];
			if(sptr != 0){
				p = sptr->val;
				if(length(p)-PTRSZ >= c*PTRSZ) {
					seekc(p,c*PTRSZ);
					s = dcgetwd(p);
					if(s != 0) {
						q = copy(s,length(s));
						pushp(q);
						continue;
					}
				}
			}
			q = salloc(1);	/*so uninitialized array elt prints as 0*/
			sputc(q, 0);
			pushp(q);
			continue;
		case 'x':
		execute:
			p = pop();
			EMPTY;
			if((readptr != &readstk[0]) && (*readptr != 0)) {
				if((*readptr)->rd == (*readptr)->wt)
					release(*readptr);
				else {
					if(readptr++ == &readstk[RDSKSZ]) {
						error("nesting depth\n");
					}
				}
			} else
				readptr++;
			*readptr = p;
			if(p != 0)
				rewind(p);
			else {
				if((c = readc()) != '\n')
					unreadc(c);
			}
			continue;
		case '?':
			if(++readptr == &readstk[RDSKSZ]) {
				error("nesting depth\n");
			}
			*readptr = 0;
			fsave = curfile;
			curfile = &bin;
			while((c = readc()) == '!')
				command();
			p = salloc(0);
			sputc(p,c);
			while((c = readc()) != '\n') {
				sputc(p,c);
				if(c == '\\')
					sputc(p,readc());
			}
			curfile = fsave;
			*readptr = p;
			continue;
		case '!':
			if(command() == 1)
				goto execute;
			continue;
		case '<':
		case '>':
		case '=':
			if(cond(c) == 1)
				goto execute;
			continue;
		default:
			Bprint(&bout,"%o is unimplemented\n",c);
		}
	}
}

Blk*
div(Blk *ddivd, Blk *ddivr)
{
	int divsign, remsign, offset, divcarry,
		carry, dig, magic, d, dd, under, first;
	long c, td, cc;
	Blk *ps, *px, *p, *divd, *divr;

	dig = 0;
	under = 0;
	divcarry = 0;
	rem = 0;
	p = salloc(0);
	if(length(ddivr) == 0) {
		pushp(ddivr);
		Bprint(&bout,"divide by 0\n");
		return(p);
	}
	divsign = remsign = first = 0;
	divr = ddivr;
	fsfile(divr);
	if(sbackc(divr) == -1) {
		divr = copy(ddivr,length(ddivr));
		chsign(divr);
		divsign = ~divsign;
	}
	divd = copy(ddivd,length(ddivd));
	fsfile(divd);
	if(sfbeg(divd) == 0 && sbackc(divd) == -1) {
		chsign(divd);
		divsign = ~divsign;
		remsign = ~remsign;
	}
	offset = length(divd) - length(divr);
	if(offset < 0)
		goto ddone;
	seekc(p,offset+1);
	sputc(divd,0);
	magic = 0;
	fsfile(divr);
	c = sbackc(divr);
	if(c < 10)
		magic++;
	c = c * 100 + (sfbeg(divr)?0:sbackc(divr));
	if(magic>0){
		c = (c * 100 +(sfbeg(divr)?0:sbackc(divr)))*2;
		c /= 25;
	}
	while(offset >= 0) {
		first++;
		fsfile(divd);
		td = sbackc(divd) * 100;
		dd = sfbeg(divd)?0:sbackc(divd);
		td = (td + dd) * 100;
		dd = sfbeg(divd)?0:sbackc(divd);
		td = td + dd;
		cc = c;
		if(offset == 0)
			td++;
		else
			cc++;
		if(magic != 0)
			td = td<<3;
		dig = td/cc;
		under=0;
		if(td%cc < 8  && dig > 0 && magic) {
			dig--;
			under=1;
		}
		rewind(divr);
		rewind(divxyz);
		carry = 0;
		while(sfeof(divr) == 0) {
			d = sgetc(divr)*dig+carry;
			carry = d / 100;
			salterc(divxyz,d%100);
		}
		salterc(divxyz,carry);
		rewind(divxyz);
		seekc(divd,offset);
		carry = 0;
		while(sfeof(divd) == 0) {
			d = slookc(divd);
			d = d-(sfeof(divxyz)?0:sgetc(divxyz))-carry;
			carry = 0;
			if(d < 0) {
				d += 100;
				carry = 1;
			}
			salterc(divd,d);
		}
		divcarry = carry;
		backc(p);
		salterc(p,dig);
		backc(p);
		fsfile(divd);
		d=sbackc(divd);
		if((d != 0) && /*!divcarry*/ (offset != 0)) {
			d = sbackc(divd) + 100;
			salterc(divd,d);
		}
		if(--offset >= 0)
			divd->wt--;
	}
	if(under) {	/* undershot last - adjust*/
		px = copy(divr,length(divr));	/*11/88 don't corrupt ddivr*/
		chsign(px);
		ps = add(px,divd);
		fsfile(ps);
		if(length(ps) > 0 && sbackc(ps) < 0) {
			release(ps);	/*only adjust in really undershot*/
		} else {
			release(divd);
			salterc(p, dig+1);
			divd=ps;
		}
	}
	if(divcarry != 0) {
		salterc(p,dig-1);
		salterc(divd,-1);
		ps = add(divr,divd);
		release(divd);
		divd = ps;
	}

	rewind(p);
	divcarry = 0;
	while(sfeof(p) == 0){
		d = slookc(p)+divcarry;
		divcarry = 0;
		if(d >= 100){
			d -= 100;
			divcarry = 1;
		}
		salterc(p,d);
	}
	if(divcarry != 0)salterc(p,divcarry);
	fsfile(p);
	while(sfbeg(p) == 0) {
		if(sbackc(p) != 0)
			break;
		truncate(p);
	}
	if(divsign < 0)
		chsign(p);
	fsfile(divd);
	while(sfbeg(divd) == 0) {
		if(sbackc(divd) != 0)
			break;
		truncate(divd);
	}
ddone:
	if(remsign<0)
		chsign(divd);
	if(divr != ddivr)
		release(divr);
	rem = divd;
	return(p);
}

int
dscale(void)
{
	Blk *dd, *dr, *r;
	int c;

	dr = pop();
	EMPTYS;
	dd = pop();
	EMPTYSR(dr);
	fsfile(dd);
	skd = sunputc(dd);
	fsfile(dr);
	skr = sunputc(dr);
	if(sfbeg(dr) == 1 || (sfbeg(dr) == 0 && sbackc(dr) == 0)) {
		sputc(dr,skr);
		pushp(dr);
		Bprint(&bout,"divide by 0\n");
		return(1);
	}
	if(sfbeg(dd) == 1 || (sfbeg(dd) == 0 && sbackc(dd) == 0)) {
		sputc(dd,skd);
		pushp(dd);
		return(1);
	}
	c = k-skd+skr;
	if(c < 0)
		r = removr(dd,-c);
	else {
		r = add0(dd,c);
		irem = 0;
	}
	arg1 = r;
	arg2 = dr;
	savk = k;
	return(0);
}

Blk*
removr(Blk *p, int n)
{
	int nn, neg;
	Blk *q, *s, *r;

	fsfile(p);
	neg = sbackc(p);
	if(neg < 0)
		chsign(p);
	rewind(p);
	nn = (n+1)/2;
	q = salloc(nn);
	while(n>1) {
		sputc(q,sgetc(p));
		n -= 2;
	}
	r = salloc(2);
	while(sfeof(p) == 0)
		sputc(r,sgetc(p));
	release(p);
	if(n == 1){
		s = div(r,tenptr);
		release(r);
		rewind(rem);
		if(sfeof(rem) == 0)
			sputc(q,sgetc(rem));
		release(rem);
		if(neg < 0){
			chsign(s);
			chsign(q);
			irem = q;
			return(s);
		}
		irem = q;
		return(s);
	}
	if(neg < 0) {
		chsign(r);
		chsign(q);
		irem = q;
		return(r);
	}
	irem = q;
	return(r);
}

Blk*
dcsqrt(Blk *p)
{
	Blk *t, *r, *q, *s;
	int c, n, nn;

	n = length(p);
	fsfile(p);
	c = sbackc(p);
	if((n&1) != 1)
		c = c*100+(sfbeg(p)?0:sbackc(p));
	n = (n+1)>>1;
	r = salloc(n);
	zero(r);
	seekc(r,n);
	nn=1;
	while((c -= nn)>=0)
		nn+=2;
	c=(nn+1)>>1;
	fsfile(r);
	backc(r);
	if(c>=100) {
		c -= 100;
		salterc(r,c);
		sputc(r,1);
	} else
		salterc(r,c);
	for(;;){
		q = div(p,r);
		s = add(q,r);
		release(q);
		release(rem);
		q = div(s,sqtemp);
		release(s);
		release(rem);
		s = copy(r,length(r));
		chsign(s);
		t = add(s,q);
		release(s);
		fsfile(t);
		nn = sfbeg(t)?0:sbackc(t);
		if(nn>=0)
			break;
		release(r);
		release(t);
		r = q;
	}
	release(t);
	release(q);
	release(p);
	return(r);
}

Blk*
dcexp(Blk *base, Blk *ex)
{
	Blk *r, *e, *p, *e1, *t, *cp;
	int temp, c, n;

	r = salloc(1);
	sputc(r,1);
	p = copy(base,length(base));
	e = copy(ex,length(ex));
	fsfile(e);
	if(sfbeg(e) != 0)
		goto edone;
	temp=0;
	c = sbackc(e);
	if(c<0) {
		temp++;
		chsign(e);
	}
	while(length(e) != 0) {
		e1=div(e,sqtemp);
		release(e);
		e = e1;
		n = length(rem);
		release(rem);
		if(n != 0) {
			e1=mult(p,r);
			release(r);
			r = e1;
		}
		t = copy(p,length(p));
		cp = mult(p,t);
		release(p);
		release(t);
		p = cp;
	}
	if(temp != 0) {
		if((c = length(base)) == 0) {
			goto edone;
		}
		if(c>1)
			create(r);
		else {
			rewind(base);
			if((c = sgetc(base))<=1) {
				create(r);
				sputc(r,c);
			} else
				create(r);
		}
	}
edone:
	release(p);
	release(e);
	return(r);
}

void
init(int argc, char *argv[])
{
	Sym *sp;
	Dir *d;

	ARGBEGIN {
	default:
		dbg = 1;
		break;
	} ARGEND
	ifile = 1;
	curfile = &bin;
	if(*argv){
		d = dirstat(*argv);
		if(d == nil) {
			fprint(2, "dc: can't open file %s\n", *argv);
			exits("open");
		}
		if(d->mode & DMDIR) {
			fprint(2, "dc: file %s is a directory\n", *argv);
			exits("open");
		}
		free(d);
		if((curfile = Bopen(*argv, OREAD)) == 0) {
			fprint(2,"dc: can't open file %s\n", *argv);
			exits("open");
		}
	}
/*	dummy = malloc(0);  *//* prepare for garbage-collection */
	scalptr = salloc(1);
	sputc(scalptr,0);
	basptr = salloc(1);
	sputc(basptr,10);
	obase=10;
	logten=log2(10L);
	ll=70;
	fw=1;
	fw1=0;
	tenptr = salloc(1);
	sputc(tenptr,10);
	obase=10;
	inbas = salloc(1);
	sputc(inbas,10);
	sqtemp = salloc(1);
	sputc(sqtemp,2);
	chptr = salloc(0);
	strptr = salloc(0);
	divxyz = salloc(0);
	stkbeg = stkptr = &stack[0];
	stkend = &stack[STKSZ];
	stkerr = 0;
	readptr = &readstk[0];
	k=0;
	sp = sptr = &symlst[0];
	while(sptr < &symlst[TBLSZ-1]) {
		sptr->next = ++sp;
		sptr++;
	}
	sptr->next=0;
	sfree = &symlst[0];
}

void
pushp(Blk *p)
{
	if(stkptr == stkend) {
		Bprint(&bout,"out of stack space\n");
		return;
	}
	stkerr=0;
	*++stkptr = p;
	return;
}

Blk*
pop(void)
{
	if(stkptr == stack) {
		stkerr=1;
		return(0);
	}
	return(*stkptr--);
}

Blk*
readin(void)
{
	Blk *p, *q;
	int dp, dpct, c;

	dp = dpct=0;
	p = salloc(0);
	for(;;){
		c = readc();
		switch(c) {
		case '.':
			if(dp != 0)
				goto gotnum;
			dp++;
			continue;
		case '\\':
			readc();
			continue;
		default:
			if(c >= 'A' && c <= 'F')
				c = c - 'A' + 10;
			else
			if(c >= '0' && c <= '9')
				c -= '0';
			else
				goto gotnum;
			if(dp != 0) {
				if(dpct >= 99)
					continue;
				dpct++;
			}
			create(chptr);
			if(c != 0)
				sputc(chptr,c);
			q = mult(p,inbas);
			release(p);
			p = add(chptr,q);
			release(q);
		}
	}
gotnum:
	unreadc(c);
	if(dp == 0) {
		sputc(p,0);
		return(p);
	} else {
		q = scale(p,dpct);
		return(q);
	}
}

/*
 * returns pointer to struct with ct 0's & p
 */
Blk*
add0(Blk *p, int ct)
{
	Blk *q, *t;

	q = salloc(length(p)+(ct+1)/2);
	while(ct>1) {
		sputc(q,0);
		ct -= 2;
	}
	rewind(p);
	while(sfeof(p) == 0) {
		sputc(q,sgetc(p));
	}
	release(p);
	if(ct == 1) {
		t = mult(tenptr,q);
		release(q);
		return(t);
	}
	return(q);
}

Blk*
mult(Blk *p, Blk *q)
{
	Blk *mp, *mq, *mr;
	int sign, offset, carry;
	int cq, cp, mt, mcr;

	offset = sign = 0;
	fsfile(p);
	mp = p;
	if(sfbeg(p) == 0) {
		if(sbackc(p)<0) {
			mp = copy(p,length(p));
			chsign(mp);
			sign = ~sign;
		}
	}
	fsfile(q);
	mq = q;
	if(sfbeg(q) == 0){
		if(sbackc(q)<0) {
			mq = copy(q,length(q));
			chsign(mq);
			sign = ~sign;
		}
	}
	mr = salloc(length(mp)+length(mq));
	zero(mr);
	rewind(mq);
	while(sfeof(mq) == 0) {
		cq = sgetc(mq);
		rewind(mp);
		rewind(mr);
		mr->rd += offset;
		carry=0;
		while(sfeof(mp) == 0) {
			cp = sgetc(mp);
			mcr = sfeof(mr)?0:slookc(mr);
			mt = cp*cq + carry + mcr;
			carry = mt/100;
			salterc(mr,mt%100);
		}
		offset++;
		if(carry != 0) {
			mcr = sfeof(mr)?0:slookc(mr);
			salterc(mr,mcr+carry);
		}
	}
	if(sign < 0) {
		chsign(mr);
	}
	if(mp != p)
		release(mp);
	if(mq != q)
		release(mq);
	return(mr);
}

void
chsign(Blk *p)
{
	int carry;
	char ct;

	carry=0;
	rewind(p);
	while(sfeof(p) == 0) {
		ct=100-slookc(p)-carry;
		carry=1;
		if(ct>=100) {
			ct -= 100;
			carry=0;
		}
		salterc(p,ct);
	}
	if(carry != 0) {
		sputc(p,-1);
		fsfile(p);
		backc(p);
		ct = sbackc(p);
		if(ct == 99 /*&& !sfbeg(p)*/) {
			truncate(p);
			sputc(p,-1);
		}
	} else{
		fsfile(p);
		ct = sbackc(p);
		if(ct == 0)
			truncate(p);
	}
	return;
}

int
readc(void)
{
loop:
	if((readptr != &readstk[0]) && (*readptr != 0)) {
		if(sfeof(*readptr) == 0)
			return(lastchar = sgetc(*readptr));
		release(*readptr);
		readptr--;
		goto loop;
	}
	lastchar = Bgetc(curfile);
	if(lastchar != -1)
		return(lastchar);
	if(readptr != &readptr[0]) {
		readptr--;
		if(*readptr == 0)
			curfile = &bin;
		goto loop;
	}
	if(curfile != &bin) {
		Bterm(curfile);
		curfile = &bin;
		goto loop;
	}
	exits(0);
	return 0;	/* shut up ken */
}

void
unreadc(char c)
{

	if((readptr != &readstk[0]) && (*readptr != 0)) {
		sungetc(*readptr,c);
	} else
		Bungetc(curfile);
	return;
}

void
binop(char c)
{
	Blk *r;

	r = 0;
	switch(c) {
	case '+':
		r = add(arg1,arg2);
		break;
	case '*':
		r = mult(arg1,arg2);
		break;
	case '/':
		r = div(arg1,arg2);
		break;
	}
	release(arg1);
	release(arg2);
	sputc(r,savk);
	pushp(r);
}

void
dcprint(Blk *hptr)
{
	Blk *p, *q, *dec;
	int dig, dout, ct, sc;

	rewind(hptr);
	while(sfeof(hptr) == 0) {
		if(sgetc(hptr)>99) {
			rewind(hptr);
			while(sfeof(hptr) == 0) {
				Bprint(&bout,"%c",sgetc(hptr));
			}
			Bprint(&bout,"\n");
			return;
		}
	}
	fsfile(hptr);
	sc = sbackc(hptr);
	if(sfbeg(hptr) != 0) {
		Bprint(&bout,"0\n");
		return;
	}
	count = ll;
	p = copy(hptr,length(hptr));
	sclobber(p);
	fsfile(p);
	if(sbackc(p)<0) {
		chsign(p);
		OUTC('-');
	}
	if((obase == 0) || (obase == -1)) {
		oneot(p,sc,'d');
		return;
	}
	if(obase == 1) {
		oneot(p,sc,'1');
		return;
	}
	if(obase == 10) {
		tenot(p,sc);
		return;
	}
	/* sleazy hack to scale top of stack - divide by 1 */
	pushp(p);
	sputc(p, sc);
	p=salloc(0);
	create(p);
	sputc(p, 1);
	sputc(p, 0);
	pushp(p);
	if(dscale() != 0)
		return;
	p = div(arg1, arg2);
	release(arg1);
	release(arg2);
	sc = savk;

	create(strptr);
	dig = logten*sc;
	dout = ((dig/10) + dig) / logo;
	dec = getdec(p,sc);
	p = removc(p,sc);
	while(length(p) != 0) {
		q = div(p,basptr);
		release(p);
		p = q;
		(*outdit)(rem,0);
	}
	release(p);
	fsfile(strptr);
	while(sfbeg(strptr) == 0)
		OUTC(sbackc(strptr));
	if(sc == 0) {
		release(dec);
		Bprint(&bout,"\n");
		return;
	}
	create(strptr);
	OUTC('.');
	ct=0;
	do {
		q = mult(basptr,dec);
		release(dec);
		dec = getdec(q,sc);
		p = removc(q,sc);
		(*outdit)(p,1);
	} while(++ct < dout);
	release(dec);
	rewind(strptr);
	while(sfeof(strptr) == 0)
		OUTC(sgetc(strptr));
	Bprint(&bout,"\n");
}

Blk*
getdec(Blk *p, int sc)
{
	int cc;
	Blk *q, *t, *s;

	rewind(p);
	if(length(p)*2 < sc) {
		q = copy(p,length(p));
		return(q);
	}
	q = salloc(length(p));
	while(sc >= 1) {
		sputc(q,sgetc(p));
		sc -= 2;
	}
	if(sc != 0) {
		t = mult(q,tenptr);
		s = salloc(cc = length(q));
		release(q);
		rewind(t);
		while(cc-- > 0)
			sputc(s,sgetc(t));
		sputc(s,0);
		release(t);
		t = div(s,tenptr);
		release(s);
		release(rem);
		return(t);
	}
	return(q);
}

void
tenot(Blk *p, int sc)
{
	int c, f;

	fsfile(p);
	f=0;
	while((sfbeg(p) == 0) && ((p->rd-p->beg-1)*2 >= sc)) {
		c = sbackc(p);
		if((c<10) && (f == 1))
			Bprint(&bout,"0%d",c);
		else
			Bprint(&bout,"%d",c);
		f=1;
		TEST2;
	}
	if(sc == 0) {
		Bprint(&bout,"\n");
		release(p);
		return;
	}
	if((p->rd-p->beg)*2 > sc) {
		c = sbackc(p);
		Bprint(&bout,"%d.",c/10);
		TEST2;
		OUTC(c%10 +'0');
		sc--;
	} else {
		OUTC('.');
	}
	while(sc>(p->rd-p->beg)*2) {
		OUTC('0');
		sc--;
	}
	while(sc > 1) {
		c = sbackc(p);
		if(c<10)
			Bprint(&bout,"0%d",c);
		else
			Bprint(&bout,"%d",c);
		sc -= 2;
		TEST2;
	}
	if(sc == 1) {
		OUTC(sbackc(p)/10 +'0');
	}
	Bprint(&bout,"\n");
	release(p);
}

void
oneot(Blk *p, int sc, char ch)
{
	Blk *q;

	q = removc(p,sc);
	create(strptr);
	sputc(strptr,-1);
	while(length(q)>0) {
		p = add(strptr,q);
		release(q);
		q = p;
		OUTC(ch);
	}
	release(q);
	Bprint(&bout,"\n");
}

void
hexot(Blk *p, int flg)
{
	int c;

	USED(flg);
	rewind(p);
	if(sfeof(p) != 0) {
		sputc(strptr,'0');
		release(p);
		return;
	}
	c = sgetc(p);
	release(p);
	if(c >= 16) {
		Bprint(&bout,"hex digit > 16");
		return;
	}
	sputc(strptr,c<10?c+'0':c-10+'a');
}

void
bigot(Blk *p, int flg)
{
	Blk *t, *q;
	int neg, l;

	if(flg == 1) {
		t = salloc(0);
		l = 0;
	} else {
		t = strptr;
		l = length(strptr)+fw-1;
	}
	neg=0;
	if(length(p) != 0) {
		fsfile(p);
		if(sbackc(p)<0) {
			neg=1;
			chsign(p);
		}
		while(length(p) != 0) {
			q = div(p,tenptr);
			release(p);
			p = q;
			rewind(rem);
			sputc(t,sfeof(rem)?'0':sgetc(rem)+'0');
			release(rem);
		}
	}
	release(p);
	if(flg == 1) {
		l = fw1-length(t);
		if(neg != 0) {
			l--;
			sputc(strptr,'-');
		}
		fsfile(t);
		while(l-- > 0)
			sputc(strptr,'0');
		while(sfbeg(t) == 0)
			sputc(strptr,sbackc(t));
		release(t);
	} else {
		l -= length(strptr);
		while(l-- > 0)
			sputc(strptr,'0');
		if(neg != 0) {
			sclobber(strptr);
			sputc(strptr,'-');
		}
	}
	sputc(strptr,' ');
}

Blk*
add(Blk *a1, Blk *a2)
{
	Blk *p;
	int carry, n, size, c, n1, n2;

	size = length(a1)>length(a2)?length(a1):length(a2);
	p = salloc(size);
	rewind(a1);
	rewind(a2);
	carry=0;
	while(--size >= 0) {
		n1 = sfeof(a1)?0:sgetc(a1);
		n2 = sfeof(a2)?0:sgetc(a2);
		n = n1 + n2 + carry;
		if(n>=100) {
			carry=1;
			n -= 100;
		} else
		if(n<0) {
			carry = -1;
			n += 100;
		} else
			carry = 0;
		sputc(p,n);
	}
	if(carry != 0)
		sputc(p,carry);
	fsfile(p);
	if(sfbeg(p) == 0) {
		c = 0;
		while(sfbeg(p) == 0 && (c = sbackc(p)) == 0)
			;
		if(c != 0)
			salterc(p,c);
		truncate(p);
	}
	fsfile(p);
	if(sfbeg(p) == 0 && sbackc(p) == -1) {
		while((c = sbackc(p)) == 99) {
			if(c == -1)
				break;
		}
		skipc(p);
		salterc(p,-1);
		truncate(p);
	}
	return(p);
}

int
eqk(void)
{
	Blk *p, *q;
	int skp, skq;

	p = pop();
	EMPTYS;
	q = pop();
	EMPTYSR(p);
	skp = sunputc(p);
	skq = sunputc(q);
	if(skp == skq) {
		arg1=p;
		arg2=q;
		savk = skp;
		return(0);
	}
	if(skp < skq) {
		savk = skq;
		p = add0(p,skq-skp);
	} else {
		savk = skp;
		q = add0(q,skp-skq);
	}
	arg1=p;
	arg2=q;
	return(0);
}

Blk*
removc(Blk *p, int n)
{
	Blk *q, *r;

	rewind(p);
	while(n>1) {
		skipc(p);
		n -= 2;
	}
	q = salloc(2);
	while(sfeof(p) == 0)
		sputc(q,sgetc(p));
	if(n == 1) {
		r = div(q,tenptr);
		release(q);
		release(rem);
		q = r;
	}
	release(p);
	return(q);
}

Blk*
scalint(Blk *p)
{
	int n;

	n = sunputc(p);
	p = removc(p,n);
	return(p);
}

Blk*
scale(Blk *p, int n)
{
	Blk *q, *s, *t;

	t = add0(p,n);
	q = salloc(1);
	sputc(q,n);
	s = dcexp(inbas,q);
	release(q);
	q = div(t,s);
	release(t);
	release(s);
	release(rem);
	sputc(q,n);
	return(q);
}

int
subt(void)
{
	arg1=pop();
	EMPTYS;
	savk = sunputc(arg1);
	chsign(arg1);
	sputc(arg1,savk);
	pushp(arg1);
	if(eqk() != 0)
		return(1);
	binop('+');
	return(0);
}

int
command(void)
{
	char line[100], *sl;
	int pid, p, c;

	switch(c = readc()) {
	case '<':
		return(cond(NL));
	case '>':
		return(cond(NG));
	case '=':
		return(cond(NE));
	default:
		sl = line;
		*sl++ = c;
		while((c = readc()) != '\n')
			*sl++ = c;
		*sl = 0;
		if((pid = fork()) == 0) {
			execl("/bin/rc","rc","-c",line,0);
			exits("shell");
		}
		for(;;) {
			if((p = waitpid()) < 0)
				break;
			if(p== pid)
				break;
		}
		Bprint(&bout,"!\n");
		return(0);
	}
}

int
cond(char c)
{
	Blk *p;
	int cc;

	if(subt() != 0)
		return(1);
	p = pop();
	sclobber(p);
	if(length(p) == 0) {
		release(p);
		if(c == '<' || c == '>' || c == NE) {
			getstk();
			return(0);
		}
		load();
		return(1);
	}
	if(c == '='){
		release(p);
		getstk();
		return(0);
	}
	if(c == NE) {
		release(p);
		load();
		return(1);
	}
	fsfile(p);
	cc = sbackc(p);
	release(p);
	if((cc<0 && (c == '<' || c == NG)) ||
	   (cc >0) && (c == '>' || c == NL)) {
		getstk();
		return(0);
	}
	load();
	return(1);
}

void
load(void)
{
	int c;
	Blk *p, *q, *t, *s;

	c = getstk() & 0377;
	sptr = stable[c];
	if(sptr != 0) {
		p = sptr->val;
		if(c >= ARRAYST) {
			q = salloc(length(p));
			rewind(p);
			while(sfeof(p) == 0) {
				s = dcgetwd(p);
				if(s == 0) {
					putwd(q, (Blk*)0);
				} else {
					t = copy(s,length(s));
					putwd(q,t);
				}
			}
			pushp(q);
		} else {
			q = copy(p,length(p));
			pushp(q);
		}
	} else {
		q = salloc(1);
		if(c <= LASTFUN) {
			Bprint(&bout,"function %c undefined\n",c+'a'-1);
			sputc(q,'c');
			sputc(q,'0');
			sputc(q,' ');
			sputc(q,'1');
			sputc(q,'Q');
		}
		else
			sputc(q,0);
		pushp(q);
	}
}

int
log2(long n)
{
	int i;

	if(n == 0)
		return(0);
	i=31;
	if(n<0)
		return(i);
	while((n= n<<1) >0)
		i--;
	return i-1;
}

Blk*
salloc(int size)
{
	Blk *hdr;
	char *ptr;

	all++;
	lall++;
	if(all - rel > active)
		active = all - rel;
	nbytes += size;
	lbytes += size;
	if(nbytes >maxsize)
		maxsize = nbytes;
	if(size > longest)
		longest = size;
	ptr = malloc((unsigned)size);
	if(ptr == 0){
		garbage("salloc");
		if((ptr = malloc((unsigned)size)) == 0)
			ospace("salloc");
	}
	if((hdr = hfree) == 0)
		hdr = morehd();
	hfree = (Blk *)hdr->rd;
	hdr->rd = hdr->wt = hdr->beg = ptr;
	hdr->last = ptr+size;
	return(hdr);
}

Blk*
morehd(void)
{
	Blk *h, *kk;

	headmor++;
	nbytes += HEADSZ;
	hfree = h = (Blk *)malloc(HEADSZ);
	if(hfree == 0) {
		garbage("morehd");
		if((hfree = h = (Blk*)malloc(HEADSZ)) == 0)
			ospace("headers");
	}
	kk = h;
	while(h<hfree+(HEADSZ/BLK))
		(h++)->rd = (char*)++kk;
	(h-1)->rd=0;
	return(hfree);
}

Blk*
copy(Blk *hptr, int size)
{
	Blk *hdr;
	unsigned sz;
	char *ptr;

	all++;
	lall++;
	lcopy++;
	nbytes += size;
	lbytes += size;
	if(size > longest)
		longest = size;
	if(size > maxsize)
		maxsize = size;
	sz = length(hptr);
	ptr = malloc(size);
	if(ptr == 0) {
		Bprint(&bout,"copy size %d\n",size);
		ospace("copy");
	}
	memmove(ptr, hptr->beg, sz);
	memset(ptr+sz, 0, size-sz);
	if((hdr = hfree) == 0)
		hdr = morehd();
	hfree = (Blk *)hdr->rd;
	hdr->rd = hdr->beg = ptr;
	hdr->last = ptr+size;
	hdr->wt = ptr+sz;
	ptr = hdr->wt;
	while(ptr<hdr->last)
		*ptr++ = '\0';
	return(hdr);
}

void
sdump(char *s1, Blk *hptr)
{
	char *p;

	Bprint(&bout,"%s %lx rd %lx wt %lx beg %lx last %lx\n",
		s1,hptr,hptr->rd,hptr->wt,hptr->beg,hptr->last);
	p = hptr->beg;
	while(p < hptr->wt)
		Bprint(&bout,"%d ",*p++);
	Bprint(&bout,"\n");
}

void
seekc(Blk *hptr, int n)
{
	char *nn,*p;

	nn = hptr->beg+n;
	if(nn > hptr->last) {
		nbytes += nn - hptr->last;
		if(nbytes > maxsize)
			maxsize = nbytes;
		lbytes += nn - hptr->last;
		if(n > longest)
			longest = n;
/*		free(hptr->beg); */
		p = realloc(hptr->beg, n);
		if(p == 0) {
/*			hptr->beg = realloc(hptr->beg, hptr->last-hptr->beg);
**			garbage("seekc");
**			if((p = realloc(hptr->beg, n)) == 0)
*/				ospace("seekc");
		}
		hptr->beg = p;
		hptr->wt = hptr->last = hptr->rd = p+n;
		return;
	}
	hptr->rd = nn;
	if(nn>hptr->wt)
		hptr->wt = nn;
}

void
salterwd(Blk *ahptr, Blk *n)
{
	Wblk *hptr;

	hptr = (Wblk*)ahptr;
	if(hptr->rdw == hptr->lastw)
		more(ahptr);
	*hptr->rdw++ = n;
	if(hptr->rdw > hptr->wtw)
		hptr->wtw = hptr->rdw;
}

void
more(Blk *hptr)
{
	unsigned size;
	char *p;

	if((size=(hptr->last-hptr->beg)*2) == 0)
		size=2;
	nbytes += size/2;
	if(nbytes > maxsize)
		maxsize = nbytes;
	if(size > longest)
		longest = size;
	lbytes += size/2;
	lmore++;
/*	free(hptr->beg);*/
	p = realloc(hptr->beg, size);

	if(p == 0) {
/*		hptr->beg = realloc(hptr->beg, (hptr->last-hptr->beg));
**		garbage("more");
**		if((p = realloc(hptr->beg,size)) == 0)
*/			ospace("more");
	}
	hptr->rd = p + (hptr->rd - hptr->beg);
	hptr->wt = p + (hptr->wt - hptr->beg);
	hptr->beg = p;
	hptr->last = p+size;
}

void
ospace(char *s)
{
	Bprint(&bout,"out of space: %s\n",s);
	Bprint(&bout,"all %ld rel %ld headmor %ld\n",all,rel,headmor);
	Bprint(&bout,"nbytes %ld\n",nbytes);
	sdump("stk",*stkptr);
	abort();
}

void
garbage(char *s)
{
	USED(s);
}

void
release(Blk *p)
{
	rel++;
	lrel++;
	nbytes -= p->last - p->beg;
	p->rd = (char*)hfree;
	hfree = p;
	free(p->beg);
}

Blk*
dcgetwd(Blk *p)
{
	Wblk *wp;

	wp = (Wblk*)p;
	if(wp->rdw == wp->wtw)
		return(0);
	return(*wp->rdw++);
}

void
putwd(Blk *p, Blk *c)
{
	Wblk *wp;

	wp = (Wblk*)p;
	if(wp->wtw == wp->lastw)
		more(p);
	*wp->wtw++ = c;
}

Blk*
lookwd(Blk *p)
{
	Wblk *wp;

	wp = (Wblk*)p;
	if(wp->rdw == wp->wtw)
		return(0);
	return(*wp->rdw);
}

int
getstk(void)
{
	int n;
	uchar c;

	c = readc();
	if(c != '<')
		return c;
	n = 0;
	while(1) {
		c = readc();
		if(c == '>')
			break;
		n = n*10+c-'0';
	}
	return n;
}
