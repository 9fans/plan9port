#include "os.h"
#include <libsec.h>

char*
sha1pickle(SHA1state *s)
{
	char *p;
	int m, n;

	m = 5*9+4*((s->blen+3)/3);
	p = malloc(m);
	if(p == nil)
		return p;
	n = sprint(p, "%8.8ux %8.8ux %8.8ux %8.8ux %8.8ux ",
		s->state[0], s->state[1], s->state[2],
		s->state[3], s->state[4]);
	enc64(p+n, m-n, s->buf, s->blen);
	return p;
}

SHA1state*
sha1unpickle(char *p)
{
	SHA1state *s;

	s = malloc(sizeof(*s));
	if(s == nil)
		return nil;
	s->state[0] = strtoul(p, &p, 16);
	s->state[1] = strtoul(p, &p, 16);
	s->state[2] = strtoul(p, &p, 16);
	s->state[3] = strtoul(p, &p, 16);
	s->state[4] = strtoul(p, &p, 16);
	s->blen = dec64(s->buf, sizeof(s->buf), p, strlen(p));
	s->malloced = 1;
	s->seeded = 1;
	return s;
}
