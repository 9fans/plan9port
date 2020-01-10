#include <u.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <fcall.h>
#include <libsec.h>
#include "dat.h"
#include "protos.h"
#include "y.tab.h"

int Cflag;
int pflag;
int Nflag;
int sflag;
int tiflag;
int toflag;

char *prom = "promiscuous";

enum
{
	Pktlen=	64*1024,
	Blen=	16*1024
};

Filter *filter;
Proto *root;
Biobuf out;
vlong starttime, pkttime;
int pcap;

int	filterpkt(Filter *f, uchar *ps, uchar *pe, Proto *pr, int);
void	printpkt(char *p, char *e, uchar *ps, uchar *pe);
void	mkprotograph(void);
Proto*	findproto(char *name);
Filter*	compile(Filter *f);
void	printfilter(Filter *f, char *tag);
void	printhelp(char*);
void	tracepkt(uchar*, int);
void	pcaphdr(int);

struct pcap_pkthdr {
        u64int	ts;	/* time stamp */
        u32int	caplen;	/* length of portion present */
        u32int	len;	/* length this packet (off wire) */
};


void
printusage(void)
{
	fprint(2, "usage: %s [-CDdpst] [-N n] [-f filter] [-h first-header] path\n", argv0);
	fprint(2, "  for protocol help: %s -? [proto]\n", argv0);
}

void
usage(void)
{
	printusage();
	exits("usage");
}

void
main(int argc, char **argv)
{
	uchar *pkt;
	char *buf, *file, *p, *e;
	int fd;
	int n;

	Binit(&out, 1, OWRITE);

	fmtinstall('E', eipfmt);
	fmtinstall('V', eipfmt);
	fmtinstall('I', eipfmt);
	fmtinstall('H', encodefmt);
	fmtinstall('F', fcallfmt);

	pkt = malloc(Pktlen+16);
	pkt += 16;
	buf = malloc(Blen);
	e = buf+Blen-1;

	pflag = 1;
	Nflag = 32;
	sflag = 0;

	mkprotograph();

	ARGBEGIN{
	default:
		usage();
	case '?':
		printusage();
		printhelp(ARGF());
		exits(0);
		break;
	case 'N':
		p = EARGF(usage());
		Nflag = atoi(p);
		break;
	case 'f':
		p = EARGF(usage());
		yyinit(p);
		yyparse();
		break;
	case 's':
		sflag = 1;
		break;
	case 'h':
		p = EARGF(usage());
		root = findproto(p);
		if(root == nil)
			sysfatal("unknown protocol: %s", p);
		break;
	case 'd':
		toflag = 1;
		break;
	case 'D':
		toflag = 1;
		pcap = 1;
		break;
	case 't':
		tiflag = 1;
		break;
	case 'T':
		tiflag = 1;
		pcap = 1;
		break;
	case 'C':
		Cflag = 1;
		break;
	case 'p':
		pflag = 0;
		break;
	}ARGEND;

	if(argc > 1)
		usage();

	if(argc == 0)
		file = nil;
	else
		file = argv[0];

	if(tiflag){
		if(file == nil)
			sysfatal("must specify file with -t");
		fd = open(file, OREAD);
		if(fd < 0)
			sysfatal("opening %s: %r", file);
	}else{
		fd = opendevice(file, pflag);
		if(fd < 0)
			sysfatal("opening device %s: %r", file);
	}
	if(root == nil)
		root = &ether;

	if(pcap)
		pcaphdr(fd);

	filter = compile(filter);

	if(tiflag){
		/* read a trace file */
		for(;;){
			if(pcap){
				struct pcap_pkthdr *goo;
				n = read(fd, pkt, 16);
				if(n != 16)
					break;
				goo = (struct pcap_pkthdr*)pkt;
				pkttime = goo->ts;
				n = goo->caplen;
			}else{
				n = read(fd, pkt, 10);
				if(n != 10)
					break;
				pkttime = NetL(pkt+2);
				pkttime = (pkttime<<32) | NetL(pkt+6);
				if(starttime == 0LL)
					starttime = pkttime;
				n = NetS(pkt);
			}
			if(readn(fd, pkt, n) != n)
				break;
			if(filterpkt(filter, pkt, pkt+n, root, 1))
				if(toflag)
					tracepkt(pkt, n);
				else
					printpkt(buf, e, pkt, pkt+n);
		}
	} else {
		/* read a real time stream */
		starttime = nsec();
		for(;;){
			n = root->framer(fd, pkt, Pktlen);
			if(n <= 0)
				break;
			pkttime = nsec();
			if(filterpkt(filter, pkt, pkt+n, root, 1))
				if(toflag)
					tracepkt(pkt, n);
				else
					printpkt(buf, e, pkt, pkt+n);
		}
	}
}

/* create a new filter node */
Filter*
newfilter(void)
{
	Filter *f;

	f = mallocz(sizeof(*f), 1);
	if(f == nil)
		sysfatal("newfilter: %r");
	return f;
}

/*
 *  apply filter to packet
 */
int
_filterpkt(Filter *f, Msg *m)
{
	Msg ma;

	if(f == nil)
		return 1;

	switch(f->op){
	case '!':
		return !_filterpkt(f->l, m);
	case LAND:
		ma = *m;
		return _filterpkt(f->l, &ma) && _filterpkt(f->r, m);
	case LOR:
		ma = *m;
		return _filterpkt(f->l, &ma) || _filterpkt(f->r, m);
	case WORD:
		if(m->needroot){
			if(m->pr != f->pr)
				return 0;
			m->needroot = 0;
		}else{
			if(m->pr && (m->pr->filter==nil || !(m->pr->filter)(f, m)))
				return 0;
		}
		if(f->l == nil)
			return 1;
		m->pr = f->pr;
		return _filterpkt(f->l, m);
	}
	sysfatal("internal error: filterpkt op: %d", f->op);
	return 0;
}
int
filterpkt(Filter *f, uchar *ps, uchar *pe, Proto *pr, int needroot)
{
	Msg m;

	if(f == nil)
		return 1;

	m.needroot = needroot;
	m.ps = ps;
	m.pe = pe;
	m.pr = pr;
	return _filterpkt(f, &m);
}

/*
 *  from the Unix world
 */
#define PCAP_VERSION_MAJOR 2
#define PCAP_VERSION_MINOR 4
#define TCPDUMP_MAGIC 0xa1b2c3d4

struct pcap_file_header {
	u32int		magic;
	u16int		version_major;
	u16int		version_minor;
	s32int		thiszone;    /* gmt to local correction */
	u32int		sigfigs;    /* accuracy of timestamps */
	u32int		snaplen;    /* max length saved portion of each pkt */
	u32int		linktype;   /* data link type (DLT_*) */
};

/*
 *  pcap trace header
 */
void
pcaphdr(int fd)
{
	if(tiflag){
		struct pcap_file_header hdr;

		if(readn(fd, &hdr, sizeof hdr) != sizeof hdr)
			sysfatal("short header");
		if(hdr.magic != TCPDUMP_MAGIC)
			sysfatal("packet header %ux != %ux", hdr.magic, TCPDUMP_MAGIC);
		if(hdr.version_major != PCAP_VERSION_MAJOR || hdr.version_minor != PCAP_VERSION_MINOR)
			sysfatal("version %d.%d != %d.%d", hdr.version_major, hdr.version_minor, PCAP_VERSION_MAJOR, PCAP_VERSION_MINOR);
		if(hdr.linktype != 1)
			sysfatal("unknown linktype %d != 1 (ethernet)", hdr.linktype);
	}
	if(toflag){
		struct pcap_file_header hdr;

		hdr.magic = TCPDUMP_MAGIC;
		hdr.version_major = PCAP_VERSION_MAJOR;
		hdr.version_minor = PCAP_VERSION_MINOR;

		hdr.thiszone = 0;
		hdr.snaplen = 1500;
		hdr.sigfigs = 0;
		hdr.linktype = 1;

		write(1, &hdr, sizeof(hdr));
	}
}

/*
 *  write out a packet trace
 */
void
tracepkt(uchar *ps, int len)
{
	struct pcap_pkthdr *goo;

	if(pcap){
		goo = (struct pcap_pkthdr*)(ps-16);
		goo->ts = pkttime;
		goo->caplen = len;
		goo->len = len;
		write(1, goo, len+16);
	} else {
		hnputs(ps-10, len);
		hnputl(ps-8, pkttime>>32);
		hnputl(ps-4, pkttime);
		write(1, ps-10, len+10);
	}
}

/*
 *  format and print a packet
 */
void
printpkt(char *p, char *e, uchar *ps, uchar *pe)
{
	Msg m;
	ulong dt;

	dt = (pkttime-starttime)/1000000LL;
	m.p = seprint(p, e, "%6.6uld ms ", dt);
	m.ps = ps;
	m.pe = pe;
	m.e = e;
	m.pr = root;
	while(m.p < m.e){
		if(!sflag)
			m.p = seprint(m.p, m.e, "\n\t");
		m.p = seprint(m.p, m.e, "%s(", m.pr->name);
		if((*m.pr->seprint)(&m) < 0){
			m.p = seprint(m.p, m.e, "TOO SHORT");
			m.ps = m.pe;
		}
		m.p = seprint(m.p, m.e, ")");
		if(m.pr == nil || m.ps >= m.pe)
			break;
	}
	*m.p++ = '\n';

	if(write(1, p, m.p - p) < 0)
		sysfatal("stdout: %r");
}

Proto **xprotos;
int nprotos;

/* look up a protocol by its name */
Proto*
findproto(char *name)
{
	int i;

	for(i = 0; i < nprotos; i++)
		if(strcmp(xprotos[i]->name, name) == 0)
			return xprotos[i];
	return nil;
}

/*
 *  add an undefined protocol to protos[]
 */
Proto*
addproto(char *name)
{
	Proto *pr;

	xprotos = realloc(xprotos, (nprotos+1)*sizeof(Proto*));
	pr = malloc(sizeof *pr);
	*pr = dump;
	pr->name = name;
	xprotos[nprotos++] = pr;
	return pr;
}

/*
 *  build a graph of protocols, this could easily be circular.  This
 *  links together all the multiplexing in the protocol modules.
 */
void
mkprotograph(void)
{
	Proto **l;
	Proto *pr;
	Mux *m;

	/* copy protos into a reallocable area */
	for(nprotos = 0; protos[nprotos] != nil; nprotos++)
		;
	xprotos = malloc(nprotos*sizeof(Proto*));
	memmove(xprotos, protos, nprotos*sizeof(Proto*));

	for(l = protos; *l != nil; l++){
		pr = *l;
		for(m = pr->mux; m != nil && m->name != nil; m++){
			m->pr = findproto(m->name);
			if(m->pr == nil)
				m->pr = addproto(m->name);
		}
	}
}

/*
 *  add in a protocol node
 */
static Filter*
addnode(Filter *f, Proto *pr)
{
	Filter *nf;
	nf = newfilter();
	nf->pr = pr;
	nf->s = pr->name;
	nf->l = f;
	nf->op = WORD;
	return nf;
}

/*
 *  recurse through the protocol graph adding missing nodes
 *  to the filter if we reach the filter's protocol
 */
static Filter*
_fillin(Filter *f, Proto *last, int depth)
{
	Mux *m;
	Filter *nf;

	if(depth-- <= 0)
		return nil;

	for(m = last->mux; m != nil && m->name != nil; m++){
		if(m->pr == nil)
			continue;
		if(f->pr == m->pr)
			return f;
		nf = _fillin(f, m->pr, depth);
		if(nf != nil)
			return addnode(nf, m->pr);
	}
	return nil;
}

static Filter*
fillin(Filter *f, Proto *last)
{
	int i;
	Filter *nf;

	/* hack to make sure top level node is the root */
	if(last == nil){
		if(f->pr == root)
			return f;
		f = fillin(f, root);
		if(f == nil)
			return nil;
		return addnode(f, root);
	}

	/* breadth first search though the protocol graph */
	nf = f;
	for(i = 1; i < 20; i++){
		nf = _fillin(f, last, i);
		if(nf != nil)
			break;
	}
	return nf;
}

/*
 *  massage tree so that all paths from the root to a leaf
 *  contain a filter node for each header.
 *
 *  also, set f->pr where possible
 */
Filter*
complete(Filter *f, Proto *last)
{
	Proto *pr;

	if(f == nil)
		return f;

	/* do a depth first traversal of the filter tree */
	switch(f->op){
	case '!':
		f->l = complete(f->l, last);
		break;
	case LAND:
	case LOR:
		f->l = complete(f->l, last);
		f->r = complete(f->r, last);
		break;
	case '=':
		break;
	case WORD:
		pr = findproto(f->s);
		f->pr = pr;
		if(pr == nil){
			if(f->l != nil){
				fprint(2, "%s unknown proto, ignoring params\n",
					f->s);
				f->l = nil;
			}
		} else {
			f->l = complete(f->l, pr);
			f = fillin(f, last);
			if(f == nil)
				sysfatal("internal error: can't get to %s", pr->name);
		}
		break;
	}
	return f;
}

/*
 *  merge common nodes under | and & moving the merged node
 *  above the | or &.
 *
 *  do some constant foldong, e.g. `true & x' becomes x and
 *  'true | x' becomes true.
 */
static int changed;

static Filter*
_optimize(Filter *f)
{
	Filter *l;

	if(f == nil)
		return f;

	switch(f->op){
	case '!':
		/* is child also a not */
		if(f->l->op == '!'){
			changed = 1;
			return f->l->l;
		}
		break;
	case LOR:
		/* are two children the same protocol? */
		if(f->l->op != f->r->op || f->r->op != WORD
		|| f->l->pr != f->r->pr || f->l->pr == nil)
			break;	/* no optimization */

		changed = 1;

		/* constant folding */
		/* if either child is childless, just return that */
		if(f->l->l == nil)
			return f->l;
		else if(f->r->l == nil)
			return f->r;

		/* move the common node up, thow away one node */
		l = f->l;
		f->l = l->l;
		f->r = f->r->l;
		l->l = f;
		return l;
	case LAND:
		/* are two children the same protocol? */
		if(f->l->op != f->r->op || f->r->op != WORD
		|| f->l->pr != f->r->pr || f->l->pr == nil)
			break;	/* no optimization */

		changed = 1;

		/* constant folding */
		/* if either child is childless, ignore it */
		if(f->l->l == nil)
			return f->r;
		else if(f->r->l == nil)
			return f->l;

		/* move the common node up, thow away one node */
		l = f->l;
		f->l = _optimize(l->l);
		f->r = _optimize(f->r->l);
		l->l = f;
		return l;
	}
	f->l = _optimize(f->l);
	f->r = _optimize(f->r);
	return f;
}

Filter*
optimize(Filter *f)
{
	do{
		changed = 0;
		f = _optimize(f);
	}while(changed);

	return f;
}

/*
 *  find any top level nodes that aren't the root
 */
int
findbogus(Filter *f)
{
	int rv;

	if(f->op != WORD){
		rv = findbogus(f->l);
		if(f->r)
			rv |= findbogus(f->r);
		return rv;
	} else if(f->pr != root){
		fprint(2, "bad top-level protocol: %s\n", f->s);
		return 1;
	}
	return 0;
}

/*
 *  compile the filter
 */
static void
_compile(Filter *f, Proto *last)
{
	if(f == nil)
		return;

	switch(f->op){
	case '!':
		_compile(f->l, last);
		break;
	case LOR:
	case LAND:
		_compile(f->l, last);
		_compile(f->r, last);
		break;
	case WORD:
		if(last != nil){
			if(last->compile == nil)
				sysfatal("unknown %s subprotocol: %s", f->pr->name, f->s);
			(*last->compile)(f);
		}
		if(f->l)
			_compile(f->l, f->pr);
		break;
	case '=':
		if(last == nil)
			sysfatal("internal error: compilewalk: badly formed tree");

		if(last->compile == nil)
			sysfatal("unknown %s field: %s", f->pr->name, f->s);
		(*last->compile)(f);
		break;
	default:
		sysfatal("internal error: compilewalk op: %d", f->op);
	}
}

Filter*
compile(Filter *f)
{
	if(f == nil)
		return f;

	/* fill in the missing header filters */
	f = complete(f, nil);

	/* constant folding */
	f = optimize(f);
	if(!toflag)
		printfilter(f, "after optimize");

	/* protocol specific compilations */
	_compile(f, nil);

	/* at this point, the root had better be the root proto */
	if(findbogus(f)){
		fprint(2, "bogus filter\n");
		exits("bad filter");
	}

	return f;
}

/*
 *  parse a byte array
 */
int
parseba(uchar *to, char *from)
{
	char nip[4];
	char *p;
	int i;

	p = from;
	for(i = 0; i < 16; i++){
		if(*p == 0)
			return -1;
		nip[0] = *p++;
		if(*p == 0)
			return -1;
		nip[1] = *p++;
		nip[2] = 0;
		to[i] = strtoul(nip, 0, 16);
	}
	return i;
}

/*
 *  compile WORD = WORD, becomes a single node with a subop
 */
void
compile_cmp(char *proto, Filter *f, Field *fld)
{
	uchar x[IPaddrlen];

	if(f->op != '=')
		sysfatal("internal error: compile_cmp %s: not a cmp", proto);

	for(; fld->name != nil; fld++){
		if(strcmp(f->l->s, fld->name) == 0){
			f->op = WORD;
			f->subop = fld->subop;
			switch(fld->ftype){
			case Fnum:
				f->ulv = atoi(f->r->s);
				break;
			case Fether:
				parseether(f->a, f->r->s);
				break;
			case Fv4ip:
				f->ulv = parseip(x, f->r->s);
				break;
			case Fv6ip:
				parseip(f->a, f->r->s);
				break;
			case Fba:
				parseba(f->a, f->r->s);
				break;
			default:
				sysfatal("internal error: compile_cmp %s: %d",
					proto, fld->ftype);
			}
			f->l = f->r = nil;
			return;
		}
	}
	sysfatal("unknown %s field in: %s = %s", proto, f->l->s, f->r->s);
}

void
_pf(Filter *f)
{
	char *s;

	if(f == nil)
		return;

	s = nil;
	switch(f->op){
	case '!':
		fprint(2, "!");
		_pf(f->l);
		break;
	case WORD:
		fprint(2, "%s", f->s);
		if(f->l != nil){
			fprint(2, "(");
			_pf(f->l);
			fprint(2, ")");
		}
		break;
	case LAND:
		s = "&&";
		goto print;
	case LOR:
		s = "||";
		goto print;
	case '=':
	print:
		_pf(f->l);
		if(s)
			fprint(2, " %s ", s);
		else
			fprint(2, " %c ", f->op);
		_pf(f->r);
		break;
	default:
		fprint(2, "???");
		break;
	}
}

void
printfilter(Filter *f, char *tag)
{
	fprint(2, "%s: ", tag);
	_pf(f);
	fprint(2, "\n");
}

void
cat(void)
{
	char buf[1024];
	int n;

	while((n = read(0, buf, sizeof buf)) > 0)
		write(1, buf, n);
}

static int fd1 = -1;
void
startmc(void)
{
	int p[2];

	if(fd1 == -1)
		fd1 = dup(1, -1);

	if(pipe(p) < 0)
		return;
	switch(fork()){
	case -1:
		return;
	default:
		close(p[0]);
		dup(p[1], 1);
		if(p[1] != 1)
			close(p[1]);
		return;
	case 0:
		close(p[1]);
		dup(p[0], 0);
		if(p[0] != 0)
			close(p[0]);
		execl("/bin/mc", "mc", nil);
		cat();
		_exits(0);
	}
}

void
stopmc(void)
{
	close(1);
	dup(fd1, 1);
	waitpid();
}

void
printhelp(char *name)
{
	int len;
	Proto *pr, **l;
	Mux *m;
	Field *f;
	char fmt[40];

	if(name == nil){
		print("protocols:\n");
		startmc();
		for(l=protos; (pr=*l) != nil; l++)
			print("  %s\n", pr->name);
		stopmc();
		return;
	}

	pr = findproto(name);
	if(pr == nil){
		print("unknown protocol %s\n", name);
		return;
	}

	if(pr->field){
		print("%s's filter attributes:\n", pr->name);
		len = 0;
		for(f=pr->field; f->name; f++)
			if(len < strlen(f->name))
				len = strlen(f->name);
		startmc();
		for(f=pr->field; f->name; f++)
			print("  %-*s - %s\n", len, f->name, f->help);
		stopmc();
	}
	if(pr->mux){
		print("%s's subprotos:\n", pr->name);
		startmc();
		snprint(fmt, sizeof fmt, "  %s %%s\n", pr->valfmt);
		for(m=pr->mux; m->name != nil; m++)
			print(fmt, m->val, m->name);
		stopmc();
	}
}

/*
 *  demultiplex to next prototol header
 */
void
demux(Mux *mx, ulong val1, ulong val2, Msg *m, Proto *def)
{
	m->pr = def;
	for(mx = mx; mx->name != nil; mx++){
		if(val1 == mx->val || val2 == mx->val){
			m->pr = mx->pr;
			break;
		}
	}
}

/*
 *  default framer just assumes the input packet is
 *  a single read
 */
int
defaultframer(int fd, uchar *pkt, int pktlen)
{
	return read(fd, pkt, pktlen);
}
