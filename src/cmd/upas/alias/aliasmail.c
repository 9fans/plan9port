#include "common.h"

/*
 *  WARNING!  This turns all upper case names into lower case
 *  local ones.
 */

/* predeclared */
static String	*getdbfiles(void);
static int	translate(char*, char**, String*, String*);
static int	lookup(String**, String*, String*);
static int	compare(String*, char*);
static char*	mklower(char*);

static int debug;
static int from;
static char *namefiles = "namefiles";
#define DEBUG if(debug)

/* loop through the names to be translated */
void
main(int argc, char *argv[])
{
	String *s;
	String *alias;		/* the alias for the name */
	char **names;		/* names of this system */
	String *files;		/* list of files to search */
	int i, rv;
	char *p;

	ARGBEGIN {
	case 'd':
		debug = 1;
		break;
	case 'f':
		from = 1;
		break;
	case 'n':
		namefiles = ARGF();
		break;
	} ARGEND

	if (chdir(UPASLIB) < 0)
		sysfatal("aliasmail chdir %s: %r", UPASLIB);

	/* get environmental info */
	names = sysnames_read();
	files = getdbfiles();
	alias = s_new();

	/* loop through the names to be translated (from standard input) */
	for(i=0; i<argc; i++) {
		s = unescapespecial(s_copy(mklower(argv[i])));
		if(strchr(s_to_c(s), '!') == 0)
			rv = translate(s_to_c(s), names, files, alias);
		else
			rv = -1;
		if(from){
			if (rv >= 0 && *s_to_c(alias) != '\0'){
				p = strchr(s_to_c(alias), '\n');
				if(p)
					*p = 0;
				p = strchr(s_to_c(alias), '!');
				if(p) {
					*p = 0;
					print("%s", s_to_c(alias));
				} else {
					p = strchr(s_to_c(alias), '@');
					if(p)
						print("%s", p+1);
					else
						print("%s", s_to_c(alias));
				}
			}
		} else {
			if (rv < 0 || *s_to_c(alias) == '\0')
				print("local!%s\n", s_to_c(s));
			else {
				/* this must be a write, not a print */
				write(1, s_to_c(alias), strlen(s_to_c(alias)));
			}
		}
		s_free(s);
	}
	exits(0);
}

/* get the list of dbfiles to search */
static String *
getdbfiles(void)
{
	Sinstack *sp;
	String *files = s_new();
	char *nf;

	if(from)
		nf = "fromfiles";
	else
		nf = namefiles;

	/* system wide aliases */
	if ((sp = s_allocinstack(nf)) != 0){
		while(s_rdinstack(sp, files))
			s_append(files, " ");
		s_freeinstack(sp);
	}


	DEBUG print("files are %s\n", s_to_c(files));

	return files;
}

/* loop through the translation files */
static int
translate(char *name,		/* name to translate */
	char **namev,		/* names of this system */
	String *files,		/* names of system alias files */
	String *alias)		/* where to put the alias */
{
	String *file = s_new();
	String **fullnamev;
	int n, rv;

	rv = -1;

	DEBUG print("translate(%s, %s, %s)\n", name,
		s_to_c(files), s_to_c(alias));

	/* create the full name to avoid loops (system!name) */
	for(n = 0; namev[n]; n++)
		;
	fullnamev = (String**)malloc(sizeof(String*)*(n+2));
	n = 0;
	fullnamev[n++] = s_copy(name);
	for(; *namev; namev++){
		fullnamev[n] = s_copy(*namev);
		s_append(fullnamev[n], "!");
		s_append(fullnamev[n], name);
		n++;
	}
	fullnamev[n] = 0;

	/* look at system-wide names */
	s_restart(files);
	while (s_parse(files, s_restart(file)) != 0) {
		if (lookup(fullnamev, file, alias)==0) {
			rv = 0;
			goto out;
		}
	}

out:
	for(n = 0; fullnamev[n]; n++)
		s_free(fullnamev[n]);
	s_free(file);
	free(fullnamev);
	return rv;
}

/*
 *  very dumb conversion to bang format
 */
static String*
attobang(String *token)
{
	char *p;
	String *tok;

	p = strchr(s_to_c(token), '@');
	if(p == 0)
		return token;

	p++;
	tok = s_copy(p);
	s_append(tok, "!");
	s_nappend(tok, s_to_c(token), p - s_to_c(token) - 1);

	return tok;
}

/*  Loop through the entries in a translation file looking for a match.
 *  Return 0 if found, -1 otherwise.
 */
static int
lookup(
	String **namev,
	String *file,
	String *alias)	/* returned String */
{
	String *line = s_new();
	String *token = s_new();
	String *bangtoken;
	int i, rv = -1;
	char *name =  s_to_c(namev[0]);
	Sinstack *sp;

	DEBUG print("lookup(%s, %s, %s, %s)\n", s_to_c(namev[0]), s_to_c(namev[1]),
		s_to_c(file), s_to_c(alias));

	s_reset(alias);
	if ((sp = s_allocinstack(s_to_c(file))) == 0)
		return -1;

	/* look for a match */
	while (s_rdinstack(sp, s_restart(line))!=0) {
		DEBUG print("line is %s\n", s_to_c(line));
		s_restart(token);
		if (s_parse(s_restart(line), token)==0)
			continue;
		if (compare(token, "#include")==0){
			if(s_parse(line, s_restart(token))!=0) {
				if(lookup(namev, line, alias) == 0)
					break;
			}
			continue;
		}
		if (compare(token, name)!=0)
			continue;
		/* match found, get the alias */
		while(s_parse(line, s_restart(token))!=0) {
			bangtoken = attobang(token);

			/* avoid definition loops */
			for(i = 0; namev[i]; i++)
				if(compare(bangtoken, s_to_c(namev[i]))==0) {
					s_append(alias, "local");
					s_append(alias, "!");
					s_append(alias, name);
					break;
				}

			if(namev[i] == 0)
				s_append(alias, s_to_c(token));
			s_append(alias, "\n");

			if(bangtoken != token)
				s_free(bangtoken);
		}
		rv = 0;
		break;
	}
	s_free(line);
	s_free(token);
	s_freeinstack(sp);
	return rv;
}

#define lower(c) ((c)>='A' && (c)<='Z' ? (c)-('A'-'a'):(c))

/* compare two Strings (case insensitive) */
static int
compare(String *s1,
	char *p2)
{
	char *p1 = s_to_c(s1);
	int rv;

	DEBUG print("comparing %s to %s\n", p1, p2);
	while((rv = lower(*p1) - lower(*p2)) == 0) {
		if (*p1 == '\0')
			break;
		p1++;
		p2++;
	}
	return rv;
}

static char*
mklower(char *name)
{
	char *p;
	char c;

	for(p = name; *p; p++){
		c = *p;
		*p = lower(c);
	}
	return name;
}
