#include <u.h>
#include <libc.h>
#include <ip.h>

char*
v4parseip(uchar *to, char *from)
{
	int i;
	char *p;

	p = from;
	for(i = 0; i < 4 && *p; i++){
		to[i] = strtoul(p, &p, 0);
		if(*p == '.')
			p++;
	}
	switch(CLASS(to)){
	case 0:	/* class A - 1 uchar net */
	case 1:
		if(i == 3){
			to[3] = to[2];
			to[2] = to[1];
			to[1] = 0;
		} else if (i == 2){
			to[3] = to[1];
			to[1] = 0;
		}
		break;
	case 2:	/* class B - 2 uchar net */
		if(i == 3){
			to[3] = to[2];
			to[2] = 0;
		}
		break;
	}
	return p;
}

ulong
parseip(uchar *to, char *from)
{
	int i, elipsis = 0, v4 = 1;
	ulong x;
	char *p, *op;

	memset(to, 0, IPaddrlen);
	p = from;
	for(i = 0; i < 16 && *p; i+=2){
		op = p;
		x = strtoul(p, &p, 16);
		if(*p == '.' || (*p == 0 && i == 0)){
			p = v4parseip(to+i, op);
			i += 4;
			break;
		}
		to[i] = x>>8;
		to[i+1] = x;
		if(*p == ':'){
			v4 = 0;
			if(*++p == ':'){
				elipsis = i+2;
				p++;
			}
		}
	}
	if(i < 16){
		memmove(&to[elipsis+16-i], &to[elipsis], i-elipsis);
		memset(&to[elipsis], 0, 16-i);
	}
	if(v4){
		to[10] = to[11] = 0xff;
		return nhgetl(to+12);
	} else
		return 6;
}

/*
 *  hack to allow ip v4 masks to be entered in the old
 *  style
 */
ulong
parseipmask(uchar *to, char *from)
{
	ulong x;
	int i;
	uchar *p;

	if(*from == '/'){
		/* as a number of prefix bits */
		i = atoi(from+1);
		if(i < 0)
			i = 0;
		if(i > 128)
			i = 128;
		memset(to, 0, IPaddrlen);
		for(p = to; i >= 8; i -= 8)
			*p++ = 0xff;
		if(i > 0)
			*p = ~((1<<(8-i))-1);
		x = nhgetl(to+IPv4off);
	} else {
		/* as a straight bit mask */
		x = parseip(to, from);
		if(memcmp(to, v4prefix, IPv4off) == 0)
			memset(to, 0xff, IPv4off);
	}
	return x;
}

/*
 *  parse a v4 ip address/mask in cidr format
 */
char*
v4parsecidr(uchar *addr, uchar *mask, char *from)
{
	int i;
	char *p;
	uchar *a;

	p = v4parseip(addr, from);

	if(*p == '/'){
		/* as a number of prefix bits */
		i = strtoul(p+1, &p, 0);
		if(i > 32)
			i = 32;
		memset(mask, 0, IPv4addrlen);
		for(a = mask; i >= 8; i -= 8)
			*a++ = 0xff;
		if(i > 0)
			*a = ~((1<<(8-i))-1);
	} else 
		memcpy(mask, defmask(addr), IPv4addrlen);
	return p;
}
