#ifndef _LIBSTRING_H_
#define _LIBSTRING_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

/*
#pragma	src	"/sys/src/libString"
#pragma	lib	"libString.a"
*/
AUTOLIB(String)

/* extensible Strings */
typedef struct String {
	Lock	lk;
	char	*base;	/* base of String */
	char	*end;	/* end of allocated space+1 */
	char	*ptr;	/* ptr into String */
	short	ref;
	uchar	fixed;
} String;

#define s_clone(s) s_copy((s)->base)
#define s_to_c(s) ((s)->base)
#define s_len(s) ((s)->ptr-(s)->base)

extern String*	s_append(String*, char*);
extern String*	s_array(char*, int);
extern String*	s_copy(char*);
extern void	s_free(String*);
extern String*	s_incref(String*);	
extern String*	s_memappend(String*, char*, int);
extern String*	s_nappend(String*, char*, int);
extern String*	s_new(void);
extern String*	s_newalloc(int);
extern String*	s_parse(String*, String*);
extern String*	s_reset(String*);
extern String*	s_restart(String*);
extern void	s_terminate(String*);
extern void	s_tolower(String*);
extern void	s_putc(String*, int);
extern String*	s_unique(String*);
extern String*	s_grow(String*, int);

#ifdef BGETC
extern int	s_read(Biobuf*, String*, int);
extern char	*s_read_line(Biobuf*, String*);
extern char	*s_getline(Biobuf*, String*);
typedef struct Sinstack Sinstack;
extern char	*s_rdinstack(Sinstack*, String*);
extern Sinstack	*s_allocinstack(char*);
extern void	s_freeinstack(Sinstack*);
#endif /* BGETC */
#if defined(__cplusplus)
}
#endif
#endif
