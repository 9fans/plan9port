#include <stdio.h>
#include <errno.h>

#if defined(V9) || defined(BSD4_2) || defined(plan9)
char *tempnam(char *dir, char *pfx) {
	int pid;
	unsigned int len;
	char *tnm, *malloc();
	static int seq = 0;

	pid = getpid();
	len = strlen(dir) + strlen(pfx) + 10;
	if ((tnm = malloc(len)) != NULL) {
		sprintf(tnm, "%s", dir);
		if (access(tnm, 7) == -1)
			return(NULL);
		do {
			sprintf(tnm, "%s/%s%d%d", dir, pfx, pid, seq++);
			errno = 0;
			if (access(tnm, 7) == -1)
				if (errno == ENOENT)
					return(tnm);
		} while (1);
	}
	return(tnm);
}
#endif
