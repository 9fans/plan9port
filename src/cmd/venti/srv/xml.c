#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "xml.h"

void xmlarena(Hio *hout, Arena *s, char *tag, int indent){
	xmlindent(hout, indent);
	hprint(hout, "<%s", tag);
	xmlaname(hout, s->name, "name");
	xmlu32int(hout, s->version, "version");
	xmlaname(hout, s->part->name, "partition");
	xmlu32int(hout, s->blocksize, "blocksize");
	xmlu64int(hout, s->base, "start");
	xmlu64int(hout, s->base+2*s->blocksize, "stop");
	xmlu32int(hout, s->ctime, "created");
	xmlu32int(hout, s->wtime, "modified");
	xmlsealed(hout, s->memstats.sealed, "sealed");
	xmlscore(hout, s->score, "score");
	xmlu32int(hout, s->memstats.clumps, "clumps");
	xmlu32int(hout, s->memstats.cclumps, "compressedclumps");
	xmlu64int(hout, s->memstats.uncsize, "data");
	xmlu64int(hout, s->memstats.used - s->memstats.clumps * ClumpSize, "compresseddata");
	xmlu64int(hout, s->memstats.used + s->memstats.clumps * ClumpInfoSize, "storage");
	hprint(hout, "/>\n");
}

void xmlindex(Hio *hout, Index *s, char *tag, int indent){
	int i;
	xmlindent(hout, indent);
	hprint(hout, "<%s", tag);
	xmlaname(hout, s->name, "name");
	xmlu32int(hout, s->version, "version");
	xmlu32int(hout, s->blocksize, "blocksize");
	xmlu32int(hout, s->tabsize, "tabsize");
	xmlu32int(hout, s->buckets, "buckets");
	xmlu32int(hout, s->div, "buckdiv");
	hprint(hout, ">\n");
	xmlindent(hout, indent + 1);
	hprint(hout, "<sects>\n");
	for(i = 0; i < s->nsects; i++)
		xmlamap(hout, &s->smap[i], "sect", indent + 2);
	xmlindent(hout, indent + 1);
	hprint(hout, "</sects>\n");
	xmlindent(hout, indent + 1);
	hprint(hout, "<amaps>\n");
	for(i = 0; i < s->narenas; i++)
		xmlamap(hout, &s->amap[i], "amap", indent + 2);
	xmlindent(hout, indent + 1);
	hprint(hout, "</amaps>\n");
	xmlindent(hout, indent + 1);
	hprint(hout, "<arenas>\n");
	for(i = 0; i < s->narenas; i++)
		xmlarena(hout, s->arenas[i], "arena", indent + 2);
	xmlindent(hout, indent + 1);
	hprint(hout, "</arenas>\n");
	xmlindent(hout, indent);
	hprint(hout, "</%s>\n", tag);
}

void xmlamap(Hio *hout, AMap *s, char *tag, int indent){
	xmlindent(hout, indent);
	hprint(hout, "<%s", tag);
	xmlaname(hout, s->name, "name");
	xmlu64int(hout, s->start, "start");
	xmlu64int(hout, s->stop, "stop");
	hprint(hout, "/>\n");
}
