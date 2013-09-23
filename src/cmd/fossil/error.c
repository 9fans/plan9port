#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

char EBadAddr[] = "illegal block address";
char EBadDir[] = "corrupted directory entry";
char EBadEntry[] = "corrupted file entry";
char EBadLabel[] = "corrupted block label";
char EBadMeta[] = "corrupted meta data";
char EBadMode[] = "illegal mode";
char EBadOffset[] = "illegal offset";
char EBadPath[] = "illegal path element";
char EBadRoot[] = "root of file system is corrupted";
char EBadSuper[] = "corrupted super block";
char EBlockTooBig[] = "block too big";
char ECacheFull[] = "no free blocks in memory cache";
char EConvert[] = "protocol botch";
char EExists[] = "file already exists";
char EFsFill[] = "file system is full";
char EIO[] = "i/o error";
char EInUse[] = "file is in use";
char ELabelMismatch[] = "block label mismatch";
char ENilBlock[] = "illegal block address";
char ENoDir[] = "directory entry is not allocated";
char ENoFile[] = "file does not exist";
char ENotDir[] = "not a directory";
char ENotEmpty[] = "directory not empty";
char ENotFile[] = "not a file";
char EReadOnly[] = "file is read only";
char ERemoved[] = "file has been removed";
char ENotArchived[] = "file is not archived";
char EResize[] = "only support truncation to zero length";
char ERoot[] = "cannot remove root";
char ESnapOld[] = "snapshot has been deleted";
char ESnapRO[] = "snapshot is read only";
char ETooBig[] = "file too big";
char EVentiIO[] = "venti i/o error";
