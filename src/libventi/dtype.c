#include <u.h>
#include <libc.h>
#include <venti.h>

enum {
	OVtErrType,		/* illegal */

	OVtRootType,
	OVtDirType,
	OVtPointerType0,
	OVtPointerType1,
	OVtPointerType2,
	OVtPointerType3,
	OVtPointerType4,
	OVtPointerType5,
	OVtPointerType6,
	OVtPointerType7,		/* not used */
	OVtPointerType8,		/* not used */
	OVtPointerType9,		/* not used */
	OVtDataType,

	OVtMaxType
};


uint todisk[] = {
	OVtDataType,
	OVtPointerType0,
	OVtPointerType1,
	OVtPointerType2,
	OVtPointerType3,
	OVtPointerType4,
	OVtPointerType5,
	OVtPointerType6,
	OVtDirType,
	OVtPointerType0,
	OVtPointerType1,
	OVtPointerType2,
	OVtPointerType3,
	OVtPointerType4,
	OVtPointerType5,
	OVtPointerType6,
	OVtRootType,
};

uint fromdisk[] = {
	~0,
	VtRootType,
	VtDirType,
	VtDirType+1,
	VtDirType+2,
	VtDirType+3,
	VtDirType+4,
	VtDirType+5,
	VtDirType+6,
	VtDirType+7,
	~0,
	~0,
	~0,
	VtDataType,
};

uint
vttodisktype(uint n)
{
	if(n >= nelem(todisk))
		return ~0;
	return todisk[n];
}

uint
vtfromdisktype(uint n)
{
	if(n >= nelem(fromdisk))
		return ~0;
	return fromdisk[n];
}

