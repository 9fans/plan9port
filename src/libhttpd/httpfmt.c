#include <u.h>
#include <libc.h>
#include <bin.h>
#include <httpd.h>

int
httpfmt(Fmt *f)
{
	char buf[HMaxWord*2];
	Rune r;
	char *t, *s;
	Htmlesc *l;

	s = va_arg(f->args, char*);
	for(t = buf; t < buf + sizeof(buf) - 8; ){
		s += chartorune(&r, s);
		if(r == 0)
			break;
		for(l = htmlesc; l->name != nil; l++)
			if(l->value == r)
				break;
		if(l->name != nil){
			strcpy(t, l->name);
			t += strlen(t);
		}else
			*t++ = r;
	}
	*t = 0;
	return fmtstrcpy(f, buf);
}
