#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include "common.h"
#include "tr2post.h"
#include "comments.h"
#include "path.h"

char *printdesclang = 0;
char *encoding = 0;
int devres;
int unitwidth;
int nspechars = 0;
struct charent spechars[MAXSPECHARS];

#define NDESCTOKS 9
static char *desctoks[NDESCTOKS] = {
	"PDL",
	"Encoding",
	"fonts",
	"sizes",
	"res",
	"hor",
	"vert",
	"unitwidth",
	"charset"
};

char *spechar[MAXSPECHARS];

int
hash(char *s, int l) {
    unsigned i;

    for (i=0; *s; s++)
	i = i*10 + *s;
    return(i % l);
}

BOOLEAN
readDESC(void) {
	char token[MAXTOKENSIZE];
	char *descnameformat = "%s/dev%s/DESC";
	char *descfilename = 0;
	Biobuf *bfd;
	Biobuf *Bfd;
	int i, state = -1;
	int fontindex = 0;

	if (debug) Bprint(Bstderr, "readDESC()\n");
	descfilename = galloc(descfilename, strlen(descnameformat)+strlen(FONTDIR)
		+strlen(devname), "readdesc");
	sprint(descfilename, descnameformat, FONTDIR, devname);
	if ((bfd = Bopen(unsharp(descfilename), OREAD)) == 0) {
		error(WARNING, "cannot open file %s\n", descfilename);
		return(0);
	}
	Bfd = bfd; /* &(bfd->Biobufhdr); */

	while (Bgetfield(Bfd, 's', token, MAXTOKENSIZE) > 0) {
		for (i=0; i<NDESCTOKS; i++) {
			if (strcmp(desctoks[i], token) == 0) {
				state = i;
				break;
			}
		}
		if (i<NDESCTOKS) continue;
		switch (state) {
		case 0:
			printdesclang=galloc(printdesclang, strlen(token)+1, "readdesc:");
			strcpy(printdesclang, token);
			if (debug) Bprint(Bstderr, "PDL %s\n", token);
			break;
		case 1:
			encoding=galloc(encoding, strlen(token)+1, "readdesc:");
			strcpy(encoding, token);
			if (debug) Bprint(Bstderr, "encoding %s\n", token);
			break;
		case 2:
			if (fontmnt <=0) {
				if (!isdigit((uchar)*token)) {
					error(WARNING, "readdesc: expecting number of fonts in mount table.\n");
					return(FALSE);
				}
				fontmnt = atoi(token) + 1;
				fontmtab = galloc(fontmtab, fontmnt*sizeof(char *), "readdesc:");

				for (i=0; i<fontmnt; i++)
					fontmtab[i] = 0;
				fontindex = 0;
			} else {
				mountfont(++fontindex, token);
				findtfn(token, TRUE);
			}
			break;
		case 3:
			/* I don't really care about sizes */
			break;
		case 4:
			/* device resolution in dots per inch */
			if (!isdigit((uchar)*token)) {
				error(WARNING, "readdesc: expecting device resolution.\n");
				return(FALSE);
			}
			devres = atoi(token);
			if (debug) Bprint(Bstderr, "res %d\n", devres);
			break;
		case 5:
			/* I don't really care about horizontal motion resolution */
			if (debug) Bprint(Bstderr, "ignoring horizontal resolution\n");
			break;
		case 6:
			/* I don't really care about vertical motion resolution */
			if (debug) Bprint(Bstderr, "ignoring vertical resolution\n");
			break;
		case 7:
			/* unitwidth is the font size at which the character widths are 1:1 */
			if (!isdigit((uchar)*token)) {
				error(WARNING, "readdesc: expecting unitwidth.\n");
				return(FALSE);
			}
			unitwidth = atoi(token);
			if (debug) Bprint(Bstderr, "unitwidth %d\n", unitwidth);
			break;
		case 8:
			/* I don't really care about this list of special characters */
			if (debug) Bprint(Bstderr, "ignoring special character <%s>\n", token);
			break;
		default:
			if (*token == '#')
				Brdline(Bfd, '\n');
			else
				error(WARNING, "unknown token %s in DESC file.\n", token);
			break;
		}
	}
	Bterm(Bfd);
	return 0;
}
