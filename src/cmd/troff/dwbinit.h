/*
 *
 * A structure used to adjust pathnames in DWB C code. Pointers
 * set the address field, arrays use the value field and must
 * also set length to the number elements in the array. Pointers
 * are always reallocated and then reinitialized; arrays are only
 * reinitialized, if there's room.
 *
 */

typedef struct {
	char	**address;
	char	*value;
	int	length;
} dwbinit;

extern void	DWBinit(char *, dwbinit *);
extern char*	DWBhome(void);
extern void	DWBprefix(char *, char *, int);
