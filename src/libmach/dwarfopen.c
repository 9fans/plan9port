#include <u.h>
#include <libc.h>
#include <bio.h>
#include "elf.h"
#include "dwarf.h"

static int
readblock(int fd, DwarfBlock *b, ulong off, ulong len)
{
	b->data = malloc(len);
	if(b->data == nil)
		return -1;
	if(seek(fd, off, 0) < 0 || readn(fd, b->data, len) != len){
		free(b->data);
		b->data = nil;
		return -1;
	}
	b->len = len;
	return 0;
}

static int
findsection(Elf *elf, char *name, ulong *off, ulong *len)
{
	ElfSect *s;

	if((s = elfsection(elf, name)) == nil)
		return -1;
	*off = s->offset;
	*len = s->size;
	return s - elf->sect;
}

static int
loadsection(Elf *elf, char *name, DwarfBlock *b)
{
	ulong off, len;

	if(findsection(elf, name, &off, &len) < 0)
		return -1;
	return readblock(elf->fd, b, off, len);
}

Dwarf*
dwarfopen(Elf *elf)
{
	Dwarf *d;

	if(elf == nil){
		werrstr("nil elf passed to dwarfopen");
		return nil;
	}

	d = mallocz(sizeof(Dwarf), 1);
	if(d == nil)
		return nil;

	d->elf = elf;
	if(loadsection(elf, ".debug_abbrev", &d->abbrev) < 0
	|| loadsection(elf, ".debug_aranges", &d->aranges) < 0
	|| loadsection(elf, ".debug_line", &d->line) < 0
	|| loadsection(elf, ".debug_pubnames", &d->pubnames) < 0
	|| loadsection(elf, ".debug_info", &d->info) < 0)
		goto err;
	loadsection(elf, ".debug_frame", &d->frame);
	loadsection(elf, ".debug_ranges", &d->ranges);
	loadsection(elf, ".debug_str", &d->str);

	/* make this a table once there are more */
	switch(d->elf->hdr.machine){
	case ElfMach386:
		d->reg = dwarf386regs;
		d->nreg = dwarf386nregs;
		break;
	default:
		werrstr("unsupported machine");
		goto err;
	}

	return d;

err:
	free(d->abbrev.data);
	free(d->aranges.data);
	free(d->frame.data);
	free(d->line.data);
	free(d->pubnames.data);
	free(d->ranges.data);
	free(d->str.data);
	free(d->info.data);
	free(d);
	return nil;
}

void
dwarfclose(Dwarf *d)
{
	free(d->abbrev.data);
	free(d->aranges.data);
	free(d->frame.data);
	free(d->line.data);
	free(d->pubnames.data);
	free(d->ranges.data);
	free(d->str.data);
	free(d->info.data);
	free(d);
}
