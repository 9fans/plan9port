#include "common.h"

enum {
	Buffersize = 64*1024
};

typedef struct Inbuf Inbuf;
struct Inbuf
{
	char buf[Buffersize];
	char *wp;
	char *rp;
	int eof;
	int in;
	int out;
	int last;
	ulong bytes;
};

static Inbuf*
allocinbuf(int in, int out)
{
	Inbuf *b;

	b = mallocz(sizeof(Inbuf), 1);
	if(b == nil)
		sysfatal("reading mailbox: %r");
	b->rp = b->wp = b->buf;
	b->in = in;
	b->out = out;
	return b;
}

static int
fill(Inbuf *b, int addspace)
{
	int i, n;

	if(b->eof && b->wp - b->rp == 0)
		return 0;

	n = b->rp - b->buf;
	if(n > 0){
		i = write(b->out, b->buf, n);
		if(i != n)
			return -1;
		b->last = b->buf[n-1];
		b->bytes += n;
	}
	if(addspace){
		if(write(b->out, " ", 1) != 1)
			return -1;
		b->last = ' ';
		b->bytes++;
	}

	n = b->wp - b->rp;
	memmove(b->buf, b->rp, n);
	b->rp = b->buf;
	b->wp = b->rp + n;

	i = read(b->in, b->buf+n, sizeof(b->buf)-n);
	if(i < 0)
		return -1;
	b->wp += i;

	return b->wp - b->rp;
}

/* code to escape ' '*From' ' at the beginning of a line */
int
appendfiletombox(int in, int out)
{
	int addspace;
	int n;
	char *p;
	int sol;
	Inbuf *b;

	seek(out, 0, 2);

	b = allocinbuf(in, out);
	addspace = 0;
	sol = 1;

	for(;;){
		if(b->wp - b->rp < 5){
			n = fill(b, addspace);
			addspace = 0;
			if(n < 0)
				goto error;
			if(n == 0)
				break;
			if(n < 5){
				b->rp = b->wp;
				continue;
			}
		}

		/* state machine looking for ' '*From' ' */
		if(!sol){
			p = memchr(b->rp, '\n', b->wp - b->rp);
			if(p == nil)
				b->rp = b->wp;
			else{
				b->rp = p+1;
				sol = 1;
			}
			continue;
		} else {
			if(*b->rp == ' ' || strncmp(b->rp, "From ", 5) != 0){
				b->rp++;
				continue;
			}
			addspace = 1;
			sol = 0;
		}
	}

	/* mailbox entries always terminate with two newlines */
	n = b->last == '\n' ? 1 : 2;
	if(write(out, "\n\n", n) != n)
		goto error;
	n += b->bytes;
	free(b);
	return n;
error:
	free(b);
	return -1;
}

int
appendfiletofile(int in, int out)
{
	int n;
	Inbuf *b;

	seek(out, 0, 2);

	b = allocinbuf(in, out);
	for(;;){
		n = fill(b, 0);
		if(n < 0)
			goto error;
		if(n == 0)
			break;
		b->rp = b->wp;
	}
	n = b->bytes;
	free(b);
	return n;
error:
	free(b);
	return -1;
}
