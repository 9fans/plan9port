/*
 * For decoding the files that get passed to validateattachment.
 * NOT a general mime decoder.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { None, Base64, Quoted };
static int decquoted(char *out, char *in, char *e);

void
main(void)
{
	Biobuf b, b1;
	char *p, *encoding;
	int e, len;

	Binit(&b, 0, OREAD);
	Binit(&b1, 1, OWRITE);

	/* header */
	encoding = nil;
	while((p = Brdstr(&b, '\n', 1)) != nil){
		if(p[0] == 0)
			break;
		if(cistrncmp(p, "Content-Transfer-Encoding: ", 27) == 0)
			encoding = strdup(p+27);
		free(p);
	}

	e = None;
	if(encoding == nil)
		e = None;
	else if(strcmp(encoding, "base64") == 0)
		e = Base64;
	else if(strcmp(encoding, "quoted-printable") == 0)
		e = Quoted;

	while((p = Brdstr(&b, '\n', 0)) != nil){
		if(strncmp(p, "--", 2) == 0 && e != None)
			break;
		len = strlen(p);
		switch(e){
		case None:
			break;
		case Base64:
			len = dec64((uchar*)p, len, p, len);
			break;
		case Quoted:
			len = decquoted(p, p, p+len);
			break;
		}
		Bwrite(&b1, p, len);
		free(p);
	}
	exits(0);
}

/*
 *  decode quoted
 */
enum
{
	Self=	1,
	Hex=	2
};
uchar	tableqp[256];

static void
initquoted(void)
{
	int c;

	memset(tableqp, 0, 256);
	for(c = ' '; c <= '<'; c++)
		tableqp[c] = Self;
	for(c = '>'; c <= '~'; c++)
		tableqp[c] = Self;
	tableqp['\t'] = Self;
	tableqp['='] = Hex;
}

static int
hex2int(int x)
{
	if(x >= '0' && x <= '9')
		return x - '0';
	if(x >= 'A' && x <= 'F')
		return (x - 'A') + 10;
	if(x >= 'a' && x <= 'f')
		return (x - 'a') + 10;
	return 0;
}

static char*
decquotedline(char *out, char *in, char *e)
{
	int c, soft;

	/* dump trailing white space */
	while(e >= in && (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n'))
		e--;

	/* trailing '=' means no newline */
	if(*e == '='){
		soft = 1;
		e--;
	} else
		soft = 0;

	while(in <= e){
		c = (*in++) & 0xff;
		switch(tableqp[c]){
		case Self:
			*out++ = c;
			break;
		case Hex:
			c = hex2int(*in++)<<4;
			c |= hex2int(*in++);
			*out++ = c;
			break;
		}
	}
	if(!soft)
		*out++ = '\n';
	*out = 0;

	return out;
}

static int
decquoted(char *out, char *in, char *e)
{
	char *p, *nl;

	if(tableqp[' '] == 0)
		initquoted();

	p = out;
	while((nl = strchr(in, '\n')) != nil && nl < e){
		p = decquotedline(p, in, nl);
		in = nl + 1;
	}
	if(in < e)
		p = decquotedline(p, in, e-1);

	/* make sure we end with a new line */
	if(*(p-1) != '\n'){
		*p++ = '\n';
		*p = 0;
	}

	return p - out;
}
