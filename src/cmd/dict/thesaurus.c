#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

void
thesprintentry(Entry e, int cmd)
{
	char *p, *pe;
	int c, i;

	p = e.start;
	pe = e.end;
	while(p < pe) {
		c = *p++;
		if(cmd == 'r') {
			outchar(c);
			continue;
		}
		switch(c) {
		case '*':
			c = *p++;
			if(cmd == 'h' && c != 'L') {
				outnl(0);
				return;
			}
			if(c == 'L' && cmd != 'h')
				outnl(0);
			if(c == 'S') {
				outchar('(');
				outchar(*p++);
				outchar(')');
			}
			break;
		case '#':
			c = *p++;
			i = *p++ - '0' - 1;
			if(i < 0 || i > 4)
				break;
			switch(c) {
			case 'a': outrune(L"áàâäa"[i]); break;
			case 'e': outrune(L"éèêëe"[i]); break;
			case 'o': outrune(L"óòôöo"[i]); break;
			case 'c': outrune(L"ccccç"[i]); break;
			default: outchar(c); break;
			}
			break;
		case '+':
		case '<':
			break;
		case ' ':
			if(cmd == 'h' && *p == '*') {
				outnl(0);
				return;
			}
		default:
			outchar(c);
		}
	}
	outnl(0);
}

long
thesnextoff(long fromoff)
{
	long a;
	char *p;

	a = Bseek(bdict, fromoff, 0);
	if(a < 0)
		return -1;
	for(;;) {
		p = Brdline(bdict, '\n');
		if(!p)
			break;
		if(p[0] == '*' && p[1] == 'L')
			return (Boffset(bdict)-Blinelen(bdict));
	}
	return -1;
}

void
thesprintkey(void)
{
	Bprint(bout, "No key\n");
}
