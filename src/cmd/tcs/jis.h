/*
	following astonishing goo courtesy of kogure.
*/
/*
 * MicroSoft Kanji Encoding (SJIS) Transformation
 */

/*
 * void
 * J2S(unsigned char *_h, unsigned char *_l)
 *	JIS X 208 to MS kanji transformation.
 *
 * Calling/Exit State:
 *	_h and _l should be in their valid range.
 *	No return value.
 */
#define J2S(_h, _l) { \
	/* lower: 21-7e >> 40-9d,9e-fb >> 40-7e,(skip 7f),80-fc */ \
	if (((_l) += (((_h)-- % 2) ? 0x1f : 0x7d)) > 0x7e) (_l)++; \
	/* upper: 21-7e >> 81-af >> 81-9f,(skip a0-df),e0-ef */ \
	if (((_h) = ((_h) / 2 + 0x71)) > 0x9f) (_h) += 0x40; \
}

/*
 * void
 * S2J(unsigned char *_h, unsigned char *_l)
 *	MS kanji to JIS X 208 transformation.
 *
 * Calling/Exit State:
 *	_h and _l should be in valid range.
 *	No return value.
 */
#define S2J(_h, _l) { \
	/* lower: 40-7e,80-fc >> 21-5f,61-dd >> 21-7e,7f-dc */ \
	if (((_l) -= 0x1f) > 0x60) (_l)--; \
	/* upper: 81-9f,e0-ef >> 00-1e,5f-6e >> 00-2e >> 21-7d */ \
	if (((_h) -= 0x81) > 0x5e) (_h) -= 0x40; (_h) *= 2, (_h) += 0x21; \
	/* upper: ,21-7d >> ,22-7e ; lower: ,7f-dc >> ,21-7e */ \
	if ((_l) > 0x7e) (_h)++, (_l) -= 0x5e; \
}

/*
 * int
 * ISJKANA(const unsigned char *_b)
 *	Tests given byte is in the range of JIS X 0201 katakana.
 *
 * Calling/Exit State:
 *	Returns 1 if it is, or 0 otherwise.
 */
#define	ISJKANA(_b)	(0xa0 <= (_b) && (_b) < 0xe0)

/*
 * int
 * CANS2JH(const unsigned char *_h)
 *	Tests given byte is in the range of valid first byte of MS
 *	kanji code; either acts as a subroutine of CANS2J() macro
 *	or can be used to parse MS kanji encoded strings.
 *
 * Calling/Exit State:
 *	Returns 1 if it is, or 0 otherwise.
 */
#define CANS2JH(_h)	((0x81 <= (_h) && (_h) < 0xf0) && !ISJKANA(_h))

/*
 * int
 * CANS2JL(const unsigned char *_l)
 *	Tests given byte is in the range of valid second byte of MS
 *	kanji code; acts as a subroutine of CANS2J() macro.
 *
 * Calling/Exit State:
 *	Returns 1 if it is, or 0 otherwise.
 */
#define CANS2JL(_l)	(0x40 <= (_l) && (_l) < 0xfd && (_l) != 0x7f)

/*
 * int
 * CANS2J(const unsigned char *_h, const unsinged char *_l)
 *	Tests given bytes form a MS kanji code point which can be
 *	transformed to a valid JIS X 208 code point.
 *
 * Calling/Exit State:
 *	Returns 1 if they are, or 0 otherwise.
 */
#define CANS2J(_h, _l)  (CANS2JH(_h) && CANS2JL(_l))

/*
 * int
 * CANJ2SB(const unsigned char *_b)
 *	Tests given bytes is in the range of valid 94 graphic
 *	character set; acts as a subroutine of CANJ2S() macro.
 *
 * Calling/Exit State:
 *	Returns 1 if it is, or 0 otherwise.
 */
#define CANJ2SB(_b)	(0x21 <= (_b) && (_b) < 0x7f)

/*
 * int
 * CANJ2S(const unsigned char *_h, const unsigned char *_l)
 *	Tests given bytes form valid JIS X 208 code points
 *	(which can be transformed to MS kanji).
 *
 * Calling/Exit State:
 *	Returns 1 if they are, or 0 otherwise.
 */
#define CANJ2S(_h, _l)	(CANJ2SB(_h) && CANJ2SB(_l))
