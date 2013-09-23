#include "stdinc.h"

#include "9.h"

enum {
	NUserHash	= 1009,
};

typedef struct Ubox Ubox;
typedef struct User User;

struct User {
	char*	uid;
	char*	uname;
	char*	leader;
	char**	group;
	int	ngroup;

	User*	next;			/* */
	User*	ihash;			/* lookup by .uid */
	User*	nhash;			/* lookup by .uname */
};

#pragma varargck type "U"   User*

struct Ubox {
	User*	head;
	User*	tail;
	int	nuser;
	int	len;

	User*	ihash[NUserHash];	/* lookup by .uid */
	User*	nhash[NUserHash];	/* lookup by .uname */
};

static struct {
	RWLock	lock;

	Ubox*	box;
} ubox;

static char usersDefault[] = {
	"adm:adm:adm:sys\n"
	"none:none::\n"
	"noworld:noworld::\n"
	"sys:sys::glenda\n"
	"glenda:glenda:glenda:\n"
};

static char* usersMandatory[] = {
	"adm",
	"none",
	"noworld",
	"sys",
	nil,
};

char* uidadm = "adm";
char* unamenone = "none";
char* uidnoworld = "noworld";

static u32int
userHash(char* s)
{
	uchar *p;
	u32int hash;

	hash = 0;
	for(p = (uchar*)s; *p != '\0'; p++)
		hash = hash*7 + *p;

	return hash % NUserHash;
}

static User*
_userByUid(Ubox* box, char* uid)
{
	User *u;

	if(box != nil){
		for(u = box->ihash[userHash(uid)]; u != nil; u = u->ihash){
			if(strcmp(u->uid, uid) == 0)
				return u;
		}
	}
	werrstr("uname: uid '%s' not found", uid);
	return nil;
}

char*
unameByUid(char* uid)
{
	User *u;
	char *uname;

	rlock(&ubox.lock);
	if((u = _userByUid(ubox.box, uid)) == nil){
		runlock(&ubox.lock);
		return nil;
	}
	uname = vtstrdup(u->uname);
	runlock(&ubox.lock);

	return uname;
}

static User*
_userByUname(Ubox* box, char* uname)
{
	User *u;

	if(box != nil){
		for(u = box->nhash[userHash(uname)]; u != nil; u = u->nhash){
			if(strcmp(u->uname, uname) == 0)
				return u;
		}
	}
	werrstr("uname: uname '%s' not found", uname);
	return nil;
}

char*
uidByUname(char* uname)
{
	User *u;
	char *uid;

	rlock(&ubox.lock);
	if((u = _userByUname(ubox.box, uname)) == nil){
		runlock(&ubox.lock);
		return nil;
	}
	uid = vtstrdup(u->uid);
	runlock(&ubox.lock);

	return uid;
}

static int
_groupMember(Ubox* box, char* group, char* member, int whenNoGroup)
{
	int i;
	User *g, *m;

	/*
	 * Is 'member' a member of 'group'?
	 * Note that 'group' is a 'uid' and not a 'uname'.
	 * A 'member' is automatically in their own group.
	 */
	if((g = _userByUid(box, group)) == nil)
		return whenNoGroup;
	if((m = _userByUname(box, member)) == nil)
		return 0;
	if(m == g)
		return 1;
	for(i = 0; i < g->ngroup; i++){
		if(strcmp(g->group[i], member) == 0)
			return 1;
	}
	return 0;
}

int
groupWriteMember(char* uname)
{
	int ret;

	/*
	 * If there is a ``write'' group, then only its members can write
	 * to the file system, no matter what the permission bits say.
	 *
	 * To users not in the ``write'' group, the file system appears
	 * read only.  This is used to serve sources.cs.bell-labs.com
	 * to the world.
	 *
	 * Note that if there is no ``write'' group, then this routine
	 * makes it look like everyone is a member -- the opposite
	 * of what groupMember does.
	 *
	 * We use this for sources.cs.bell-labs.com.
	 * If this slows things down too much on systems that don't
	 * use this functionality, we could cache the write group lookup.
	 */

	rlock(&ubox.lock);
	ret = _groupMember(ubox.box, "write", uname, 1);
	runlock(&ubox.lock);
	return ret;
}

static int
_groupRemMember(Ubox* box, User* g, char* member)
{
	int i;

	if(_userByUname(box, member) == nil)
		return 0;

	for(i = 0; i < g->ngroup; i++){
		if(strcmp(g->group[i], member) == 0)
			break;
	}
	if(i >= g->ngroup){
		if(strcmp(g->uname, member) == 0)
			werrstr("uname: '%s' always in own group", member);
		else
			werrstr("uname: '%s' not in group '%s'",
				member, g->uname);
		return 0;
	}

	vtfree(g->group[i]);

	box->len -= strlen(member);
	if(g->ngroup > 1)
		box->len--;
	g->ngroup--;
	switch(g->ngroup){
	case 0:
		vtfree(g->group);
		g->group = nil;
		break;
	default:
		for(; i < g->ngroup; i++)
			g->group[i] = g->group[i+1];
		g->group[i] = nil;		/* prevent accidents */
		g->group = vtrealloc(g->group, g->ngroup * sizeof(char*));
		break;
	}

	return 1;
}

static int
_groupAddMember(Ubox* box, User* g, char* member)
{
	User *u;

	if((u = _userByUname(box, member)) == nil)
		return 0;
	if(_groupMember(box, g->uid, u->uname, 0)){
		if(strcmp(g->uname, member) == 0)
			werrstr("uname: '%s' always in own group", member);
		else
			werrstr("uname: '%s' already in group '%s'",
				member, g->uname);
		return 0;
	}

	g->group = vtrealloc(g->group, (g->ngroup+1)*sizeof(char*));
	g->group[g->ngroup] = vtstrdup(member);
	box->len += strlen(member);
	g->ngroup++;
	if(g->ngroup > 1)
		box->len++;

	return 1;
}

int
groupMember(char* group, char* member)
{
	int r;

	if(group == nil)
		return 0;

	rlock(&ubox.lock);
	r = _groupMember(ubox.box, group, member, 0);
	runlock(&ubox.lock);

	return r;
}

int
groupLeader(char* group, char* member)
{
	int r;
	User *g;

	/*
	 * Is 'member' the leader of 'group'?
	 * Note that 'group' is a 'uid' and not a 'uname'.
	 * Uname 'none' cannot be a group leader.
	 */
	if(strcmp(member, unamenone) == 0 || group == nil)
		return 0;

	rlock(&ubox.lock);
	if((g = _userByUid(ubox.box, group)) == nil){
		runlock(&ubox.lock);
		return 0;
	}
	if(g->leader != nil){
		if(strcmp(g->leader, member) == 0){
			runlock(&ubox.lock);
			return 1;
		}
		r = 0;
	}
	else
		r = _groupMember(ubox.box, group, member, 0);
	runlock(&ubox.lock);

	return r;
}

static void
userFree(User* u)
{
	int i;

	vtfree(u->uid);
	vtfree(u->uname);
	if(u->leader != nil)
		vtfree(u->leader);
	if(u->ngroup){
		for(i = 0; i < u->ngroup; i++)
			vtfree(u->group[i]);
		vtfree(u->group);
	}
	vtfree(u);
}

static User*
userAlloc(char* uid, char* uname)
{
	User *u;

	u = vtmallocz(sizeof(User));
	u->uid = vtstrdup(uid);
	u->uname = vtstrdup(uname);

	return u;
}

int
validUserName(char* name)
{
	Rune *r;
	static Rune invalid[] = L"#:,()";

	for(r = invalid; *r != '\0'; r++){
		if(utfrune(name, *r))
			return 0;
	}
	return 1;
}

static int
userFmt(Fmt* fmt)
{
	User *u;
	int i, r;

	u = va_arg(fmt->args, User*);

	r = fmtprint(fmt, "%s:%s:", u->uid, u->uname);
	if(u->leader != nil)
		r += fmtprint(fmt, u->leader);
	r += fmtprint(fmt, ":");
	if(u->ngroup){
		r += fmtprint(fmt, u->group[0]);
		for(i = 1; i < u->ngroup; i++)
			r += fmtprint(fmt, ",%s", u->group[i]);
	}

	return r;
}

static int
usersFileWrite(Ubox* box)
{
	Fs *fs;
	User *u;
	int i, r;
	Fsys *fsys;
	char *p, *q, *s;
	File *dir, *file;

	if((fsys = fsysGet("main")) == nil)
		return 0;
	fsysFsRlock(fsys);
	fs = fsysGetFs(fsys);

	/*
	 * BUG:
	 * 	the owner/group/permissions need to be thought out.
	 */
	r = 0;
	if((dir = fileOpen(fs, "/active")) == nil)
		goto tidy0;
	if((file = fileWalk(dir, uidadm)) == nil)
		file = fileCreate(dir, uidadm, ModeDir|0775, uidadm);
	fileDecRef(dir);
	if(file == nil)
		goto tidy;
	dir = file;
	if((file = fileWalk(dir, "users")) == nil)
		file = fileCreate(dir, "users", 0664, uidadm);
	fileDecRef(dir);
	if(file == nil)
		goto tidy;
	if(!fileTruncate(file, uidadm))
		goto tidy;

	p = s = vtmalloc(box->len+1);
	q = p + box->len+1;
	for(u = box->head; u != nil; u = u->next){
		p += snprint(p, q-p, "%s:%s:", u->uid, u->uname);
		if(u->leader != nil)
			p+= snprint(p, q-p, u->leader);
		p += snprint(p, q-p, ":");
		if(u->ngroup){
			p += snprint(p, q-p, u->group[0]);
			for(i = 1; i < u->ngroup; i++)
				p += snprint(p, q-p, ",%s", u->group[i]);
		}
		p += snprint(p, q-p, "\n");
	}
	r = fileWrite(file, s, box->len, 0, uidadm);
	vtfree(s);

tidy:
	if(file != nil)
		fileDecRef(file);
tidy0:
	fsysFsRUnlock(fsys);
	fsysPut(fsys);

	return r;
}

static void
uboxRemUser(Ubox* box, User *u)
{
	User **h, *up;

	h = &box->ihash[userHash(u->uid)];
	for(up = *h; up != nil && up != u; up = up->ihash)
		h = &up->ihash;
	assert(up == u);
	*h = up->ihash;
	box->len -= strlen(u->uid);

	h = &box->nhash[userHash(u->uname)];
	for(up = *h; up != nil && up != u; up = up->nhash)
		h = &up->nhash;
	assert(up == u);
	*h = up->nhash;
	box->len -= strlen(u->uname);

	h = &box->head;
	for(up = *h; up != nil && strcmp(up->uid, u->uid) != 0; up = up->next)
		h = &up->next;
	assert(up == u);
	*h = u->next;
	u->next = nil;

	box->len -= 4;
	box->nuser--;
}

static void
uboxAddUser(Ubox* box, User* u)
{
	User **h, *up;

	h = &box->ihash[userHash(u->uid)];
	u->ihash = *h;
	*h = u;
	box->len += strlen(u->uid);

	h = &box->nhash[userHash(u->uname)];
	u->nhash = *h;
	*h = u;
	box->len += strlen(u->uname);

	h = &box->head;
	for(up = *h; up != nil && strcmp(up->uid, u->uid) < 0; up = up->next)
		h = &up->next;
	u->next = *h;
	*h = u;

	box->len += 4;
	box->nuser++;
}

static void
uboxDump(Ubox* box)
{
	User* u;

	consPrint("nuser %d len = %d\n", box->nuser, box->len);

	for(u = box->head; u != nil; u = u->next)
		consPrint("%U\n", u);
}

static void
uboxFree(Ubox* box)
{
	User *next, *u;

	for(u = box->head; u != nil; u = next){
		next = u->next;
		userFree(u);
	}
	vtfree(box);
}

static int
uboxInit(char* users, int len)
{
	User *g, *u;
	Ubox *box, *obox;
	int blank, comment, i, nline, nuser;
	char *buf, *f[5], **line, *p, *q, *s;

	/*
	 * Strip out whitespace and comments.
	 * Note that comments are pointless, they disappear
	 * when the server writes the database back out.
	 */
	blank = 1;
	comment = nline = 0;

	s = p = buf = vtmalloc(len+1);
	for(q = users; *q != '\0'; q++){
		if(*q == '\r' || *q == '\t' || *q == ' ')
			continue;
		if(*q == '\n'){
			if(!blank){
				if(p != s){
					*p++ = '\n';
					nline++;
					s = p;
				}
				blank = 1;
			}
			comment = 0;
			continue;
		}
		if(*q == '#')
			comment = 1;
		blank = 0;
		if(!comment)
			*p++ = *q;
	}
	*p = '\0';

	line = vtmallocz((nline+2)*sizeof(char*));
	if((i = gettokens(buf, line, nline+2, "\n")) != nline){
		fprint(2, "nline %d (%d) botch\n", nline, i);
		vtfree(line);
		vtfree(buf);
		return 0;
	}

	/*
	 * Everything is updated in a local Ubox until verified.
	 */
	box = vtmallocz(sizeof(Ubox));

	/*
	 * First pass - check format, check for duplicates
	 * and enter in hash buckets.
	 */
	nuser = 0;
	for(i = 0; i < nline; i++){
		s = vtstrdup(line[i]);
		if(getfields(s, f, nelem(f), 0, ":") != 4){
			fprint(2, "bad line '%s'\n", line[i]);
			vtfree(s);
			continue;
		}
		if(*f[0] == '\0' || *f[1] == '\0'){
			fprint(2, "bad line '%s'\n", line[i]);
			vtfree(s);
			continue;
		}
		if(!validUserName(f[0])){
			fprint(2, "invalid uid '%s'\n", f[0]);
			vtfree(s);
			continue;
		}
		if(_userByUid(box, f[0]) != nil){
			fprint(2, "duplicate uid '%s'\n", f[0]);
			vtfree(s);
			continue;
		}
		if(!validUserName(f[1])){
			fprint(2, "invalid uname '%s'\n", f[0]);
			vtfree(s);
			continue;
		}
		if(_userByUname(box, f[1]) != nil){
			fprint(2, "duplicate uname '%s'\n", f[1]);
			vtfree(s);
			continue;
		}

		u = userAlloc(f[0], f[1]);
		uboxAddUser(box, u);
		line[nuser] = line[i];
		nuser++;

		vtfree(s);
	}
	assert(box->nuser == nuser);

	/*
	 * Second pass - fill in leader and group information.
	 */
	for(i = 0; i < nuser; i++){
		s = vtstrdup(line[i]);
		getfields(s, f, nelem(f), 0, ":");

		assert(g = _userByUname(box, f[1]));
		if(*f[2] != '\0'){
			if((u = _userByUname(box, f[2])) == nil)
				g->leader = vtstrdup(g->uname);
			else
				g->leader = vtstrdup(u->uname);
			box->len += strlen(g->leader);
		}
		for(p = f[3]; p != nil; p = q){
			if((q = utfrune(p, L',')) != nil)
				*q++ = '\0';
			if(!_groupAddMember(box, g, p)){
				// print/log error here
			}
		}

		vtfree(s);
	}

	vtfree(line);
	vtfree(buf);

	for(i = 0; usersMandatory[i] != nil; i++){
		if((u = _userByUid(box, usersMandatory[i])) == nil){
			werrstr("user '%s' is mandatory", usersMandatory[i]);
			uboxFree(box);
			return 0;
		}
		if(strcmp(u->uid, u->uname) != 0){
			werrstr("uid/uname for user '%s' must match",
				usersMandatory[i]);
			uboxFree(box);
			return 0;
		}
	}

	wlock(&ubox.lock);
	obox = ubox.box;
	ubox.box = box;
	wunlock(&ubox.lock);

	if(obox != nil)
		uboxFree(obox);

	return 1;
}

int
usersFileRead(char* path)
{
	char *p;
	File *file;
	Fsys *fsys;
	int len, r;
	uvlong size;

	if((fsys = fsysGet("main")) == nil)
		return 0;
	fsysFsRlock(fsys);

	if(path == nil)
		path = "/active/adm/users";

	r = 0;
	if((file = fileOpen(fsysGetFs(fsys), path)) != nil){
		if(fileGetSize(file, &size)){
			len = size;
			p = vtmalloc(size+1);
			if(fileRead(file, p, len, 0) == len){
				p[len] = '\0';
				r = uboxInit(p, len);
			}
		}
		fileDecRef(file);
	}

	fsysFsRUnlock(fsys);
	fsysPut(fsys);

	return r;
}

static int
cmdUname(int argc, char* argv[])
{
	User *u, *up;
	int d, dflag, i, r;
	char *p, *uid, *uname;
	char *createfmt = "fsys main create /active/usr/%s %s %s d775";
	char *usage = "usage: uname [-d] uname [uid|:uid|%%newname|=leader|+member|-member]";

	dflag = 0;

	ARGBEGIN{
	default:
		return cliError(usage);
	case 'd':
		dflag = 1;
		break;
	}ARGEND

	if(argc < 1){
		if(!dflag)
			return cliError(usage);
		rlock(&ubox.lock);
		uboxDump(ubox.box);
		runlock(&ubox.lock);
		return 1;
	}

	uname = argv[0];
	argc--; argv++;

	if(argc == 0){
		rlock(&ubox.lock);
		if((u = _userByUname(ubox.box, uname)) == nil){
			runlock(&ubox.lock);
			return 0;
		}
		consPrint("\t%U\n", u);
		runlock(&ubox.lock);
		return 1;
	}

	wlock(&ubox.lock);
	u = _userByUname(ubox.box, uname);
	while(argc--){
		if(argv[0][0] == '%'){
			if(u == nil){
				wunlock(&ubox.lock);
				return 0;
			}
			p = &argv[0][1];
			if((up = _userByUname(ubox.box, p)) != nil){
				werrstr("uname: uname '%s' already exists",
					up->uname);
				wunlock(&ubox.lock);
				return 0;
			}
			for(i = 0; usersMandatory[i] != nil; i++){
				if(strcmp(usersMandatory[i], uname) != 0)
					continue;
				werrstr("uname: uname '%s' is mandatory",
					uname);
				wunlock(&ubox.lock);
				return 0;
			}

			d = strlen(p) - strlen(u->uname);
			for(up = ubox.box->head; up != nil; up = up->next){
				if(up->leader != nil){
					if(strcmp(up->leader, u->uname) == 0){
						vtfree(up->leader);
						up->leader = vtstrdup(p);
						ubox.box->len += d;
					}
				}
				for(i = 0; i < up->ngroup; i++){
					if(strcmp(up->group[i], u->uname) != 0)
						continue;
					vtfree(up->group[i]);
					up->group[i] = vtstrdup(p);
					ubox.box->len += d;
					break;
				}
			}

			uboxRemUser(ubox.box, u);
			vtfree(u->uname);
			u->uname = vtstrdup(p);
			uboxAddUser(ubox.box, u);
		}
		else if(argv[0][0] == '='){
			if(u == nil){
				wunlock(&ubox.lock);
				return 0;
			}
			if((up = _userByUname(ubox.box, &argv[0][1])) == nil){
				if(argv[0][1] != '\0'){
					wunlock(&ubox.lock);
					return 0;
				}
			}
			if(u->leader != nil){
				ubox.box->len -= strlen(u->leader);
				vtfree(u->leader);
				u->leader = nil;
			}
			if(up != nil){
				u->leader = vtstrdup(up->uname);
				ubox.box->len += strlen(u->leader);
			}
		}
		else if(argv[0][0] == '+'){
			if(u == nil){
				wunlock(&ubox.lock);
				return 0;
			}
			if((up = _userByUname(ubox.box, &argv[0][1])) == nil){
				wunlock(&ubox.lock);
				return 0;
			}
			if(!_groupAddMember(ubox.box, u, up->uname)){
				wunlock(&ubox.lock);
				return 0;
			}
		}
		else if(argv[0][0] == '-'){
			if(u == nil){
				wunlock(&ubox.lock);
				return 0;
			}
			if((up = _userByUname(ubox.box, &argv[0][1])) == nil){
				wunlock(&ubox.lock);
				return 0;
			}
			if(!_groupRemMember(ubox.box, u, up->uname)){
				wunlock(&ubox.lock);
				return 0;
			}
		}
		else{
			if(u != nil){
				werrstr("uname: uname '%s' already exists",
					u->uname);
				wunlock(&ubox.lock);
				return 0;
			}

			uid = argv[0];
			if(*uid == ':')
				uid++;
			if((u = _userByUid(ubox.box, uid)) != nil){
				werrstr("uname: uid '%s' already exists",
					u->uid);
				wunlock(&ubox.lock);
				return 0;
			}

			u = userAlloc(uid, uname);
			uboxAddUser(ubox.box, u);
			if(argv[0][0] != ':'){
				// should have an option for the mode and gid
				p = smprint(createfmt, uname, uname, uname);
				r = cliExec(p);
				vtfree(p);
				if(r == 0){
					wunlock(&ubox.lock);
					return 0;
				}
			}
		}
		argv++;
	}

	if(usersFileWrite(ubox.box) == 0){
		wunlock(&ubox.lock);
		return 0;
	}
	if(dflag)
		uboxDump(ubox.box);
	wunlock(&ubox.lock);

	return 1;
}

static int
cmdUsers(int argc, char* argv[])
{
	Ubox *box;
	int dflag, r, wflag;
	char *file;
	char *usage = "usage: users [-d | -r file] [-w]";

	dflag = wflag = 0;
	file = nil;

	ARGBEGIN{
	default:
		return cliError(usage);
	case 'd':
		dflag = 1;
		break;
	case 'r':
		file = ARGF();
		if(file == nil)
			return cliError(usage);
		break;
	case 'w':
		wflag = 1;
		break;
	}ARGEND

	if(argc)
		return cliError(usage);

	if(dflag && file)
		return cliError("cannot use -d and -r together");

	if(dflag)
		uboxInit(usersDefault, sizeof(usersDefault));
	else if(file){
		if(usersFileRead(file) == 0)
			return 0;
	}

	rlock(&ubox.lock);
	box = ubox.box;
	consPrint("\tnuser %d len %d\n", box->nuser, box->len);

	r = 1;
	if(wflag)
		r = usersFileWrite(box);
	runlock(&ubox.lock);
	return r;
}

int
usersInit(void)
{
	fmtinstall('U', userFmt);

	uboxInit(usersDefault, sizeof(usersDefault));

	cliAddCmd("users", cmdUsers);
	cliAddCmd("uname", cmdUname);

	return 1;
}
