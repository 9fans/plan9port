typedef struct Postcrud Postcrud;
struct Postcrud
{
	int fd[2];
	Srv *s;
	char *name;
	char *mtpt;
	int flag;
};

Postcrud *_post1(Srv*, char*, char*, int);
void _post2(void*);
void _post3(Postcrud*);
