#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include "imagefile.h"

/*
 * Hacked version for writing from Rawimage to file.
 * Assumes 8 bits per component.
 */

#define	HSHIFT	3	/* HSHIFT==5 runs slightly faster, but hash table is 64x bigger */
#define	NHASH	(1<<(HSHIFT*NMATCH))
#define	HMASK	(NHASH-1)
#define	hupdate(h, c)	((((h)<<HSHIFT)^(c))&HMASK)
typedef struct Hlist Hlist;
struct Hlist{
	uchar *s;
	Hlist *next, *prev;
};

int
writerawimage(int fd, Rawimage *i)
{
	uchar *outbuf, *outp, *eout;		/* encoded data, pointer, end */
	uchar *loutp;				/* start of encoded line */
	Hlist *hash;				/* heads of hash chains of past strings */
	Hlist *chain, *hp;			/* hash chain members, pointer */
	Hlist *cp;				/* next Hlist to fall out of window */
	int h;					/* hash value */
	uchar *line, *eline;			/* input line, end pointer */
	uchar *data, *edata;			/* input buffer, end pointer */
	ulong n;				/* length of input buffer */
	int bpl;				/* input line length */
	int offs, runlen;			/* offset, length of consumed data */
	uchar dumpbuf[NDUMP];			/* dump accumulator */
	int ndump;				/* length of dump accumulator */
	int ncblock;				/* size of buffer */
	Rectangle r;
	uchar *p, *q, *s, *es, *t;
	char hdr[11+5*12+1], buf[16];
	ulong desc;

	r = i->r;
	switch(i->chandesc){
	default:
		werrstr("can't handle chandesc %d", i->chandesc);
		return -1;
	case CY:
		bpl = Dx(r);
		desc = GREY8;
		break;
	case CYA16:
		bpl = 2*Dx(r);
		desc = CHAN2(CGrey, 8, CAlpha, 8);
		break;
	case CRGBV:
		bpl = Dx(r);
		desc = CMAP8;
		break;
	case CRGBVA16:
		bpl = 2*Dx(r);
		desc = CHAN2(CMap, 8, CAlpha, 8);
		break;
	case CRGB24:
		bpl = 3*Dx(r);
		desc = RGB24;
		break;
	case CRGBA32:
		bpl = 4*Dx(r);
		desc = RGBA32;
		break;
	}
	ncblock = _compblocksize(r, bpl/Dx(r));
	outbuf = malloc(ncblock);
	hash = malloc(NHASH*sizeof(Hlist));
	chain = malloc(NMEM*sizeof(Hlist));
	if(outbuf == 0 || hash == 0 || chain == 0){
	ErrOut:
		free(outbuf);
		free(hash);
		free(chain);
		return -1;
	}
	n = Dy(r)*bpl;
	data = i->chans[0];
	sprint(hdr, "compressed\n%11s %11d %11d %11d %11d ",
		chantostr(buf, desc), r.min.x, r.min.y, r.max.x, r.max.y);
	if(write(fd, hdr, 11+5*12) != 11+5*12){
		werrstr("i/o error writing header");
		goto ErrOut;
	}
	edata = data+n;
	eout = outbuf+ncblock;
	line = data;
	r.max.y = r.min.y;
	while(line != edata){
		memset(hash, 0, NHASH*sizeof(Hlist));
		memset(chain, 0, NMEM*sizeof(Hlist));
		cp = chain;
		h = 0;
		outp = outbuf;
		for(n = 0; n != NMATCH; n++)
			h = hupdate(h, line[n]);
		loutp = outbuf;
		while(line != edata){
			ndump = 0;
			eline = line+bpl;
			for(p = line; p != eline; ){
				if(eline-p < NRUN)
					es = eline;
				else
					es = p+NRUN;
				q = 0;
				runlen = 0;
				for(hp = hash[h].next; hp; hp = hp->next){
					s = p + runlen;
					if(s >= es)
						continue;
					t = hp->s + runlen;
					for(; s >= p; s--)
						if(*s != *t--)
							goto matchloop;
					t += runlen+2;
					s += runlen+2;
					for(; s < es; s++)
						if(*s != *t++)
							break;
					n = s-p;
					if(n > runlen){
						runlen = n;
						q = hp->s;
						if(n == NRUN)
							break;
					}
			matchloop: ;
				}
				if(runlen < NMATCH){
					if(ndump == NDUMP){
						if(eout-outp < ndump+1)
							goto Bfull;
						*outp++ = ndump-1+128;
						memmove(outp, dumpbuf, ndump);
						outp += ndump;
						ndump = 0;
					}
					dumpbuf[ndump++] = *p;
					runlen = 1;
				}
				else{
					if(ndump != 0){
						if(eout-outp < ndump+1)
							goto Bfull;
						*outp++ = ndump-1+128;
						memmove(outp, dumpbuf, ndump);
						outp += ndump;
						ndump = 0;
					}
					offs = p-q-1;
					if(eout-outp < 2)
						goto Bfull;
					*outp++ = ((runlen-NMATCH)<<2) + (offs>>8);
					*outp++ = offs&255;
				}
				for(q = p+runlen; p != q; p++){
					if(cp->prev)
						cp->prev->next = 0;
					cp->next = hash[h].next;
					cp->prev = &hash[h];
					if(cp->next)
						cp->next->prev = cp;
					cp->prev->next = cp;
					cp->s = p;
					if(++cp == &chain[NMEM])
						cp = chain;
					if(edata-p > NMATCH)
						h = hupdate(h, p[NMATCH]);
				}
			}
			if(ndump != 0){
				if(eout-outp < ndump+1)
					goto Bfull;
				*outp++ = ndump-1+128;
				memmove(outp, dumpbuf, ndump);
				outp += ndump;
			}
			line = eline;
			loutp = outp;
			r.max.y++;
		}
	Bfull:
		if(loutp == outbuf){
			werrstr("compressor out of sync");
			goto ErrOut;
		}
		n = loutp-outbuf;
		sprint(hdr, "%11d %11ld ", r.max.y, n);
		write(fd, hdr, 2*12);
		write(fd, outbuf, n);
		r.min.y = r.max.y;
	}
	free(outbuf);
	free(hash);
	free(chain);
	return 0;
}
