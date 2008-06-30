/* These are the Macintosh key scancode constants -- from Inside Macintosh */
#define QZ_ESCAPE		0x35
#define QZ_F1			0x7A
#define QZ_F2			0x78
#define QZ_F3			0x63
#define QZ_F4			0x76
#define QZ_F5			0x60
#define QZ_F6			0x61
#define QZ_F7			0x62
#define QZ_F8			0x64
#define QZ_F9			0x65
#define QZ_F10			0x6D
#define QZ_F11			0x67
#define QZ_F12			0x6F
#define QZ_PRINT		0x69
#define QZ_SCROLLOCK    	0x6B
#define QZ_PAUSE		0x71
#define QZ_POWER		0x7F
#define QZ_BACKQUOTE		0x32
#define QZ_1			0x12
#define QZ_2			0x13
#define QZ_3			0x14
#define QZ_4			0x15
#define QZ_5			0x17
#define QZ_6			0x16
#define QZ_7			0x1A
#define QZ_8			0x1C
#define QZ_9			0x19
#define QZ_0			0x1D
#define QZ_MINUS		0x1B
#define QZ_EQUALS		0x18
#define QZ_BACKSPACE		0x33
#define QZ_INSERT		0x72
#define QZ_HOME			0x73
#define QZ_PAGEUP		0x74
#define QZ_NUMLOCK		0x47
#define QZ_KP_EQUALS		0x51
#define QZ_KP_DIVIDE		0x4B
#define QZ_KP_MULTIPLY		0x43
#define QZ_TAB			0x30
#define QZ_q			0x0C
#define QZ_w			0x0D
#define QZ_e			0x0E
#define QZ_r			0x0F
#define QZ_t			0x11
#define QZ_y			0x10
#define QZ_u			0x20
#define QZ_i			0x22
#define QZ_o			0x1F
#define QZ_p			0x23
#define QZ_LEFTBRACKET		0x21
#define QZ_RIGHTBRACKET		0x1E
#define QZ_BACKSLASH		0x2A
#define QZ_DELETE		0x75
#define QZ_END			0x77
#define QZ_PAGEDOWN		0x79
#define QZ_KP7			0x59
#define QZ_KP8			0x5B
#define QZ_KP9			0x5C
#define QZ_KP_MINUS		0x4E
#define QZ_CAPSLOCK		0x39
#define QZ_a			0x00
#define QZ_s			0x01
#define QZ_d			0x02
#define QZ_f			0x03
#define QZ_g			0x05
#define QZ_h			0x04
#define QZ_j			0x26
#define QZ_k			0x28
#define QZ_l			0x25
#define QZ_SEMICOLON		0x29
#define QZ_QUOTE		0x27
#define QZ_RETURN		0x24
#define QZ_KP4			0x56
#define QZ_KP5			0x57
#define QZ_KP6			0x58
#define QZ_KP_PLUS		0x45
#define QZ_LSHIFT		0x38
#define QZ_z			0x06
#define QZ_x			0x07
#define QZ_c			0x08
#define QZ_v			0x09
#define QZ_b			0x0B
#define QZ_n			0x2D
#define QZ_m			0x2E
#define QZ_COMMA		0x2B
#define QZ_PERIOD		0x2F
#define QZ_SLASH		0x2C
/* These are the same as the left versions - use left by default */
#if 0
#define QZ_RSHIFT		0x38
#endif
#define QZ_UP			0x7E
#define QZ_KP1			0x53
#define QZ_KP2			0x54
#define QZ_KP3			0x55
#define QZ_KP_ENTER		0x4C
#define QZ_LCTRL		0x3B
#define QZ_LALT			0x3A
#define QZ_LMETA		0x37
#define QZ_SPACE		0x31
/* These are the same as the left versions - use left by default */
#if 0
#define QZ_RMETA		0x37
#define QZ_RALT			0x3A
#define QZ_RCTRL		0x3B
#endif
#define QZ_LEFT			0x7B
#define QZ_DOWN			0x7D
#define QZ_RIGHT		0x7C
#define QZ_KP0			0x52
#define QZ_KP_PERIOD		0x41

/* Wierd, these keys are on my iBook under MacOS X */
#define QZ_IBOOK_ENTER		0x34
#define QZ_IBOOK_LEFT		0x3B
#define QZ_IBOOK_RIGHT		0x3C
#define QZ_IBOOK_DOWN		0x3D
#define QZ_IBOOK_UP		0x3E
#define KEY_ENTER 13
#define KEY_TAB 9

#define KEY_BASE 0x100

/*  Function keys  */
#define KEY_F (KEY_BASE+64)

/* Control keys */
#define KEY_CTRL (KEY_BASE)
#define KEY_BACKSPACE (KEY_CTRL+0)
#define KEY_DELETE (KEY_CTRL+1)
#define KEY_INSERT (KEY_CTRL+2)
#define KEY_HOME (KEY_CTRL+3)
#define KEY_END (KEY_CTRL+4)
#define KEY_PAGE_UP (KEY_CTRL+5)
#define KEY_PAGE_DOWN (KEY_CTRL+6)
#define KEY_ESC (KEY_CTRL+7)

/* Control keys short name */
#define KEY_BS KEY_BACKSPACE
#define KEY_DEL KEY_DELETE
#define KEY_INS KEY_INSERT
#define KEY_PGUP KEY_PAGE_UP
#define KEY_PGDOWN KEY_PAGE_DOWN
#define KEY_PGDWN KEY_PAGE_DOWN

/* Cursor movement */
#define KEY_CRSR (KEY_BASE+16)
#define KEY_RIGHT (KEY_CRSR+0)
#define KEY_LEFT (KEY_CRSR+1)
#define KEY_DOWN (KEY_CRSR+2)
#define KEY_UP (KEY_CRSR+3)

/* Multimedia keyboard/remote keys */
#define KEY_MM_BASE (0x100+384)
#define KEY_POWER (KEY_MM_BASE+0)
#define KEY_MENU (KEY_MM_BASE+1)
#define KEY_PLAY (KEY_MM_BASE+2)
#define KEY_PAUSE (KEY_MM_BASE+3)
#define KEY_PLAYPAUSE (KEY_MM_BASE+4)
#define KEY_STOP (KEY_MM_BASE+5)
#define KEY_FORWARD (KEY_MM_BASE+6)
#define KEY_REWIND (KEY_MM_BASE+7)
#define KEY_NEXT (KEY_MM_BASE+8)
#define KEY_PREV (KEY_MM_BASE+9)
#define KEY_VOLUME_UP (KEY_MM_BASE+10)
#define KEY_VOLUME_DOWN (KEY_MM_BASE+11)
#define KEY_MUTE (KEY_MM_BASE+12)

/* Keypad keys */
#define KEY_KEYPAD (KEY_BASE+32)
#define KEY_KP0 (KEY_KEYPAD+0)
#define KEY_KP1 (KEY_KEYPAD+1)
#define KEY_KP2 (KEY_KEYPAD+2)
#define KEY_KP3 (KEY_KEYPAD+3)
#define KEY_KP4 (KEY_KEYPAD+4)
#define KEY_KP5 (KEY_KEYPAD+5)
#define KEY_KP6 (KEY_KEYPAD+6)
#define KEY_KP7 (KEY_KEYPAD+7)
#define KEY_KP8 (KEY_KEYPAD+8)
#define KEY_KP9 (KEY_KEYPAD+9)
#define KEY_KPDEC (KEY_KEYPAD+10)
#define KEY_KPINS (KEY_KEYPAD+11)
#define KEY_KPDEL (KEY_KEYPAD+12)
#define KEY_KPENTER (KEY_KEYPAD+13)

/* Special keys */
#define KEY_INTERN (0x1000)
#define KEY_CLOSE_WIN (KEY_INTERN+0)
