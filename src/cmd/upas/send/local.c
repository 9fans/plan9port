#include "common.h"
#include "send.h"

static void
mboxfile(dest *dp, String *user, String *path, char *file)
{
	char *cp;

	mboxpath(s_to_c(user), s_to_c(dp->addr), path, 0);
	cp = strrchr(s_to_c(path), '/');
	if(cp)
		path->ptr = cp+1;
	else
		path->ptr = path->base;
	s_append(path, file);
}

/*
 *  Check forwarding requests
 */
extern dest*
expand_local(dest *dp)
{
	Biobuf *fp;
	String *file, *line, *s;
	dest *rv;
	int forwardok;
	char *user;

	/* short circuit obvious security problems */
	if(strstr(s_to_c(dp->addr), "/../")){
		dp->status = d_unknown;
		return 0;
	}

	/* isolate user's name if part of a path */
	user = strrchr(s_to_c(dp->addr), '!');
	if(user)
		user++;
	else
		user = s_to_c(dp->addr);

	/* if no replacement string, plug in user's name */
	if(dp->repl1 == 0){
		dp->repl1 = s_new();
		mboxname(user, dp->repl1);
	}

	s = unescapespecial(s_clone(dp->repl1));

	/*
	 *  if this is the descendant of a `forward' file, don't
	 *  look for a forward.
	 */
	forwardok = 1;
	for(rv = dp->parent; rv; rv = rv->parent)
		if(rv->status == d_cat){
			forwardok = 0;
			break;
		}
	file = s_new();
	if(forwardok){
		/*
		 *  look for `forward' file for forwarding address(es)
		 */
		mboxfile(dp, s, file, "forward");
		fp = sysopen(s_to_c(file), "r", 0);
		if (fp != 0) {
			line = s_new();
			for(;;){
				if(s_read_line(fp, line) == nil)
					break;
				if(*(line->ptr - 1) != '\n')
					break;
				if(*(line->ptr - 2) == '\\')
					*(line->ptr-2) = ' ';
				*(line->ptr-1) = ' ';
			}
			sysclose(fp);
			if(debug)
				fprint(2, "forward = %s\n", s_to_c(line));
			rv = s_to_dest(s_restart(line), dp);
			s_free(line);
			if(rv){
				s_free(file);
				s_free(s);
				return rv;
			}
		}
	}

	/*
	 *  look for a 'pipe' file.  This won't work if there are
	 *  special characters in the account name since the file
	 *  name passes through a shell.  tdb.
	 */
	mboxfile(dp, dp->repl1, s_reset(file), "pipeto");
	if(sysexist(s_to_c(file))){
		if(debug)
			fprint(2, "found a pipeto file\n");
		dp->status = d_pipeto;
		line = s_new();
		s_append(line, "upasname='");
		s_append(line, user);
		s_append(line, "' ");
		s_append(line, s_to_c(file));
		s_append(line, " ");
		s_append(line, s_to_c(dp->addr));
		s_append(line, " ");
		s_append(line, s_to_c(dp->repl1));
		s_free(dp->repl1);
		dp->repl1 = line;
		s_free(file);
		s_free(s);
		return dp;
	}

	/*
	 *  see if the mailbox directory exists
	 */
	mboxfile(dp, s, s_reset(file), ".");
	if(sysexist(s_to_c(file)))
		dp->status = d_cat;
	else
		dp->status = d_unknown;
	s_free(file);
	s_free(s);
	return 0;
}
