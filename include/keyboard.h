#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif
typedef struct 	Keyboardctl Keyboardctl;

struct	Keyboardctl
{
	struct Channel	*c;	/* chan(Rune)[20] */
};


extern	Keyboardctl*	initkeyboard(char*);
extern	int			ctlkeyboard(Keyboardctl*, char*);
extern	void			closekeyboard(Keyboardctl*);

enum {
	KF=	0xF000,	/* Rune: beginning of private Unicode space */
	/* KF|1, KF|2, ..., KF|0xC is F1, F2, ..., F12 */
	Khome=	KF|0x0D,
	Kup=	KF|0x0E,
	Kpgup=	KF|0x0F,
	Kprint=	KF|0x10,
	Kleft=	KF|0x11,
	Kright=	KF|0x12,
	Kdown=	0x80,
	Kview=	0x80,
	Kpgdown=	KF|0x13,
	Kins=	KF|0x14,
	Kend=	KF|0x18,

	Kalt=		KF|0x15,
	Kshift=	KF|0x16,
	Kctl=		KF|0x17,

	Kbs=	0x08,
	Kdel=	0x7f,
	Kesc=	0x1b,
	Keof=	0x04,

	Kcmd=	0xF100	/* Rune: beginning of Cmd+'a', Cmd+'A', etc on Mac */
};

#if defined(__cplusplus)
}
#endif
#endif
