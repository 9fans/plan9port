#include	"mk.h"

static char *vexpand(char*, Envy*, Bufblock*);

#define getfields mkgetfields

static int
getfields(char *str, char **args, int max, int mflag, char *set)
{
	Rune r;
	int nr, intok, narg;

	if(max <= 0)
		return 0;

	narg = 0;
	args[narg] = str;
	if(!mflag)
		narg++;
	intok = 0;
	for(;; str += nr) {
		nr = chartorune(&r, str);
		if(r == 0)
			break;
		if(utfrune(set, r)) {
			if(narg >= max)
				break;
			*str = 0;
			intok = 0;
			args[narg] = str + nr;
			if(!mflag)
				narg++;
		} else {
			if(!intok && mflag)
				narg++;
			intok = 1;
		}
	}
	return narg;
}

void
shprint(char *s, Envy *env, Bufblock *buf, Shell *sh)
{
	int n;
	Rune r;

	while(*s) {
		n = chartorune(&r, s);
		if (r == '$')
			s = vexpand(s, env, buf);
		else {
			rinsert(buf, r);
			s += n;
			s = sh->copyq(s, r, buf);	/*handle quoted strings*/
		}
	}
	insert(buf, 0);
}

static char *
mygetenv(char *name, Envy *env)
{
	if (!env)
		return 0;
	if (symlook(name, S_WESET, 0) == 0 && symlook(name, S_INTERNAL, 0) == 0)
		return 0;
		/* only resolve internal variables and variables we've set */
	for(; env->name; env++){
		if (strcmp(env->name, name) == 0)
			return wtos(env->values, ' ');
	}
	return 0;
}

static char *
vexpand(char *w, Envy *env, Bufblock *buf)
{
	char *s, carry, *p, *q;

	assert("vexpand no $", *w == '$');
	p = w+1;	/* skip dollar sign */
	if(*p == '{') {
		p++;
		q = utfrune(p, '}');
		if (!q)
			q = strchr(p, 0);
	} else
		q = shname(p);
	carry = *q;
	*q = 0;
	s = mygetenv(p, env);
	*q = carry;
	if (carry == '}')
		q++;
	if (s) {
		bufcpy(buf, s, strlen(s));
		free(s);
	} else 		/* copy name intact*/
		bufcpy(buf, w, q-w);
	return(q);
}

void
front(char *s)
{
	char *t, *q;
	int i, j;
	char *flds[512];

	q = strdup(s);
	i = getfields(q, flds, 512, 0, " \t\n");
	if(i > 5){
		flds[4] = flds[i-1];
		flds[3] = "...";
		i = 5;
	}
	t = s;
	for(j = 0; j < i; j++){
		for(s = flds[j]; *s; *t++ = *s++);
		*t++ = ' ';
	}
	*t = 0;
	free(q);
}
