#include "a.h"

static Json *parsevalue(char**);

static char*
wskip(char *p)
{
	while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\v')
		p++;
	return p;
}

static int
ishex(int c)
{
	return '0' <= c && c <= '9' ||
		'a' <= c && c <= 'f' ||
		'A' <= c && c <= 'F';
}

static Json*
newjval(int type)
{
	Json *v;

	v = emalloc(sizeof *v);
	v->ref = 1;
	v->type = type;
	return v;
}

static Json*
badjval(char **pp, char *fmt, ...)
{
	char buf[ERRMAX];
	va_list arg;

	if(fmt){
		va_start(arg, fmt);
		vsnprint(buf, sizeof buf, fmt, arg);
		va_end(arg);
		errstr(buf, sizeof buf);
	}
	*pp = nil;
	return nil;
}

static char*
_parsestring(char **pp, int *len)
{
	char *p, *q, *w, *s, *r;
	char buf[5];
	Rune rune;

	p = wskip(*pp);
	if(*p != '"'){
		badjval(pp, "missing opening quote for string");
		return nil;
	}
	for(q=p+1; *q && *q != '\"'; q++){
		if(*q == '\\' && *(q+1) != 0)
			q++;
		if((*q & 0xFF) < 0x20){	// no control chars
			badjval(pp, "control char in string");
			return nil;
		}
	}
	if(*q == 0){
		badjval(pp, "no closing quote in string");
		return nil;
	}
	s = emalloc(q - p);
	w = s;
	for(r=p+1; r<q; ){
		if(*r != '\\'){
			*w++ = *r++;
			continue;
		}
		r++;
		switch(*r){
		default:
			free(s);
			badjval(pp, "bad escape \\%c in string", *r&0xFF);
			return nil;
		case '\\':
		case '\"':
		case '/':
			*w++ = *r++;
			break;
		case 'b':
			*w++ = '\b';
			r++;
			break;
		case 'f':
			*w++ = '\f';
			r++;
			break;
		case 'n':
			*w++ = '\n';
			r++;
			break;
		case 'r':
			*w++ = '\r';
			r++;
			break;
		case 't':
			*w++ = '\t';
			r++;
			break;
		case 'u':
			r++;
			if(!ishex(r[0]) || !ishex(r[1]) || !ishex(r[2]) || !ishex(r[3])){
				free(s);
				badjval(pp, "bad hex \\u%.4s", r);
				return nil;
			}
			memmove(buf, r, 4);
			buf[4] = 0;
			rune = strtol(buf, 0, 16);
			if(rune == 0){
				free(s);
				badjval(pp, "\\u0000 in string");
				return nil;
			}
			r += 4;
			w += runetochar(w, &rune);
			break;
		}
	}
	*w = 0;
	if(len)
		*len = w - s;
	*pp = q+1;
	return s;
}

static Json*
parsenumber(char **pp)
{
	char *p, *q;
	char *t;
	double d;
	Json *v;

	/* -?(0|[1-9][0-9]*)(\.(0|[1-9][0-9]*))?([Ee][-+]?[0-9]+) */
	p = wskip(*pp);
	q = p;
	if(*q == '-')
		q++;
	if(*q == '0')
		q++;
	else{
		if(*q < '1' || *q > '9')
			return badjval(pp, "invalid number");
		while('0' <= *q && *q <= '9')
			q++;
	}
	if(*q == '.'){
		q++;
		if(*q < '0' || *q > '9')
			return badjval(pp, "invalid number");
		while('0' <= *q && *q <= '9')
			q++;
	}
	if(*q == 'e' || *q == 'E'){
		q++;
		if(*q == '-' || *q == '+')
			q++;
		if(*q < '0' || *q > '9')
			return badjval(pp, "invalid number");
		while('0' <= *q && *q <= '9')
			q++;
	}

	t = emalloc(q-p+1);
	memmove(t, p, q-p);
	t[q-p] = 0;
	errno = 0;
	d = strtod(t, nil);
	if(errno != 0){
		free(t);
		return badjval(pp, nil);
	}
	free(t);
	v = newjval(Jnumber);
	v->number = d;
	*pp = q;
	return v;
}

static Json*
parsestring(char **pp)
{
	char *s;
	Json *v;
	int len;

	s = _parsestring(pp, &len);
	if(s == nil)
		return nil;
	v = newjval(Jstring);
	v->string = s;
	v->len = len;
	return v;
}

static Json*
parsename(char **pp)
{
	if(strncmp(*pp, "true", 4) == 0){
		*pp += 4;
		return newjval(Jtrue);
	}
	if(strncmp(*pp, "false", 5) == 0){
		*pp += 5;
		return newjval(Jfalse);
	}
	if(strncmp(*pp, "null", 4) == 0){
		*pp += 4;
		return newjval(Jtrue);
	}
	return badjval(pp, "invalid name");
}

static Json*
parsearray(char **pp)
{
	char *p;
	Json *v;

	p = *pp;
	if(*p++ != '[')
		return badjval(pp, "missing bracket for array");
	v = newjval(Jarray);
	p = wskip(p);
	if(*p != ']'){
		for(;;){
			if(v->len%32 == 0)
				v->value = erealloc(v->value, (v->len+32)*sizeof v->value[0]);
			if((v->value[v->len++] = parsevalue(&p)) == nil){
				jclose(v);
				return badjval(pp, nil);
			}
			p = wskip(p);
			if(*p == ']')
				break;
			if(*p++ != ','){
				jclose(v);
				return badjval(pp, "missing comma in array");
			}
		}
	}
	p++;
	*pp = p;
	return v;
}

static Json*
parseobject(char **pp)
{
	char *p;
	Json *v;

	p = *pp;
	if(*p++ != '{')
		return badjval(pp, "missing brace for object");
	v = newjval(Jobject);
	p = wskip(p);
	if(*p != '}'){
		for(;;){
			if(v->len%32 == 0){
				v->name = erealloc(v->name, (v->len+32)*sizeof v->name[0]);
				v->value = erealloc(v->value, (v->len+32)*sizeof v->value[0]);
			}
			if((v->name[v->len++] = _parsestring(&p, nil)) == nil){
				jclose(v);
				return badjval(pp, nil);
			}
			p = wskip(p);
			if(*p++ != ':'){
				jclose(v);
				return badjval(pp, "missing colon in object");
			}
			if((v->value[v->len-1] = parsevalue(&p)) == nil){
				jclose(v);
				return badjval(pp, nil);
			}
			p = wskip(p);
			if(*p == '}')
				break;
			if(*p++ != ','){
				jclose(v);
				return badjval(pp, "missing comma in object");
			}
		}
	}
	p++;
	*pp = p;
	return v;
}

static Json*
parsevalue(char **pp)
{
	*pp = wskip(*pp);
	switch(**pp){
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '-':
		return parsenumber(pp);
	case 't':
	case 'f':
	case 'n':
		return parsename(pp);
	case '\"':
		return parsestring(pp);
	case '[':
		return parsearray(pp);
	case '{':
		return parseobject(pp);
	default:
		return badjval(pp, "unexpected char <%02x>", **pp & 0xFF);
	}
}

Json*
parsejson(char *text)
{
	Json *v;

	v = parsevalue(&text);
	if(v && text && *wskip(text) != 0){
		jclose(v);
		werrstr("extra data in json");
		return nil;
	}
	return v;
}

void
_printjval(Fmt *fmt, Json *v, int n)
{
	int i;

	if(v == nil){
		fmtprint(fmt, "nil");
		return;
	}
	switch(v->type){
	case Jstring:
		fmtprint(fmt, "\"%s\"", v->string);
		break;
	case Jnumber:
		if(floor(v->number) == v->number)
			fmtprint(fmt, "%.0f", v->number);
		else
			fmtprint(fmt, "%g", v->number);
		break;
	case Jobject:
		fmtprint(fmt, "{");
		if(n >= 0)
			n++;
		for(i=0; i<v->len; i++){
			if(n > 0)
				fmtprint(fmt, "\n%*s", n*4, "");
			fmtprint(fmt, "\"%s\" : ", v->name[i]);
			_printjval(fmt, v->value[i], n);
			fmtprint(fmt, ",");
		}
		if(n > 0){
			n--;
			if(v->len > 0)
				fmtprint(fmt, "\n%*s", n*4);
		}
		fmtprint(fmt, "}");
		break;
	case Jarray:
		fmtprint(fmt, "[");
		if(n >= 0)
			n++;
		for(i=0; i<v->len; i++){
			if(n > 0)
				fmtprint(fmt, "\n%*s", n*4, "");
			_printjval(fmt, v->value[i], n);
			fmtprint(fmt, ",");
		}
		if(n > 0){
			n--;
			if(v->len > 0)
				fmtprint(fmt, "\n%*s", n*4);
		}
		fmtprint(fmt, "]");
		break;
	case Jtrue:
		fmtprint(fmt, "true");
		break;
	case Jfalse:
		fmtprint(fmt, "false");
		break;
	case Jnull:
		fmtprint(fmt, "null");
		break;
	}
}

/*
void
printjval(Json *v)
{
	Fmt fmt;
	char buf[256];

	fmtfdinit(&fmt, 1, buf, sizeof buf);
	_printjval(&fmt, v, 0);
	fmtprint(&fmt, "\n");
	fmtfdflush(&fmt);
}
*/

int
jsonfmt(Fmt *fmt)
{
	Json *v;

	v = va_arg(fmt->args, Json*);
	if(fmt->flags&FmtSharp)
		_printjval(fmt, v, 0);
	else
		_printjval(fmt, v, -1);
	return 0;
}

Json*
jincref(Json *v)
{
	if(v == nil)
		return nil;
	++v->ref;
	return v;
}

void
jclose(Json *v)
{
	int i;

	if(v == nil)
		return;
	if(--v->ref > 0)
		return;
	if(v->ref < 0)
		sysfatal("jclose: ref %d", v->ref);

	switch(v->type){
	case Jstring:
		free(v->string);
		break;
	case Jarray:
		for(i=0; i<v->len; i++)
			jclose(v->value[i]);
		free(v->value);
		break;
	case Jobject:
		for(i=0; i<v->len; i++){
			free(v->name[i]);
			jclose(v->value[i]);
		}
		free(v->value);
		free(v->name);
		break;
	}
	free(v);
}

Json*
jlookup(Json *v, char *name)
{
	int i;

	if(v->type != Jobject)
		return nil;
	for(i=0; i<v->len; i++)
		if(strcmp(v->name[i], name) == 0)
			return v->value[i];
	return nil;
}

Json*
jwalk(Json *v, char *path)
{
	char elem[128], *p, *next;
	int n;

	for(p=path; *p && v; p=next){
		next = strchr(p, '/');
		if(next == nil)
			next = p+strlen(p);
		if(next-p >= sizeof elem)
			sysfatal("jwalk path elem too long - %s", path);
		memmove(elem, p, next-p);
		elem[next-p] = 0;
		if(*next == '/')
			next++;
		if(v->type == Jarray && *elem && (n=strtol(elem, &p, 10)) >= 0 && *p == 0){
			if(n >= v->len)
				return nil;
			v = v->value[n];
		}else
			v = jlookup(v, elem);
	}
	return v;
}

char*
jstring(Json *jv)
{
	if(jv == nil || jv->type != Jstring)
		return nil;
	return jv->string;
}

vlong
jint(Json *jv)
{
	if(jv == nil || jv->type != Jnumber)
		return -1;
	return jv->number;
}

double
jnumber(Json *jv)
{
	if(jv == nil || jv->type != Jnumber)
		return 0;
	return jv->number;
}

int
jstrcmp(Json *jv, char *s)
{
	char *t;

	t = jstring(jv);
	if(t == nil)
		return -2;
	return strcmp(t, s);
}
