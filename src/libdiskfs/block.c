#include <u.h>
#include <libc.h>
#include <bio.h>
#include <diskfs.h>

void
blockdump(Block *bb, char *desc)
{
	uchar *p, *ep;
	int i;
	Biobuf b;

	Binit(&b, 2, OWRITE);

	Bprint(&b, "%s\n", desc);

	p = bb->data;
	ep = bb->data + bb->len;

	while(p < ep){
		for(i=0; i<16; i++){
			if(p+i < ep)
				Bprint(&b, "%.2ux ", p[i]);
			else
				Bprint(&b, "   ");
			if(i==7)
				Bprint(&b, "- ");
		}
		Bprint(&b, " ");
		for(i=0; i<16; i++){
			if(p+i < ep)
				Bprint(&b, "%c", p[i] >= 0x20 && p[i] <= 0x7F ? p[i] : '.');
			else
				Bprint(&b, " ");
		}
		p += 16;
		Bprint(&b, "\n");
	}
	Bterm(&b);
}

void
blockput(Block *b)
{
	if(b == nil)
		return;
	if(!b->_close){
		fprint(2, "no blockPut\n");
		abort();
	}
	(*b->_close)(b);
}
