typedef struct Addr Addr;
struct Addr
{
	Addr *next;
	char *val;
};

extern Addr* readaddrs(char*, Addr*);
