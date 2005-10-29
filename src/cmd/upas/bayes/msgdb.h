typedef struct Msgdb Msgdb;

Msgdb *mdopen(char*, int);
long mdget(Msgdb*, char*);
void mdput(Msgdb*, char*, long);

void mdenum(Msgdb*);
int mdnext(Msgdb*, char**, long*);

void mdclose(Msgdb*);
