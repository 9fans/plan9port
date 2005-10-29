typedef struct Stringtab	Stringtab;
struct Stringtab {
	Stringtab *link;
	Stringtab *hash;
	char *str;
	int n;
	int count;
	int date;
};

typedef struct Hash Hash;
struct Hash
{
	int sorted;
	Stringtab **stab;
	int nstab;
	int ntab;
	Stringtab *all;
};

Stringtab *findstab(Hash*, char*, int, int);
Stringtab *sortstab(Hash*);

int Bwritehash(Biobuf*, Hash*);	/* destroys hash */
void Breadhash(Biobuf*, Hash*, int);
void freehash(Hash*);
Biobuf *Bopenlock(char*, int);
