#pragma	lib	"libcomplete.a"
#pragma src "/sys/src/libcomplete"

typedef struct Completion Completion;

struct Completion{
	uchar advance;		/* whether forward progress has been made */
	uchar complete;	/* whether the completion now represents a file or directory */
	char *string;		/* the string to advance, suffixed " " or "/" for file or directory */
	int nfile;			/* number of files that matched */
	char **filename;	/* their names */
};

Completion* complete(char *dir, char *s);
void freecompletion(Completion*);
