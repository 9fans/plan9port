#include <u.h>
#include <libc.h>
#include <draw.h>
#include <html.h>
#include "impl.h"

Rune **runeconsttab;
char *_runeconsttab[] = {
	"        ",
	" ",
	"",
	"#",
	"+",
	", ",
	"-",
	"-->",
	"1",
	"<",
	">",
	"?",
	"Index search terms:",
	"Reset",
	"Submit",
	"^0-9",
	"_ISINDEX_",
	"_blank",
	"_fr",
	"_no_name_submit_",
	"_parent",
	"_self",
	"_top",
	"application/x-www-form-urlencoded",
	"circle",
	"cm",
	"content-script-type",
	"disc",
	"em",
	"in",
	"javascript",
	"jscript",
	"jscript1.1",
	"mm",
	"none",
	"pi",
	"pt",
	"refresh",
	"select",
	"square",
	"textarea",
};

Rune**
_cvtstringtab(char **tab, int n)
{
	int i;
	Rune **rtab;

	rtab = emalloc(n*sizeof(rtab[0]));
	for(i=0; i<n; i++)
		rtab[i] = toStr((uchar*)tab[i], strlen(tab[i]), US_Ascii);
	return rtab;
}

StringInt*
_cvtstringinttab(AsciiInt *tab, int n)
{
	int i;
	StringInt *stab;

	stab = emalloc(n*sizeof(stab[0]));
	for(i=0; i<n; i++){
		stab[i].key = toStr((uchar*)tab[i].key, strlen(tab[i].key), US_Ascii);
		stab[i].val = tab[i].val;
	}
	return stab;
}

void
_runetabinit(void)
{
	runeconsttab = _cvtstringtab(_runeconsttab, nelem(_runeconsttab));
	return;
}
