/*
 *
 * Pathname management routines for DWB C programs.
 *
 * Applications should initialize a dwbinit array with the string
 * pointers and arrays that need to be updated, and then hand that
 * array to DWBinit before much else happens in their main program.
 * DWBinit calls DWBhome to get the current home directory. DWBhome
 * uses the last definition of DWBENV (usually "DWBHOME") in file
 * DWBCONFIG (e.g., /usr/lib/dwb3.4) or the value assigned to that
 * variable in the environment if the DWBCONFIG file doesn't exist,
 * can't be read, or doesn't define DWBENV.
 *
 * DWBCONFIG must be a simple shell script - comments, a definition
 * of DWBHOME, and perhaps an export or echo is about all that's
 * allowed. The parsing in DWBhome is simple and makes no attempt
 * to duplicate the shell. It only looks for DWBHOME= as the first
 * non-white space string on a line, so
 *
 *	#
 *	# A sample DWBCONFIG shell script
 *	#
 *
 *	DWBHOME=/usr/add-on/dwb3.4
 *	export DWBHOME
 *
 * means DWBhome would return "/usr/add-on/dwb3.4" for the DWB home
 * directory. A DWBCONFIG file means there can only be one working
 * copy of a DWB release on a system, which seems like a good idea.
 * Using DWBCONFIG also means programs will always include correct
 * versions of files (e.g., prologues or macro packages).
 *
 * Relying on an environment variable guarantees nothing. You could
 * execute a version of dpost, but your environment might point at
 * incorrect font tables or prologues. Despite the obvious problems
 * we've also implemented an environment variable approach, but it's
 * only used if there's no DWBCONFIG file.
 *
 * DWBinit calls DWBhome to get the DWB home directory prefix and
 * then marches through its dwbinit argument, removing the default
 * home directory and prepending the new home. DWBinit stops when
 * it reaches an element that has NULL for its address and value
 * fields. Pointers in a dwbinit array are reallocated and properly
 * initialized; arrays are simply reinitialized if there's room.
 * All pathnames that are to be adjusted should be relative. For
 * example,
 *
 *	char	*fontdir = "lib/font";
 *	char	xyzzy[25] = "etc/xyzzy";
 *
 * would be represented in a dwbinit array as,
 *
 *	dwbinit allpaths[] = {
 *		&fontdir, NULL, 0,
 *		NULL, xyzzy, sizeof(xyzzy),
 *		NULL, NULL, 0
 *	};
 *
 * The last element must have NULL entries for the address and
 * value fields. The main() routine would then do,
 *
 *	#include "dwbinit.h"
 *
 *	main() {
 *
 *		DWBinit("program name", allpaths);
 *		...
 *	}
 *
 * Debugging is enabled if DWBDEBUG is in the environment and has
 * the value ON. Output is occasionally useful and probably should
 * be documented.
 *
 */

#include <u.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "dwbinit.h"

#ifndef DWBCONFIG
#define DWBCONFIG	"/dev/null"
#endif

#ifndef DWBENV
#define DWBENV		"DWBHOME"
#endif

#ifndef DWBHOME
#define DWBHOME		""
#endif

#ifndef DWBDEBUG
#define DWBDEBUG	"DWBDEBUG"
#endif

#ifndef DWBPREFIX
#define DWBPREFIX	"\\*(.P"
#endif

/*****************************************************************************/

void DWBdebug(dwbinit *ptr, int level)
{

    char	*path;
    char	*home;
    static char	*debug = NULL;

/*
 *
 * Debugging output, but only if DWBDEBUG is defined to be ON in the
 * environment. Dumps general info the first time through.
 *
 */

    if ( debug == NULL && (debug = getenv(DWBDEBUG)) == NULL )
	debug = "OFF";

    if ( strcmp(debug, "ON") == 0 ) {
	if ( level == 0 ) {
	    fprintf(stderr, "Environment variable: %s\n", DWBENV);
	    fprintf(stderr, "Configuration file: %s\n", DWBCONFIG);
	    fprintf(stderr, "Default home: %s\n", DWBHOME);
	    if ( (home = DWBhome()) != NULL )
		fprintf(stderr, "Current home: %s\n", home);
	}   /* End if */

	fprintf(stderr, "\n%s pathnames:\n", level == 0 ? "Original" : "Final");
	for ( ; ptr->value != NULL || ptr->address != NULL; ptr++ ) {
	    if ( (path = ptr->value) == NULL ) {
		path = *ptr->address;
		fprintf(stderr, " pointer: %s\n", path);
	    } else fprintf(stderr, " array[%d]: %s\n", ptr->length, path);
	    if ( level == 0 && *path == '/' )
		fprintf(stderr, "  WARNING - absolute path\n");
	}   /* End for */
    }	/* End if */

}   /* End of DWBdebug */

/*****************************************************************************/

extern	char	*unsharp(char*);

char *DWBhome(void)
{

    FILE	*fp;
    char	*ptr;
    char	*path;
    int		len;
    char	buf[200];
    char	*home = NULL;

/*
 *
 * Return the DWB home directory. Uses the last definition of DWBENV
 * (usually "DWBHOME") in file DWBCONFIG (perhaps /usr/lib/dwb3.4) or
 * the value assigned to the variable named by the DWBENV string in
 * the environment if DWBCONFIG doesn't exist or doesn't define DWBENV.
 * Skips the file lookup if DWBCONFIG can't be read. Returns NULL if
 * there's no home directory.
 *
 */

    if ( (fp = fopen(DWBCONFIG, "r")) != NULL ) {
	len = strlen(DWBENV);
	while ( fgets(buf, sizeof(buf), fp) != NULL ) {
	    for ( ptr = buf; isspace((uchar)*ptr); ptr++ ) ;
	    if ( strncmp(ptr, DWBENV, len) == 0 && *(ptr+len) == '=' ) {
		path = ptr + len + 1;
		for ( ptr = path; !isspace((uchar)*ptr) && *ptr != ';'; ptr++ ) ;
		*ptr = '\0';
		if ( home != NULL )
		    free(home);
		if ( (home = malloc(strlen(path)+1)) != NULL )
		    strcpy(home, path);
	    }	/* End if */
	}   /* End while */
	fclose(fp);
    }   /* End if */

    if ( home == NULL ) {
	if ( (home = getenv(DWBENV)) == NULL ) {
	    if ( (home = DWBHOME) == NULL || *home == '\0' || *home == ' ' )
		home = NULL;
	}   /* End if */
	if ( home != NULL )
	    home = unsharp(home);
    }	/* End if */

    while (home && *home == '/' && *(home +1) == '/')	/* remove extra slashes */
	home++;
    return(home);

}   /* End of DWBhome */

/*****************************************************************************/

void DWBinit(char *prog, dwbinit *paths)
{

    char	*prefix;
    char	*value;
    char	*path;
    int		plen;
    int		length;
    dwbinit	*opaths = paths;

/*
 *
 * Adjust the pathnames listed in paths, using the home directory
 * returned by DWBhome(). Stops when it reaches an element that has
 * NULL address and value fields. Assumes pathnames are relative,
 * but changes everything. DWBdebug issues a warning if an original
 * path begins with a /.
 *
 * A non-NULL address refers to a pointer, which is reallocated and
 * then reinitialized. A NULL address implies a non-NULL value field
 * and describes a character array that we only reinitialize. The
 * length field for an array is the size of that array. The length
 * field of a pointer is an increment that's added to the length
 * required to store the new pathname string - should help when we
 * want to change character arrays to pointers in applications like
 * troff.
 *
 */

    if ( (prefix = DWBhome()) == NULL ) {
	fprintf(stderr, "%s: no DWB home directory\n", prog);
	exit(1);
    }	/* End if */

    DWBdebug(opaths, 0);
    plen = strlen(prefix);

    for ( ; paths->value != NULL || paths->address != NULL; paths++ ) {
	if ( paths->address == NULL ) {
	    length = 0;
	    value = paths->value;
	} else {
	    length = paths->length;
	    value = *paths->address;
	}   /* End else */

	length += plen + 1 + strlen(value);	/* +1 is for the '/' */

	if ( (path = malloc(length+1)) == NULL ) {
	    fprintf(stderr, "%s: can't allocate pathname memory\n", prog);
	    exit(1);
	}   /* End if */

	if ( *value != '\0' ) {
	    char *eop = prefix;
	    while(*eop++)
		;
	    eop -= 2;
	    if (*value != '/' && *eop != '/') {
		sprintf(path, "%s/%s", prefix, value);
	    } else if (*value == '/' && *eop == '/') {
		value++;
		sprintf(path, "%s%s", prefix, value);
	    } else
		sprintf(path, "%s%s", prefix, value);
	} else
		sprintf(path, "%s", prefix);

	if ( paths->address == NULL ) {
	    if ( strlen(path) >= paths->length ) {
		fprintf(stderr, "%s: no room for %s\n", prog, path);
		exit(1);
	    }	/* End if */
	    strcpy(paths->value, path);
	    free(path);
	} else *paths->address = path;
    }	/* End for */

    DWBdebug(opaths, 1);

}   /* End of DWBinit */

/*****************************************************************************/

void DWBprefix( char *prog, char *path, int length)
{

    char	*home;
    char	buf[512];
    int		len = strlen(DWBPREFIX);

/*
 *
 * Replace a leading DWBPREFIX string in path by the current DWBhome().
 * Used by programs that pretend to handle .so requests. Assumes path
 * is an array with room for length characters. The implementation is
 * not great, but should be good enough for now. Also probably should
 * have DWBhome() only do the lookup once, and remember the value if
 * called again.
 *
 */

    if ( strncmp(path, DWBPREFIX, len) == 0 ) {
	if ( (home = DWBhome()) != NULL ) {
	    if ( strlen(home) + strlen(path+len) < length ) {
		sprintf(buf, "%s%s", home, path+len);
		strcpy(path, buf);		/* assuming there's room in path */
	    } else fprintf(stderr, "%s: no room to grow path %s", prog, path);
	}   /* End if */
    }	/* End if */

}   /* End of DWBprefix */

/*****************************************************************************/
