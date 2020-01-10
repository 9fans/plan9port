#include <u.h>
#include <libc.h>
#include "elf.h"

int
elfsym(Elf *elf, ElfSect *symtab, ElfSect *strtab, int ndx, ElfSym *
