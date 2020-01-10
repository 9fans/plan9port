#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <event.h>
#include "sky.h"
#include "strings.c"

enum
{
	NNGC=7840,	/* number of NGC numbers [1..NNGC] */
	NIC = 5386,	/* number of IC numbers */
	NNGCrec=NNGC+NIC,	/* number of records in the NGC catalog (including IC's, starting at NNGC */
	NMrec=122,	/* number of M records */
	NM=110,		/* number of M numbers */
	NAbell=2712,	/* number of records in the Abell catalog */
	NName=1000,	/* number of prose names; estimated maximum (read from editable text file) */
	NBayer=1517,	/* number of bayer entries */
	NSAO=258998,	/* number of SAO stars */
	MAXcon=1932,	/* maximum number of patches in a constellation */
	Ncon=88,	/* number of constellations */
	Npatch=92053,	/* highest patch number */
};

char		ngctype[NNGCrec];
Mindexrec	mindex[NMrec];
Namerec		name[NName];
Bayerec		bayer[NBayer];
int32		con[MAXcon];
ushort		conindex[Ncon+1];
int32		patchaddr[Npatch+1];

Record	*rec;
Record	*orec;
Record	*cur;

char	*dir;
int	saodb;
int	ngcdb;
int	abelldb;
int	ngctypedb;
int	mindexdb;
int	namedb;
int	bayerdb;
int	condb;
int	conindexdb;
int	patchdb;
char	parsed[3];
int32	nrec;
int32	nreca;
int32	norec;
int32	noreca;

Biobuf	bin;
Biobuf	bout;

void
main(int argc, char *argv[])
{
	char *line;

	dir = unsharp(DIR);
	Binit(&bin, 0, OREAD);
	Binit(&bout, 1, OWRITE);
	if(argc != 1)
		dir = argv[1];
	astro("", 1);
	while(line = Brdline(&bin, '\n')){
		line[Blinelen(&bin)-1] = 0;
		lookup(line, 1);
		Bflush(&bout);
	}
	if(display != nil){
		closedisplay(display);
		/* automatic refresh of rio window is triggered by mouse */
	/*	close(open("/dev/mouse", OREAD)); */
	}
	return;
}

void
reset(void)
{
	nrec = 0;
	cur = rec;
}

void
grow(void)
{
	nrec++;
	if(nreca < nrec){
		nreca = nrec+50;
		rec = realloc(rec, nreca*sizeof(Record));
		if(rec == 0){
			fprint(2, "scat: realloc fails\n");
			exits("realloc");
		}
	}
	cur = rec+nrec-1;
}

void
copy(void)
{
	if(noreca < nreca){
		noreca = nreca;
		orec = realloc(orec, nreca*sizeof(Record));
		if(orec == 0){
			fprint(2, "scat: realloc fails\n");
			exits("realloc");
		}
	}
	memmove(orec, rec, nrec*sizeof(Record));
	norec = nrec;
}

int
eopen(char *s)
{
	char buf[128];
	int f;

	sprint(buf, "%s/%s.scat", dir, s);
	f = open(buf, 0);
	if(f<0){
		fprint(2, "scat: can't open %s\n", buf);
		exits("open");
	}
	return f;
}


void
Eread(int f, char *name, void *addr, int32 n)
{
	if(read(f, addr, n) != n){	/* BUG! */
		fprint(2, "scat: read error on %s\n", name);
		exits("read");
	}
}

char*
skipbl(char *s)
{
	while(*s!=0 && (*s==' ' || *s=='\t'))
		s++;
	return s;
}

char*
skipstr(char *s, char *t)
{
	while(*s && *s==*t)
		s++, t++;
	return skipbl(s);
}

/* produce little-endian int32 at address l */
int32
Long(int32 *l)
{
	uchar *p;

	p = (uchar*)l;
	return (int32)p[0]|((int32)p[1]<<8)|((int32)p[2]<<16)|((int32)p[3]<<24);
}

/* produce little-endian int32 at address l */
int
Short(short *s)
{
	uchar *p;

	p = (uchar*)s;
	return p[0]|(p[1]<<8);
}

void
nameopen(void)
{
	Biobuf b;
	int i;
	char *l, *p;

	if(namedb == 0){
		namedb = eopen("name");
		Binit(&b, namedb, OREAD);
		for(i=0; i<NName; i++){
			l = Brdline(&b, '\n');
			if(l == 0)
				break;
			p = strchr(l, '\t');
			if(p == 0){
		Badformat:
				Bprint(&bout, "warning: name.scat bad format; line %d\n", i+1);
				break;
			}
			*p++ = 0;
			strcpy(name[i].name, l);
			if(strncmp(p, "ngc", 3) == 0)
				name[i].ngc = atoi(p+3);
			else if(strncmp(p, "ic", 2) == 0)
				name[i].ngc = atoi(p+2)+NNGC;
			else if(strncmp(p, "sao", 3) == 0)
				name[i].sao = atoi(p+3);
			else if(strncmp(p, "abell", 5) == 0)
				name[i].abell = atoi(p+5);
			else
				goto Badformat;
		}
		if(i == NName)
			Bprint(&bout, "warning: too many names in name.scat (max %d); extra ignored\n", NName);
		close(namedb);

		bayerdb = eopen("bayer");
		Eread(bayerdb, "bayer", bayer, sizeof bayer);
		close(bayerdb);
		for(i=0; i<NBayer; i++)
			bayer[i].sao = Long(&bayer[i].sao);
	}
}

void
saoopen(void)
{
	if(saodb == 0){
		nameopen();
		saodb = eopen("sao");
	}
}

void
ngcopen(void)
{
	if(ngcdb == 0){
		nameopen();
		ngcdb = eopen("ngc2000");
		ngctypedb = eopen("ngc2000type");
		Eread(ngctypedb, "ngctype", ngctype, sizeof ngctype);
		close(ngctypedb);
	}
}

void
abellopen(void)
{
	/* nothing extra to do with abell: it's directly indexed by number */
	if(abelldb == 0)
		abelldb = eopen("abell");
}

void
patchopen(void)
{
	Biobuf *b;
	int32 l, m;
	char buf[100];

	if(patchdb == 0){
		patchdb = eopen("patch");
		sprint(buf, "%s/patchindex.scat", dir);
		b = Bopen(buf, OREAD);
		if(b == 0){
			fprint(2, "can't open %s\n", buf);
			exits("open");
		}
		for(m=0,l=0; l<=Npatch; l++)
			patchaddr[l] = m += Bgetc(b)*4;
		Bterm(b);
	}
}

void
mopen(void)
{
	int i;

	if(mindexdb == 0){
		mindexdb = eopen("mindex");
		Eread(mindexdb, "mindex", mindex, sizeof mindex);
		close(mindexdb);
		for(i=0; i<NMrec; i++)
			mindex[i].ngc = Short(&mindex[i].ngc);
	}
}

void
constelopen(void)
{
	int i;

	if(condb == 0){
		condb = eopen("con");
		conindexdb = eopen("conindex");
		Eread(conindexdb, "conindex", conindex, sizeof conindex);
		close(conindexdb);
		for(i=0; i<Ncon+1; i++)
			conindex[i] = Short((short*)&conindex[i]);
	}
}

void
lowercase(char *s)
{
	for(; *s; s++)
		if('A'<=*s && *s<='Z')
			*s += 'a'-'A';
}

int
loadngc(int32 index)
{
	static int failed;
	int32 j;

	ngcopen();
	j = (index-1)*sizeof(NGCrec);
	grow();
	cur->type = NGC;
	cur->index = index;
	seek(ngcdb, j, 0);
	/* special case: NGC data may not be available */
	if(read(ngcdb, &cur->u.ngc, sizeof(NGCrec)) != sizeof(NGCrec)){
		if(!failed){
			fprint(2, "scat: NGC database not available\n");
			failed++;
		}
		cur->type = NONGC;
		cur->u.ngc.ngc = 0;
		cur->u.ngc.ra = 0;
		cur->u.ngc.dec = 0;
		cur->u.ngc.diam = 0;
		cur->u.ngc.mag = 0;
		return 0;
	}
	cur->u.ngc.ngc = Short(&cur->u.ngc.ngc);
	cur->u.ngc.ra = Long(&cur->u.ngc.ra);
	cur->u.ngc.dec = Long(&cur->u.ngc.dec);
	cur->u.ngc.diam = Long(&cur->u.ngc.diam);
	cur->u.ngc.mag = Short(&cur->u.ngc.mag);
	return 1;
}

int
loadabell(int32 index)
{
	int32 j;

	abellopen();
	j = index-1;
	grow();
	cur->type = Abell;
	cur->index = index;
	seek(abelldb, j*sizeof(Abellrec), 0);
	Eread(abelldb, "abell", &cur->u.abell, sizeof(Abellrec));
	cur->u.abell.abell = Short(&cur->u.abell.abell);
	if(cur->u.abell.abell != index){
		fprint(2, "bad format in abell catalog\n");
		exits("abell");
	}
	cur->u.abell.ra = Long(&cur->u.abell.ra);
	cur->u.abell.dec = Long(&cur->u.abell.dec);
	cur->u.abell.glat = Long(&cur->u.abell.glat);
	cur->u.abell.glong = Long(&cur->u.abell.glong);
	cur->u.abell.rad = Long(&cur->u.abell.rad);
	cur->u.abell.mag10 = Short(&cur->u.abell.mag10);
	cur->u.abell.pop = Short(&cur->u.abell.pop);
	cur->u.abell.dist = Short(&cur->u.abell.dist);
	return 1;
}

int
loadsao(int index)
{
	if(index<=0 || index>NSAO)
		return 0;
	saoopen();
	grow();
	cur->type = SAO;
	cur->index = index;
	seek(saodb, (index-1)*sizeof(SAOrec), 0);
	Eread(saodb, "sao", &cur->u.sao, sizeof(SAOrec));
	cur->u.sao.ra = Long(&cur->u.sao.ra);
	cur->u.sao.dec = Long(&cur->u.sao.dec);
	cur->u.sao.dra = Long(&cur->u.sao.dra);
	cur->u.sao.ddec = Long(&cur->u.sao.ddec);
	cur->u.sao.mag = Short(&cur->u.sao.mag);
	cur->u.sao.mpg = Short(&cur->u.sao.mpg);
	cur->u.sao.hd = Long(&cur->u.sao.hd);
	return 1;
}

int
loadplanet(int index, Record *r)
{
	if(index<0 || index>NPlanet || planet[index].name[0]=='\0')
		return 0;
	grow();
	cur->type = Planet;
	cur->index = index;
	/* check whether to take new or existing record */
	if(r == nil)
		memmove(&cur->u.planet, &planet[index], sizeof(Planetrec));
	else
		memmove(&cur->u.planet, &r->u.planet, sizeof(Planetrec));
	return 1;
}

int
loadpatch(int32 index)
{
	int i;

	patchopen();
	if(index<=0 || index>Npatch)
		return 0;
	grow();
	cur->type = Patch;
	cur->index = index;
	seek(patchdb, patchaddr[index-1], 0);
	cur->u.patch.nkey = (patchaddr[index]-patchaddr[index-1])/4;
	Eread(patchdb, "patch", cur->u.patch.key, cur->u.patch.nkey*4);
	for(i=0; i<cur->u.patch.nkey; i++)
		cur->u.patch.key[i] = Long(&cur->u.patch.key[i]);
	return 1;
}

int
loadtype(int t)
{
	int i;

	ngcopen();
	for(i=0; i<NNGCrec; i++)
		if(t == (ngctype[i])){
			grow();
			cur->type = NGCN;
			cur->index = i+1;
		}
	return 1;
}

void
flatten(void)
{
	int i, j, notflat;
	Record *or;
	int32 key;

    loop:
	copy();
	reset();
	notflat = 0;
	for(i=0,or=orec; i<norec; i++,or++){
		switch(or->type){
		default:
			fprint(2, "bad type %d in flatten\n", or->type);
			break;

		case NONGC:
			break;

		case Planet:
		case Abell:
		case NGC:
		case SAO:
			grow();
			memmove(cur, or, sizeof(Record));
			break;

		case NGCN:
			if(loadngc(or->index))
				notflat = 1;
			break;

		case NamedSAO:
			loadsao(or->index);
			notflat = 1;
			break;

		case NamedNGC:
			if(loadngc(or->index))
				notflat = 1;
			break;

		case NamedAbell:
			loadabell(or->index);
			notflat = 1;
			break;

		case PatchC:
			loadpatch(or->index);
			notflat = 1;
			break;

		case Patch:
			for(j=1; j<or->u.patch.nkey; j++){
				key = or->u.patch.key[j];
				if((key&0x3F) == SAO)
					loadsao((key>>8)&0xFFFFFF);
				else if((key&0x3F) == Abell)
					loadabell((key>>8)&0xFFFFFF);
				else
					loadngc((key>>16)&0xFFFF);
			}
			break;
		}
	}
	if(notflat)
		goto loop;
}

int
ism(int index)
{
	int i;

	for(i=0; i<NMrec; i++)
		if(mindex[i].ngc == index)
			return 1;
	return 0;
}

char*
alpha(char *s, char *t)
{
	int n;

	n = strlen(t);
	if(strncmp(s, t, n)==0 && (s[n]<'a' || 'z'<s[n]))
		return skipbl(s+n);
	return 0;

}

char*
text(char *s, char *t)
{
	int n;

	n = strlen(t);
	if(strncmp(s, t, n)==0 && (s[n]==0 || s[n]==' ' || s[n]=='\t'))
		return skipbl(s+n);
	return 0;

}

int
cull(char *s, int keep, int dobbox)
{
	int i, j, nobj, keepthis;
	Record *or;
	char *t;
	int dogrtr, doless, dom, dosao, dongc, doabell;
	int mgrtr, mless;
	char obj[100];

	memset(obj, 0, sizeof(obj));
	nobj = 0;
	dogrtr = 0;
	doless = 0;
	dom = 0;
	dongc = 0;
	dosao = 0;
	doabell = 0;
	mgrtr = mless= 0;
	if(dobbox)
		goto Cull;
	for(;;){
		if(s[0] == '>'){
			dogrtr = 1;
			mgrtr = 10 * strtod(s+1, &t);
			if(mgrtr==0  && t==s+1){
				fprint(2, "bad magnitude\n");
				return 0;
			}
			s = skipbl(t);
			continue;
		}
		if(s[0] == '<'){
			doless = 1;
			mless = 10 * strtod(s+1, &t);
			if(mless==0  && t==s+1){
				fprint(2, "bad magnitude\n");
				return 0;
			}
			s = skipbl(t);
			continue;
		}
		if(t = text(s, "m")){
 			dom = 1;
			s = t;
			continue;
		}
		if(t = text(s, "sao")){
			dosao = 1;
			s = t;
			continue;
		}
		if(t = text(s, "ngc")){
			dongc = 1;
			s = t;
			continue;
		}
		if(t = text(s, "abell")){
			doabell = 1;
			s = t;
			continue;
		}
		for(i=0; names[i].name; i++)
			if(t = alpha(s, names[i].name)){
				if(nobj > 100){
					fprint(2, "too many object types\n");
					return 0;
				}
				obj[nobj++] = names[i].type;
				s = t;
				goto Continue;
			}
		break;
	    Continue:;
	}
	if(*s){
		fprint(2, "syntax error in object list\n");
		return 0;
	}

    Cull:
	flatten();
	copy();
	reset();
	if(dom)
		mopen();
	if(dosao)
		saoopen();
	if(dongc || nobj)
		ngcopen();
	if(doabell)
		abellopen();
	for(i=0,or=orec; i<norec; i++,or++){
		keepthis = !keep;
		if(dobbox && inbbox(or->u.ngc.ra, or->u.ngc.dec))
			keepthis = keep;
		if(doless && or->u.ngc.mag <= mless)
			keepthis = keep;
		if(dogrtr && or->u.ngc.mag >= mgrtr)
			keepthis = keep;
		if(dom && (or->type==NGC && ism(or->u.ngc.ngc)))
			keepthis = keep;
		if(dongc && or->type==NGC)
			keepthis = keep;
		if(doabell && or->type==Abell)
			keepthis = keep;
		if(dosao && or->type==SAO)
			keepthis = keep;
		for(j=0; j<nobj; j++)
			if(or->type==NGC && or->u.ngc.type==obj[j])
				keepthis = keep;
		if(keepthis){
			grow();
			memmove(cur, or, sizeof(Record));
		}
	}
	return 1;
}

int
compar(const void *va, const void *vb)
{
	Record *a=(Record*)va, *b=(Record*)vb;

	if(a->type == b->type)
		return a->index - b->index;
	return a->type - b->type;
}

void
sort(void)
{
	int i;
	Record *r, *s;

	if(nrec == 0)
		return;
	qsort(rec, nrec, sizeof(Record), compar);
	r = rec+1;
	s = rec;
	for(i=1; i<nrec; i++,r++){
		/* may have multiple instances of a planet in the scene */
		if(r->type==s->type && r->index==s->index && r->type!=Planet)
			continue;
		memmove(++s, r, sizeof(Record));
	}
	nrec = (s+1)-rec;
}

char	greekbuf[128];

char*
togreek(char *s)
{
	char *t;
	int i, n;
	Rune r;

	t = greekbuf;
	while(*s){
		for(i=1; i<=24; i++){
			n = strlen(greek[i]);
			if(strncmp(s, greek[i], n)==0 && (s[n]==' ' || s[n]=='\t')){
				s += n;
				t += runetochar(t, &greeklet[i]);
				goto Cont;
			}
		}
		n = chartorune(&r, s);
		for(i=0; i<n; i++)
			*t++ = *s++;
    Cont:;
	}
	*t = 0;
	return greekbuf;
}

char*
fromgreek(char *s)
{
	char *t;
	int i, n;
	Rune r;

	t = greekbuf;
	while(*s){
		n = chartorune(&r, s);
		for(i=1; i<=24; i++){
			if(r == greeklet[i]){
				strcpy(t, greek[i]);
				t += strlen(greek[i]);
				s += n;
				goto Cont;
			}
		}
		for(i=0; i<n; i++)
			*t++ = *s++;
    Cont:;
	}
	*t = 0;
	return greekbuf;
}

#ifdef OLD
/*
 * Old version
 */
int
coords(int deg)
{
	int i;
	int x, y;
	Record *or;
	int32 dec, ra, ndec, nra;
	int rdeg;

	flatten();
	copy();
	reset();
	deg *= 2;
	for(i=0,or=orec; i<norec; i++,or++){
		if(or->type == Planet)	/* must keep it here */
			loadplanet(or->index, or);
		dec = or->u.ngc.dec/MILLIARCSEC;
		ra = or->u.ngc.ra/MILLIARCSEC;
		rdeg = deg/cos((dec*PI)/180);
		for(y=-deg; y<=+deg; y++){
			ndec = dec*2+y;
			if(ndec/2>=90 || ndec/2<=-90)
				continue;
			/* fp errors hurt here, so we round 1' to the pole */
			if(ndec >= 0)
				ndec = ndec*500*60*60 + 60000;
			else
				ndec = ndec*500*60*60 - 60000;
			for(x=-rdeg; x<=+rdeg; x++){
				nra = ra*2+x;
				if(nra/2 < 0)
					nra += 360*2;
				if(nra/2 >= 360)
					nra -= 360*2;
				/* fp errors hurt here, so we round up 1' */
				nra = nra/2*MILLIARCSEC + 60000;
				loadpatch(patcha(angle(nra), angle(ndec)));
			}
		}
	}
	sort();
	return 1;
}
#endif

/*
 * New version attempts to match the boundaries of the plot better.
 */
int
coords(int deg)
{
	int i;
	int x, y, xx;
	Record *or;
	int32 min, circle;
	double factor;

	flatten();
	circle = 360*MILLIARCSEC;
	deg *= MILLIARCSEC;

	/* find center */
	folded = 0;
	bbox(0, 0, 0);
	/* now expand */
	factor = cos(angle((decmax+decmin)/2));
	if(factor < .2)
		factor = .2;
	factor = floor(1/factor);
	folded = 0;
	bbox(factor*deg, deg, 1);
	Bprint(&bout, "%s to ", hms(angle(ramin)));
	Bprint(&bout, "%s\n", hms(angle(ramax)));
	Bprint(&bout, "%s to ", dms(angle(decmin)));
	Bprint(&bout, "%s\n", dms(angle(decmax)));
	copy();
	reset();
	for(i=0,or=orec; i<norec; i++,or++)
		if(or->type == Planet)	/* must keep it here */
			loadplanet(or->index, or);
	min = ramin;
	if(ramin > ramax)
		min -= circle;
	for(x=min; x<=ramax; x+=250*60*60){
		xx = x;
		if(xx < 0)
			xx += circle;
		for(y=decmin; y<=decmax; y+=250*60*60)
			if(-circle/4 < y && y < circle/4)
				loadpatch(patcha(angle(xx), angle(y)));
	}
	sort();
	cull(nil, 1, 1);
	return 1;
}

void
pplate(char *flags)
{
	int i;
	int32 c;
	int na, rah, ram, d1, d2;
	double r0;
	int ra, dec;
	int32 ramin, ramax, decmin, decmax;	/* all in degrees */
	Record *r;
	int folded;
	Angle racenter, deccenter, rasize, decsize, a[4];
	Picture *pic;

	rasize = -1.0;
	decsize = -1.0;
	na = 0;
	for(;;){
		while(*flags==' ')
			flags++;
		if(('0'<=*flags && *flags<='9') || *flags=='+' || *flags=='-'){
			if(na >= 3)
				goto err;
			a[na++] = getra(flags);
			while(*flags && *flags!=' ')
				flags++;
			continue;
		}
		if(*flags){
	err:
			Bprint(&bout, "syntax error in plate\n");
			return;
		}
		break;
	}
	switch(na){
	case 0:
		break;
	case 1:
		rasize = a[0];
		decsize = rasize;
		break;
	case 2:
		rasize = a[0];
		decsize = a[1];
		break;
	case 3:
	case 4:
		racenter = a[0];
		deccenter = a[1];
		rasize = a[2];
		if(na == 4)
			decsize = a[3];
		else
			decsize = rasize;
		if(rasize<0.0 || decsize<0.0){
			Bprint(&bout, "negative sizes\n");
			return;
		}
		goto done;
	}
	folded = 0;
	/* convert to milliarcsec */
	c = 1000*60*60;
    Again:
	if(nrec == 0){
		Bprint(&bout, "empty\n");
		return;
	}
	ramin = 0x7FFFFFFF;
	ramax = -0x7FFFFFFF;
	decmin = 0x7FFFFFFF;
	decmax = -0x7FFFFFFF;
	for(r=rec,i=0; i<nrec; i++,r++){
		if(r->type == Patch){
			radec(r->index, &rah, &ram, &dec);
			ra = 15*rah+ram/4;
			r0 = c/cos(RAD(dec));
			ra *= c;
			dec *= c;
			if(dec == 0)
				d1 = c, d2 = c;
			else if(dec < 0)
				d1 = c, d2 = 0;
			else
				d1 = 0, d2 = c;
		}else if(r->type==SAO || r->type==NGC || r->type==Abell){
			ra = r->u.ngc.ra;
			dec = r->u.ngc.dec;
			d1 = 0, d2 = 0, r0 = 0;
		}else if(r->type==NGCN){
			loadngc(r->index);
			continue;
		}else if(r->type==NamedSAO){
			loadsao(r->index);
			continue;
		}else if(r->type==NamedNGC){
			loadngc(r->index);
			continue;
		}else if(r->type==NamedAbell){
			loadabell(r->index);
			continue;
		}else
			continue;
		if(dec+d2 > decmax)
			decmax = dec+d2;
		if(dec-d1 < decmin)
			decmin = dec-d1;
		if(folded){
			ra -= 180*c;
			if(ra < 0)
				ra += 360*c;
		}
		if(ra+r0 > ramax)
			ramax = ra+r0;
		if(ra < ramin)
			ramin = ra;
	}
	if(!folded && ramax-ramin>270*c){
		folded = 1;
		goto Again;
	}
	racenter = angle(ramin+(ramax-ramin)/2);
	deccenter = angle(decmin+(decmax-decmin)/2);
	if(rasize<0 || decsize<0){
		rasize = angle(ramax-ramin)*cos(deccenter);
		decsize = angle(decmax-decmin);
	}
    done:
	if(DEG(rasize)>1.1 || DEG(decsize)>1.1){
		Bprint(&bout, "plate too big: %s", ms(rasize));
		Bprint(&bout, " x %s\n", ms(decsize));
		Bprint(&bout, "trimming to 30'x30'\n");
		rasize = RAD(0.5);
		decsize = RAD(0.5);
	}
	Bprint(&bout, "%s %s ", hms(racenter), dms(deccenter));
	Bprint(&bout, "%s", ms(rasize));
	Bprint(&bout, " x %s\n", ms(decsize));
	Bflush(&bout);
	flatten();
	pic = image(racenter, deccenter, rasize, decsize);
	if(pic == 0)
		return;
	Bprint(&bout, "plate %s locn %d %d %d %d\n", pic->name, pic->minx, pic->miny, pic->maxx, pic->maxy);
	Bflush(&bout);
	displaypic(pic);
}

void
lookup(char *s, int doreset)
{
	int i, j, k;
	int rah, ram, deg;
	char *starts, *inputline=s, *t, *u;
	Record *r;
	Rune c;
	int32 n;
	double x;
	Angle ra;

	lowercase(s);
	s = skipbl(s);

	if(*s == 0)
		goto Print;

	if(t = alpha(s, "flat")){
		if(*t){
			fprint(2, "flat takes no arguments\n");
			return;
		}
		if(nrec == 0){
			fprint(2, "no records\n");
			return;
		}
		flatten();
		goto Print;
	}

	if(t = alpha(s, "print")){
		if(*t){
			fprint(2, "print takes no arguments\n");
			return;
		}
		for(i=0,r=rec; i<nrec; i++,r++)
			prrec(r);
		return;
	}

	if(t = alpha(s, "add")){
		lookup(t, 0);
		return;
	}

	if(t = alpha(s, "sao")){
		n = strtoul(t, &u, 10);
		if(n<=0 || n>NSAO)
			goto NotFound;
		t = skipbl(u);
		if(*t){
			fprint(2, "syntax error in sao\n");
			return;
		}
		if(doreset)
			reset();
		if(!loadsao(n))
			goto NotFound;
		goto Print;
	}

	if(t = alpha(s, "ngc")){
		n = strtoul(t, &u, 10);
		if(n<=0 || n>NNGC)
			goto NotFound;
		t = skipbl(u);
		if(*t){
			fprint(2, "syntax error in ngc\n");
			return;
		}
		if(doreset)
			reset();
		if(!loadngc(n))
			goto NotFound;
		goto Print;
	}

	if(t = alpha(s, "ic")){
		n = strtoul(t, &u, 10);
		if(n<=0 || n>NIC)
			goto NotFound;
		t = skipbl(u);
		if(*t){
			fprint(2, "syntax error in ic\n");
			return;
		}
		if(doreset)
			reset();
		if(!loadngc(n+NNGC))
			goto NotFound;
		goto Print;
	}

	if(t = alpha(s, "abell")){
		n = strtoul(t, &u, 10);
		if(n<=0 || n>NAbell)
			goto NotFound;
		if(doreset)
			reset();
		if(!loadabell(n))
			goto NotFound;
		goto Print;
	}

	if(t = alpha(s, "m")){
		n = strtoul(t, &u, 10);
		if(n<=0 || n>NM)
			goto NotFound;
		mopen();
		for(j=n-1; mindex[j].m<n; j++)
			;
		if(doreset)
			reset();
		while(mindex[j].m == n){
			if(mindex[j].ngc){
				grow();
				cur->type = NGCN;
				cur->index = mindex[j].ngc;
			}
			j++;
		}
		goto Print;
	}

	for(i=1; i<=Ncon; i++)
		if(t = alpha(s, constel[i])){
			if(*t){
				fprint(2, "syntax error in constellation\n");
				return;
			}
			constelopen();
			seek(condb, 4L*conindex[i-1], 0);
			j = conindex[i]-conindex[i-1];
			Eread(condb, "con", con, 4*j);
			if(doreset)
				reset();
			for(k=0; k<j; k++){
				grow();
				cur->type = PatchC;
				cur->index = Long(&con[k]);
			}
			goto Print;
		}

	if(t = alpha(s, "expand")){
		n = 0;
		if(*t){
			if(*t<'0' && '9'<*t){
		Expanderr:
				fprint(2, "syntax error in expand\n");
				return;
			}
			n = strtoul(t, &u, 10);
			t = skipbl(u);
			if(*t)
				goto Expanderr;
		}
		coords(n);
		goto Print;
	}

	if(t = alpha(s, "plot")){
		if(nrec == 0){
			Bprint(&bout, "empty\n");
			return;
		}
		plot(t);
		return;
	}

	if(t = alpha(s, "astro")){
		astro(t, 0);
		return;
	}

	if(t = alpha(s, "plate")){
		pplate(t);
		return;
	}

	if(t = alpha(s, "gamma")){
		while(*t==' ')
			t++;
		u = t;
		x = strtod(t, &u);
		if(u > t)
			gam.gamma = x;
		Bprint(&bout, "%.2f\n", gam.gamma);
		return;
	}

	if(t = alpha(s, "keep")){
		if(!cull(t, 1, 0))
			return;
		goto Print;
	}

	if(t = alpha(s, "drop")){
		if(!cull(t, 0, 0))
			return;
		goto Print;
	}

	for(i=0; planet[i].name[0]; i++){
		if(t = alpha(s, planet[i].name)){
			if(doreset)
				reset();
			loadplanet(i, nil);
			goto Print;
		}
	}

	for(i=0; names[i].name; i++){
		if(t = alpha(s, names[i].name)){
			if(*t){
				fprint(2, "syntax error in type\n");
				return;
			}
			if(doreset)
				reset();
			loadtype(names[i].type);
			goto Print;
		}
	}

	switch(s[0]){
	case '"':
		starts = ++s;
		while(*s != '"')
			if(*s++ == 0){
				fprint(2, "bad star name\n");
				return;
			}
		*s = 0;
		if(doreset)
			reset();
		j = nrec;
		saoopen();
		starts = fromgreek(starts);
		for(i=0; i<NName; i++)
			if(equal(starts, name[i].name)){
				grow();
				if(name[i].sao){
					rec[j].type = NamedSAO;
					rec[j].index = name[i].sao;
				}
				if(name[i].ngc){
					rec[j].type = NamedNGC;
					rec[j].index = name[i].ngc;
				}
				if(name[i].abell){
					rec[j].type = NamedAbell;
					rec[j].index = name[i].abell;
				}
				strcpy(rec[j].u.named.name, name[i].name);
				j++;
			}
		if(parsename(starts))
			for(i=0; i<NBayer; i++)
				if(bayer[i].name[0]==parsed[0] &&
				  (bayer[i].name[1]==parsed[1] || parsed[1]==0) &&
				   bayer[i].name[2]==parsed[2]){
					grow();
					rec[j].type = NamedSAO;
					rec[j].index = bayer[i].sao;
					strncpy(rec[j].u.named.name, starts, sizeof(rec[j].u.named.name));
					j++;
				}
		if(j == 0){
			*s = '"';
			goto NotFound;
		}
		break;

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		strtoul(s, &t, 10);
		if(*t != 'h'){
	BadCoords:
			fprint(2, "bad coordinates %s\n", inputline);
			break;
		}
		ra = DEG(getra(s));
		while(*s && *s!=' ' && *s!='\t')
			s++;
		rah = ra/15;
		ra = ra-rah*15;
		ram = ra*4;
		deg = strtol(s, &t, 10);
		if(t == s)
			goto BadCoords;
		/* degree sign etc. is optional */
		chartorune(&c, t);
		if(c == 0xb0)
			deg = DEG(getra(s));
		if(doreset)
			reset();
		if(abs(deg)>=90 || rah>=24)
			goto BadCoords;
		if(!loadpatch(patch(rah, ram, deg)))
			goto NotFound;
		break;

	default:
		fprint(2, "unknown command %s\n", inputline);
		return;
	}

    Print:
	if(nrec == 0)
		Bprint(&bout, "empty\n");
	else if(nrec <= 2)
		for(i=0; i<nrec; i++)
			prrec(rec+i);
	else
		Bprint(&bout, "%ld items\n", nrec);
	return;

    NotFound:
	fprint(2, "%s not found\n", inputline);
	return;
}

char *ngctypes[] =
{
[Galaxy] 		= "Gx",
[PlanetaryN] 	= "Pl",
[OpenCl] 		= "OC",
[GlobularCl] 	= "Gb",
[DiffuseN]		= "Nb",
[NebularCl] 	= "C+N",
[Asterism]		= "Ast",
[Knot] 		= "Kt",
[Triple]		= "***",
[Double]		= "D*",
[Single]		= "*",
[Uncertain]	= "?",
[Nonexistent]	= "-",
[Unknown]	= " ",
[PlateDefect]	= "PD"
};

char*
ngcstring(int d)
{
	if(d<Galaxy || d>PlateDefect)
		return "can't happen";
	return ngctypes[d];
}

short	descindex[NINDEX];

void
printnames(Record *r)
{
	int i, ok, done;

	done = 0;
	for(i=0; i<NName; i++){	/* stupid linear search! */
		ok = 0;
		if(r->type==SAO && r->index==name[i].sao)
			ok = 1;
		if(r->type==NGC && r->u.ngc.ngc==name[i].ngc)
			ok = 1;
		if(r->type==Abell && r->u.abell.abell==name[i].abell)
			ok = 1;
		if(ok){
			if(done++ == 0)
				Bprint(&bout, "\t");
			Bprint(&bout, " \"%s\"", togreek(name[i].name));
		}
	}
	if(done)
		Bprint(&bout, "\n");
}

int
equal(char *s1, char *s2)
{
	int c;

	while(*s1){
		if(*s1==' '){
			while(*s1==' ')
				s1++;
			continue;
		}
		while(*s2==' ')
			s2++;
		c=*s2;
		if('A'<=*s2 && *s2<='Z')
			c^=' ';
		if(*s1!=c)
			return 0;
		s1++, s2++;
	}
	return 1;
}

int
parsename(char *s)
{
	char *blank;
	int i;

	blank = strchr(s, ' ');
	if(blank==0 || strchr(blank+1, ' ') || strlen(blank+1)!=3)
		return 0;
	blank++;
	parsed[0] = parsed[1] = parsed[2] = 0;
	if('0'<=s[0] && s[0]<='9'){
		i = atoi(s);
		parsed[0] = i;
		if(i > 100)
			return 0;
	}else{
		for(i=1; i<=24; i++)
			if(strncmp(greek[i], s, strlen(greek[i]))==0){
				parsed[0]=100+i;
				goto out;
			}
		return 0;
	    out:
		if('0'<=s[strlen(greek[i])] && s[strlen(greek[i])]<='9')
			parsed[1]=s[strlen(greek[i])]-'0';
	}
	for(i=1; i<=88; i++)
		if(strcmp(constel[i], blank)==0){
			parsed[2] = i;
			return 1;
		}
	return 0;
}

char*
dist_grp(int dg)
{
	switch(dg){
	default:
		return "unknown";
	case 1:
		return "13.3-14.0";
	case 2:
		return "14.1-14.8";
	case 3:
		return "14.9-15.6";
	case 4:
		return "15.7-16.4";
	case 5:
		return "16.5-17.2";
	case 6:
		return "17.3-18.0";
	case 7:
		return ">18.0";
	}
}

char*
rich_grp(int dg)
{
	switch(dg){
	default:
		return "unknown";
	case 0:
		return "30-40";
	case 1:
		return "50-79";
	case 2:
		return "80-129";
	case 3:
		return "130-199";
	case 4:
		return "200-299";
	case 5:
		return ">=300";
	}
}

char*
nameof(Record *r)
{
	NGCrec *n;
	SAOrec *s;
	Abellrec *a;
	static char buf[128];
	int i;

	switch(r->type){
	default:
		return nil;
	case SAO:
		s = &r->u.sao;
		if(s->name[0] == 0)
			return nil;
		if(s->name[0] >= 100){
			i = snprint(buf, sizeof buf, "%C", greeklet[s->name[0]-100]);
			if(s->name[1])
				i += snprint(buf+i, sizeof buf-i, "%d", s->name[1]);
		}else
			i = snprint(buf, sizeof buf, " %d", s->name[0]);
		snprint(buf+i, sizeof buf-i, " %s", constel[(uchar)s->name[2]]);
		break;
	case NGC:
		n = &r->u.ngc;
		if(n->type >= Uncertain)
			return nil;
		if(n->ngc <= NNGC)
			snprint(buf, sizeof buf, "NGC%4d ", n->ngc);
		else
			snprint(buf, sizeof buf, "IC%4d ", n->ngc-NNGC);
		break;
	case Abell:
		a = &r->u.abell;
		snprint(buf, sizeof buf, "Abell%4d", a->abell);
		break;
	}
	return buf;
}

void
prrec(Record *r)
{
	NGCrec *n;
	SAOrec *s;
	Abellrec *a;
	Planetrec *p;
	int i, rah, ram, dec, nn;
	int32 key;

	if(r) switch(r->type){
	default:
		fprint(2, "can't prrec type %d\n", r->type);
		exits("type");

	case Planet:
		p = &r->u.planet;
		Bprint(&bout, "%s", p->name);
		Bprint(&bout, "\t%s %s",
			hms(angle(p->ra)),
			dms(angle(p->dec)));
		Bprint(&bout, " %3.2f° %3.2f°",
			p->az/(double)MILLIARCSEC, p->alt/(double)MILLIARCSEC);
		Bprint(&bout, " %s",
			ms(angle(p->semidiam)));
		if(r->index <= 1)
			Bprint(&bout, " %g", p->phase);
		Bprint(&bout, "\n");
		break;

	case NGC:
		n = &r->u.ngc;
		if(n->ngc <= NNGC)
			Bprint(&bout, "NGC%4d ", n->ngc);
		else
			Bprint(&bout, "IC%4d ", n->ngc-NNGC);
		Bprint(&bout, "%s ", ngcstring(n->type));
		if(n->mag == UNKNOWNMAG)
			Bprint(&bout, "----");
		else
			Bprint(&bout, "%.1f%c", n->mag/10.0, n->magtype);
		Bprint(&bout, "\t%s %s\t%c%.1f'\n",
			hm(angle(n->ra)),
			dm(angle(n->dec)),
			n->diamlim,
			DEG(angle(n->diam))*60.);
		prdesc(n->desc, desctab, descindex);
		printnames(r);
		break;

	case Abell:
		a = &r->u.abell;
		Bprint(&bout, "Abell%4d  %.1f %.2f° %dMpc", a->abell, a->mag10/10.0,
			DEG(angle(a->rad)), a->dist);
		Bprint(&bout, "\t%s %s\t%.2f %.2f\n",
			hm(angle(a->ra)),
			dm(angle(a->dec)),
			DEG(angle(a->glat)),
			DEG(angle(a->glong)));
		Bprint(&bout, "\tdist grp: %s  rich grp: %s  %d galaxies/°²\n",
			dist_grp(a->distgrp),
			rich_grp(a->richgrp),
			a->pop);
		printnames(r);
		break;

	case SAO:
		s = &r->u.sao;
		Bprint(&bout, "SAO%6ld  ", r->index);
		if(s->mag==UNKNOWNMAG)
			Bprint(&bout, "---");
		else
			Bprint(&bout, "%.1f", s->mag/10.0);
		if(s->mpg==UNKNOWNMAG)
			Bprint(&bout, ",---");
		else
			Bprint(&bout, ",%.1f", s->mpg/10.0);
		Bprint(&bout, "  %s %s  %.4fs %.3f\"",
			hms(angle(s->ra)),
			dms(angle(s->dec)),
			DEG(angle(s->dra))*(4*60),
			DEG(angle(s->ddec))*(60*60));
		Bprint(&bout, "  %.3s %c %.2s %ld %d",
			s->spec, s->code, s->compid, s->hd, s->hdcode);
		if(s->name[0])
			Bprint(&bout, " \"%s\"", nameof(r));
		Bprint(&bout, "\n");
		printnames(r);
		break;

	case Patch:
		radec(r->index, &rah, &ram, &dec);
		Bprint(&bout, "%dh%dm %d°", rah, ram, dec);
		key = r->u.patch.key[0];
		Bprint(&bout, " %s", constel[key&0xFF]);
		if((key>>=8) & 0xFF)
			Bprint(&bout, " %s", constel[key&0xFF]);
		if((key>>=8) & 0xFF)
			Bprint(&bout, " %s", constel[key&0xFF]);
		if((key>>=8) & 0xFF)
			Bprint(&bout, " %s", constel[key&0xFF]);
		for(i=1; i<r->u.patch.nkey; i++){
			key = r->u.patch.key[i];
			switch(key&0x3F){
			case SAO:
				Bprint(&bout, " SAO%ld", (key>>8)&0xFFFFFF);
				break;
			case Abell:
				Bprint(&bout, " Abell%ld", (key>>8)&0xFFFFFF);
				break;
			default:	/* NGC */
				nn = (key>>16)&0xFFFF;
				if(nn > NNGC)
					Bprint(&bout, " IC%d", nn-NNGC);
				else
					Bprint(&bout, " NGC%d", nn);
				Bprint(&bout, "(%s)", ngcstring(key&0x3F));
				break;
			}
		}
		Bprint(&bout, "\n");
		break;

	case NGCN:
		if(r->index <= NNGC)
			Bprint(&bout, "NGC%ld\n", r->index);
		else
			Bprint(&bout, "IC%ld\n", r->index-NNGC);
		break;

	case NamedSAO:
		Bprint(&bout, "SAO%ld \"%s\"\n", r->index, togreek(r->u.named.name));
		break;

	case NamedNGC:
		if(r->index <= NNGC)
			Bprint(&bout, "NGC%ld \"%s\"\n", r->index, togreek(r->u.named.name));
		else
			Bprint(&bout, "IC%ld \"%s\"\n", r->index-NNGC, togreek(r->u.named.name));
		break;

	case NamedAbell:
		Bprint(&bout, "Abell%ld \"%s\"\n", r->index, togreek(r->u.named.name));
		break;

	case PatchC:
		radec(r->index, &rah, &ram, &dec);
		Bprint(&bout, "%dh%dm %d\n", rah, ram, dec);
		break;
	}
}
