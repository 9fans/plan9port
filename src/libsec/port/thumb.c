#include <u.h>
#include <libc.h>
#include <bio.h>
#include <auth.h>
#include <mp.h>
#include <libsec.h>

enum{ ThumbTab = 1<<10 };

static void *
emalloc(int n)
{
	void *p;
	if(n==0)
		n=1;
	p = malloc(n);
	if(p == nil){
		exits("out of memory");
	}
	memset(p, 0, n);
	return p;
}

void
freeThumbprints(Thumbprint *table)
{
	Thumbprint *hd, *p, *q;
	for(hd = table; hd < table+ThumbTab; hd++){
		for(p = hd->next; p; p = q){
			q = p->next;
			free(p);
		}
	}
	free(table);
}

int
okThumbprint(uchar *sum, Thumbprint *table)
{
	Thumbprint *p;
	int i = ((sum[0]<<8) + sum[1]) & (ThumbTab-1);

	for(p = table[i].next; p; p = p->next)
		if(memcmp(sum, p->sha1, SHA1dlen) == 0)
			return 1;
	return 0;
}

static void
loadThumbprints(char *file, Thumbprint *table, Thumbprint *crltab)
{
	Thumbprint *entry;
	Biobuf *bin;
	char *line, *field[50];
	uchar sum[SHA1dlen];
	int i;

	bin = Bopen(file, OREAD);
	if(bin == nil)
		return;
	for(; (line = Brdstr(bin, '\n', 1)) != 0; free(line)){
		if(tokenize(line, field, nelem(field)) < 2)
			continue;
		if(strcmp(field[0], "#include") == 0){
			loadThumbprints(field[1], table, crltab);
			continue;
		}
		if(strcmp(field[0], "x509") != 0 || strncmp(field[1], "sha1=", strlen("sha1=")) != 0)
			continue;
		field[1] += strlen("sha1=");
		dec16(sum, sizeof(sum), field[1], strlen(field[1]));
		if(crltab && okThumbprint(sum, crltab))
			continue;
		entry = (Thumbprint*)emalloc(sizeof(*entry));
		memcpy(entry->sha1, sum, SHA1dlen);
		i = ((sum[0]<<8) + sum[1]) & (ThumbTab-1);
		entry->next = table[i].next;
		table[i].next = entry;
	}
	Bterm(bin);
}

Thumbprint *
initThumbprints(char *ok, char *crl)
{
	Thumbprint *table, *crltab = nil;

	if(crl){
		crltab = emalloc(ThumbTab * sizeof(*table));
		loadThumbprints(crl, crltab, nil);
	}
	table = emalloc(ThumbTab * sizeof(*table));
	loadThumbprints(ok, table, crltab);
	free(crltab);
	return table;
}
