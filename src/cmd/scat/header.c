#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#include	"sky.h"

struct
{
	char	name[9];
	char	offset;
} Hproto[] =
{
	"ppo1",		Pppo1,
	"ppo2",		Pppo2,
	"ppo3",		Pppo3,
	"ppo4",		Pppo4,
	"ppo5",		Pppo5,
	"ppo6",		Pppo6,

	"amdx1",	Pamdx1,
	"amdx2",	Pamdx2,
	"amdx3",	Pamdx3,
	"amdx4",	Pamdx4,
	"amdx5",	Pamdx5,
	"amdx6",	Pamdx6,
	"amdx7",	Pamdx7,
	"amdx8",	Pamdx8,
	"amdx9",	Pamdx9,
	"amdx10",	Pamdx10,
	"amdx11",	Pamdx11,
	"amdx12",	Pamdx12,
	"amdx13",	Pamdx13,
	"amdx14",	Pamdx14,
	"amdx15",	Pamdx15,
	"amdx16",	Pamdx16,
	"amdx17",	Pamdx17,
	"amdx18",	Pamdx18,
	"amdx19",	Pamdx19,
	"amdx20",	Pamdx20,

	"amdy1",	Pamdy1,
	"amdy2",	Pamdy2,
	"amdy3",	Pamdy3,
	"amdy4",	Pamdy4,
	"amdy5",	Pamdy5,
	"amdy6",	Pamdy6,
	"amdy7",	Pamdy7,
	"amdy8",	Pamdy8,
	"amdy9",	Pamdy9,
	"amdy10",	Pamdy10,
	"amdy11",	Pamdy11,
	"amdy12",	Pamdy12,
	"amdy13",	Pamdy13,
	"amdy14",	Pamdy14,
	"amdy15",	Pamdy15,
	"amdy16",	Pamdy16,
	"amdy17",	Pamdy17,
	"amdy18",	Pamdy18,
	"amdy19",	Pamdy19,
	"amdy20",	Pamdy20,

	"pltscale",	Ppltscale,
	"xpixelsz",	Pxpixelsz,
	"ypixelsz",	Pypixelsz,

	"pltrah",	Ppltrah,
	"pltram",	Ppltram,
	"pltras",	Ppltras,
	"pltdecd",	Ppltdecd,
	"pltdecm",	Ppltdecm,
	"pltdecs",	Ppltdecs,

};

Header*
getheader(char *rgn)
{
	char rec[81], name[81], value[81];
	char *p;
	Biobuf *bin;
	Header hd, *h;
	int i, j, decsn, dss;

	dss = 0;
	sprint(rec, "/lib/sky/dssheaders/%s.hhh", rgn);
	bin = Bopen(unsharp(rec), OREAD);
/*
	if(bin == 0) {
		dss = 102;
		sprint(rec, "/n/juke/dss/dss.102/headers/%s.hhh", rgn);
		bin = Bopen(rec, OREAD);
	}
	if(bin == 0) {
		dss = 61;
		sprint(rec, "/n/juke/dss/dss.061/headers/%s.hhh", rgn);
		bin = Bopen(rec, OREAD);
	}
*/
	if(bin == 0) {
		fprint(2, "cannot open %s\n", rgn);
		exits("file");
	}
	if(debug)
		Bprint(&bout, "reading %s\n", rec);
	if(dss)
		Bprint(&bout, "warning: reading %s from jukebox\n", rec);

	memset(&hd, 0, sizeof(hd));
	j = 0;
	decsn = 0;
	for(;;) {
		if(dss) {
			if(Bread(bin, rec, 80) != 80)
				break;
			rec[80] = 0;
		} else {
			p = Brdline(bin, '\n');
			if(p == 0)
				break;
			p[Blinelen(bin)-1] = 0;
			strcpy(rec, p);
		}

		p = strchr(rec, '/');
		if(p)
			*p = 0;
		p = strchr(rec, '=');
		if(p == 0)
			continue;
		*p++ = 0;
		if(getword(name, rec) == 0)
			continue;
		if(getword(value, p) == 0)
			continue;
		if(strcmp(name, "pltdecsn") == 0) {
			if(strchr(value, '-'))
				decsn = 1;
			continue;
		}
		for(i=0; i<nelem(Hproto); i++) {
			j++;
			if(j >= nelem(Hproto))
				j = 0;
			if(strcmp(name, Hproto[j].name) == 0) {
				hd.param[(uchar)Hproto[j].offset] = atof(value);
				break;
			}
		}
	}
	Bterm(bin);

	hd.param[Ppltra] = RAD(hd.param[Ppltrah]*15 +
		hd.param[Ppltram]/4 + hd.param[Ppltras]/240);
	hd.param[Ppltdec] = RAD(hd.param[Ppltdecd] +
		hd.param[Ppltdecm]/60 + hd.param[Ppltdecs]/3600);
	if(decsn)
		hd.param[Ppltdec] = -hd.param[Ppltdec];
	hd.amdflag = 0;
	for(i=Pamdx1; i<=Pamdx20; i++)
		if(hd.param[i] != 0) {
			hd.amdflag = 1;
			break;
		}
	h = malloc(sizeof(*h));
	*h = hd;
	return h;
}

void
getplates(void)
{
	char rec[81], *q;
	Plate *p;
	Biobuf *bin;
	int c, i, dss;

	dss = 0;
	sprint(rec, "/lib/sky/dssheaders/lo_comp.lis");
	bin = Bopen(rec, OREAD);
	if(bin == 0) {
		dss = 102;
		sprint(rec, "%s/headers/lo_comp.lis", dssmount(dss));
		bin = Bopen(rec, OREAD);
	}
	if(bin == 0) {
		dss = 61;
		sprint(rec, "%s/headers/lo_comp.lis", dssmount(dss));
		bin = Bopen(rec, OREAD);
	}
	if(bin == 0) {
		fprint(2, "can't open lo_comp.lis; try 9fs juke\n");
		exits("open");
	}
	if(debug)
		Bprint(&bout, "reading %s\n", rec);
	if(dss)
		Bprint(&bout, "warning: reading %s from jukebox\n", rec);
	for(nplate=0;;) {
		if(dss) {
			if(Bread(bin, rec, 80) != 80)
				break;
			rec[80] = 0;
		} else {
			q = Brdline(bin, '\n');
			if(q == 0)
				break;
			q[Blinelen(bin)-1] = 0;
			strcpy(rec, q);
		}
		if(rec[0] == '#')
			continue;
		if(nplate < nelem(plate)) {
			p = &plate[nplate];
			memmove(p->rgn, rec+0, 5);
			if(p->rgn[4] == ' ')
				p->rgn[4] = 0;
			for(i=0; c=p->rgn[i]; i++)
				if(c >= 'A' && c <= 'Z')
					p->rgn[i] += 'a'-'A';
			p->ra = RAD(atof(rec+12)*15 +
				atof(rec+15)/4 +
				atof(rec+18)/240);
			p->dec = RAD(atof(rec+26) +
				atof(rec+29)/60 +
				atof(rec+32)/3600);
			if(rec[25] == '-')
				p->dec = -p->dec;
			p->disk = atoi(rec+53);
			if(p->disk == 0)
				continue;
		}
		nplate++;
	}
	Bterm(bin);

	if(nplate >= nelem(plate))
		fprint(2, "nplate too small %d %d\n", nelem(plate), nplate);
	if(debug)
		Bprint(&bout, "%d plates\n", nplate);
}

char*
dssmount(int dskno)
{
	Bprint(&bout, "not mounting dss\n");
	return "/n/dss";
}
