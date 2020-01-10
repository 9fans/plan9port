/*
 * Deterministic regexp program.
 */
typedef struct Dreprog Dreprog;
typedef struct Dreinst Dreinst;
typedef struct Drecase Drecase;

struct Dreinst
{
	int isfinal;
	int isloop;
	Drecase *c;
	int nc;
};

struct Dreprog
{
	Dreinst *start[4];
	int ninst;
	Dreinst inst[1];
};

struct Drecase
{
	uint start;
	Dreinst *next;
};

Dreprog* dregcvt(Reprog*);
int dregexec(Dreprog*, char*, int);
Dreprog* Breaddfa(Biobuf *b);
void Bprintdfa(Biobuf*, Dreprog*);
