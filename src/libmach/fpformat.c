#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

/*
 *	Format floating point registers
 *
 *	Register codes in format field:
 *	'X' - print as 32-bit hexadecimal value
 *	'F' - 64-bit double register when modif == 'F'; else 32-bit single reg
 *	'f' - 32-bit ieee float
 *	'8' - big endian 80-bit ieee extended float
 *	'3' - little endian 80-bit ieee extended float with hole in bytes 8&9
 */
int
fpformat(Map *map, Regdesc *rp, char *buf, uint n, uint modif)
{
	char reg[12];
	u32int r;

	switch(rp->format)
	{
	case 'X':
		if (get4(map, rp->offset, &r) < 0)
			return -1;
		snprint(buf, n, "%lux", r);
		break;
	case 'F':	/* first reg of double reg pair */
		if (modif == 'F')
		if ((rp->format=='F') || (((rp+1)->flags&RFLT) && (rp+1)->format == 'f')) {
			if (get1(map, rp->offset, (uchar *)reg, 8) < 0)
				return -1;
			mach->ftoa64(buf, n, reg);
			if (rp->format == 'F')
				return 1;
			return 2;
		}
			/* treat it like 'f' */
		if (get1(map, rp->offset, (uchar *)reg, 4) < 0)
			return -1;
		mach->ftoa32(buf, n, reg);
		break;
	case 'f':	/* 32 bit float */
		if (get1(map, rp->offset, (uchar *)reg, 4) < 0)
			return -1;
		mach->ftoa32(buf, n, reg);
		break;
	case '3':	/* little endian ieee 80 with hole in bytes 8&9 */
		if (get1(map, rp->offset, (uchar *)reg, 10) < 0)
			return -1;
		memmove(reg+10, reg+8, 2);	/* open hole */
		memset(reg+8, 0, 2);		/* fill it */
		leieeeftoa80(buf, n, reg);
		break;
	case '8':	/* big-endian ieee 80 */
		if (get1(map, rp->offset, (uchar *)reg, 10) < 0)
			return -1;
		beieeeftoa80(buf, n, reg);
		break;
	default:	/* unknown */
		break;
	}
	return 1;
}
