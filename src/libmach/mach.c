#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

Mach *mach;

extern Mach mach386;
extern Mach machpower;

static Mach *machs[] = 
{
	&mach386,
	&machpower,
};

Mach*
machbyname(char *name)
{
	int i;

	for(i=0; i<nelem(machs); i++)
		if(strcmp(machs[i]->name, name) == 0){
			mach = machs[i];
			return machs[i];
		}
	werrstr("machine '%s' not found", name);
	return nil;
}

