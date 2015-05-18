#include	"mk.h"
#define	ARMAG	"!<arch>\n"
#define	SARMAG	8

#define	ARFMAG	"`\n"
#define SARNAME	16

struct	ar_hdr
{
	char	name[SARNAME];
	char	date[12];
	char	uid[6];
	char	gid[6];
	char	mode[8];
	char	size[10];
	char	fmag[2];
};
#define	SAR_HDR	(SARNAME+44)

static int dolong = 1;

static void atimes(char *);
static char *split(char*, char**);

long
readn(int f, void *av, long n)
{
	char *a;
	long m, t;

	a = av;
	t = 0;
	while(t < n){
		m = read(f, a+t, n-t);
		if(m <= 0){
			if(t == 0)
				return m;
			break;
		}
		t += m;
	}
	return t;
}
long
atimeof(int force, char *name)
{
	Symtab *sym;
	long t;
	char *archive, *member, buf[512];

	archive = split(name, &member);
	if(archive == 0)
		Exit();

	t = mtime(archive);
	sym = symlook(archive, S_AGG, 0);
	if(sym){
		if(force || (t > sym->u.value)){
			atimes(archive);
			sym->u.value = t;
		}
	}
	else{
		atimes(archive);
		/* mark the aggegate as having been done */
		symlook(strdup(archive), S_AGG, "")->u.value = t;
	}
		/* truncate long member name to sizeof of name field in archive header */
	if(dolong)
		snprint(buf, sizeof(buf), "%s(%s)", archive, member);
	else
		snprint(buf, sizeof(buf), "%s(%.*s)", archive, SARNAME, member);
	sym = symlook(buf, S_TIME, 0);
	if (sym)
		return sym->u.value;
	return 0;
}

void
atouch(char *name)
{
	char *archive, *member;
	int fd, i;
	struct ar_hdr h;
	long t;

	archive = split(name, &member);
	if(archive == 0)
		Exit();

	fd = open(archive, ORDWR);
	if(fd < 0){
		fd = create(archive, OWRITE, 0666);
		if(fd < 0){
			fprint(2, "create %s: %r\n", archive);
			Exit();
		}
		write(fd, ARMAG, SARMAG);
	}
	if(symlook(name, S_TIME, 0)){
		/* hoon off and change it in situ */
		LSEEK(fd, SARMAG, 0);
		while(read(fd, (char *)&h, sizeof(h)) == sizeof(h)){
			for(i = SARNAME-1; i > 0 && h.name[i] == ' '; i--)
					;
			h.name[i+1]=0;
			if(strcmp(member, h.name) == 0){
				t = SARNAME-sizeof(h);	/* ughgghh */
				LSEEK(fd, t, 1);
				fprint(fd, "%-12ld", time(0));
				break;
			}
			t = atol(h.size);
			if(t&01) t++;
			LSEEK(fd, t, 1);
		}
	}
	close(fd);
}

static void
atimes(char *ar)
{
	struct ar_hdr h;
	long t;
	int fd, i, namelen;
	char buf[2048], *p, *strings;
	char name[1024];
	Symtab *sym;

	strings = nil;
	fd = open(ar, OREAD);
	if(fd < 0)
		return;

	if(read(fd, buf, SARMAG) != SARMAG){
		close(fd);
		return;
	}
	while(readn(fd, (char *)&h, sizeof(h)) == sizeof(h)){
		t = atol(h.date);
		if(t == 0)	/* as it sometimes happens; thanks ken */
			t = 1;
		namelen = 0;
		if(memcmp(h.name, "#1/", 3) == 0){	/* BSD */
			namelen = atoi(h.name+3);
			if(namelen >= sizeof name){
				namelen = 0;
				goto skip;
			}
			if(readn(fd, name, namelen) != namelen)
				break;
			name[namelen] = 0;
		}else if(memcmp(h.name, "// ", 2) == 0){ /* GNU */
			/* date, uid, gid, mode all ' ' */
			for(i=2; i<16+12+6+6+8; i++)
				if(h.name[i] != ' ')
					goto skip;
			t = atol(h.size);
			if(t&01)
				t++;
			free(strings);
			strings = malloc(t+1);
			if(strings){
				if(readn(fd, strings, t) != t){
					free(strings);
					strings = nil;
					break;
				}
				strings[t] = 0;
				continue;
			}
			goto skip;
		}else if(strings && h.name[0]=='/' && isdigit((uchar)h.name[1])){
			i = strtol(h.name+1, &p, 10);
			if(*p != ' ' || i >= strlen(strings))
				goto skip;
			p = strings+i;
			for(; *p && *p != '/'; p++)
				;
			namelen = p-(strings+i);
			if(namelen >= sizeof name){
				namelen = 0;
				goto skip;
			}
			memmove(name, strings+i, namelen);
			name[namelen] = 0;
			namelen = 0;
		}else{
			strncpy(name, h.name, sizeof(h.name));
			for(i = sizeof(h.name)-1; i > 0 && name[i] == ' '; i--)
					;
			if(name[i] == '/')		/* system V bug */
				i--;
			name[i+1]=0;
		}
		snprint(buf, sizeof buf, "%s(%s)", ar, name);
		sym = symlook(strdup(buf), S_TIME, (void *)t);
		sym->u.value = t;
	skip:
		t = atol(h.size);
		if(t&01) t++;
		t -= namelen;
		LSEEK(fd, t, 1);
	}
	close(fd);
	free(strings);
}

static int
type(char *file)
{
	int fd;
	char buf[SARMAG];

	fd = open(file, OREAD);
	if(fd < 0){
		if(symlook(file, S_BITCH, 0) == 0){
			if(strlen(file) < 2 || strcmp(file+strlen(file)-2, ".a") != 0)
				Bprint(&bout, "%s doesn't exist: assuming it will be an archive\n", file);
			symlook(file, S_BITCH, (void *)file);
		}
		return 1;
	}
	if(read(fd, buf, SARMAG) != SARMAG){
		close(fd);
		return 0;
	}
	close(fd);
	return !strncmp(ARMAG, buf, SARMAG);
}

static char*
split(char *name, char **member)
{
	char *p, *q;

	p = strdup(name);
	q = utfrune(p, '(');
	if(q){
		*q++ = 0;
		if(member)
			*member = q;
		q = utfrune(q, ')');
		if (q)
			*q = 0;
		if(type(p))
			return p;
		free(p);
		fprint(2, "mk: '%s' is not an archive\n", name);
	}
	return 0;
}
