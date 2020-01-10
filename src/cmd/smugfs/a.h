#include <u.h>
#include <errno.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <auth.h>
#include <9p.h>
#include <libsec.h>

#define APIKEY        "G9ANE2zvCozKEoLQ5qaR1AUtcE5YpuDj"
#define HOST          "api.smugmug.com"
#define UPLOAD_HOST   "upload.smugmug.com"
#define API_VERSION   "1.2.1"
#define PATH          "/services/api/json/" API_VERSION "/"
#define USER_AGENT    "smugfs (part of Plan 9 from User Space)"

void*	emalloc(int);
void*	erealloc(void*, int);
char*	estrdup(char*);
int	urlencodefmt(Fmt*);
int	timefmt(Fmt*);
int	writen(int, void*, int);


// Generic cache

typedef struct Cache Cache;
typedef struct CEntry CEntry;

struct CEntry
{
	char *name;
	struct {
		CEntry *next;
		CEntry *prev;
	} list;
	struct {
		CEntry *next;
	} hash;
};

Cache *newcache(int sizeofentry, int maxentry, void (*cefree)(CEntry*));
CEntry *cachelookup(Cache*, char*, int);
void cacheflush(Cache*, char*);

// JSON parser

typedef struct Json Json;

enum
{
	Jstring,
	Jnumber,
	Jobject,
	Jarray,
	Jtrue,
	Jfalse,
	Jnull
};

struct Json
{
	int ref;
	int type;
	char *string;
	double number;
	char **name;
	Json **value;
	int len;
};

void	jclose(Json*);
Json*	jincref(Json*);
vlong	jint(Json*);
Json*	jlookup(Json*, char*);
double	jnumber(Json*);
int	jsonfmt(Fmt*);
int	jstrcmp(Json*, char*);
char*	jstring(Json*);
Json*	jwalk(Json*, char*);
Json*	parsejson(char*);


// Wrapper to hide whether we're using OpenSSL for HTTPS.

typedef struct Protocol Protocol;
typedef struct Pfd Pfd;
struct Protocol
{
	Pfd *(*connect)(char *host);
	int (*read)(Pfd*, void*, int);
	int (*write)(Pfd*, void*, int);
	void (*close)(Pfd*);
};

Protocol http;
Protocol https;


// HTTP library

typedef struct HTTPHeader HTTPHeader;
struct HTTPHeader
{
	int code;
	char proto[100];
	char codedesc[100];
	vlong contentlength;
	char contenttype[100];
};

char *httpreq(Protocol *proto, char *host, char *request, HTTPHeader *hdr, int rfd, vlong rlength);
int httptofile(Protocol *proto, char *host, char *req, HTTPHeader *hdr, int wfd);


// URL downloader - caches in files on disk

int download(char *url, HTTPHeader *hdr);
void downloadflush(char*);

// JSON RPC

enum
{
	MaxResponse = 1<<29,
};

Json*	jsonrpc(Protocol *proto, char *host, char *path, char *method, char *name1, va_list arg, int usecache);
Json*	jsonupload(Protocol *proto, char *host, char *req, int rfd, vlong rlength);
void	jcacheflush(char*);

extern int chattyhttp;


// SmugMug RPC

#ifdef __GNUC__
#define check_nil __attribute__((sentinel))
#else
#define check_nil
#endif

Json* smug(char *method, char *name1, ...) check_nil;  // cached, http
Json* ncsmug(char *method, char *name1, ...) check_nil;  // not cached, https


// Session information

extern Json *userinfo;
extern char *sessid;


// File system

extern Srv xsrv;
void xinit(void);
extern int nickindex(char*);

// Logging

typedef struct Logbuf Logbuf;
struct Logbuf
{
	Req *wait;
	Req **waitlast;
	int rp;
	int wp;
	char *msg[128];
};

extern void	lbkick(Logbuf*);
extern void	lbappend(Logbuf*, char*, ...);
extern void	lbvappend(Logbuf*, char*, va_list);
/* #pragma varargck argpos lbappend 2 */
extern void	lbread(Logbuf*, Req*);
extern void	lbflush(Logbuf*, Req*);
/* #pragma varargck argpos flog 1 */

extern void	rpclog(char*, ...);
extern void	rpclogflush(Req*);
extern void	rpclogread(Req*);
extern void	rpclogwrite(Req*);

enum
{
	STACKSIZE = 32768
};

extern int printerrors;
