/*
 * S-expressions as used by IMAP.
 */

enum
{
	SxUnknown = 0,
	SxAtom,
	SxString,
	SxNumber,
	SxList
};

typedef struct Sx Sx;
struct Sx
{
	int type;
	char *data;
	int ndata;
	vlong number;
	Sx **sx;
	int nsx;
};

Sx*	Brdsx(Biobuf*);
Sx*	Brdsx1(Biobuf*);
void	freesx(Sx*);
int	oksx(Sx*);
int	sxfmt(Fmt*);
int	sxwalk(Sx*);
