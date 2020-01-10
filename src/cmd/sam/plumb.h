typedef struct Plumbmsg Plumbmsg;

struct Plumbmsg {
        char *src;
        char *dst;
        char *wdir;
        char *type;
        char *attr;
        char *data;
        int ndata;
};

char *plumbunpackattr(char*);
char *plumbpack(Plumbmsg *, int *);
int plumbfree(Plumbmsg *);
char *cleanname(char*);
