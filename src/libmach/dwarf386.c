#include <u.h>
#include <libc.h>
#include <mach.h>
#include "elf.h"
#include "dwarf.h"

char*
dwarf386regs[] =
{
	"AX",
	"CX",
	"DX",
	"BX",
	"SP",
	"BP",
	"SI",
	"DI",
};

int dwarf386nregs = nelem(dwarf386regs);
