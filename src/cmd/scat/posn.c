#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#include	"sky.h"

void
amdinv(Header *h, Angle ra, Angle dec, float mag, float col)
{
	int i, max_iterations;
	float tolerance;
	float object_x, object_y, delta_x, delta_y, f, fx, fy, g, gx, gy;
	float x1, x2, x3, x4;
	float y1, y2, y3, y4;

	/*
	 *  Initialize
	 */
	max_iterations = 50;
	tolerance = 0.0000005;

	/*
	 *  Convert RA and Dec to St.coords
	 */
	traneqstd(h, ra, dec);

	/*
	 *  Set initial value for x,y
	 */
	object_x = h->xi/h->param[Ppltscale];
	object_y = h->eta/h->param[Ppltscale];

	/*
	 *  Iterate by Newtons method
	 */
	for(i = 0; i < max_iterations; i++) {
		/*
		 *  X plate model
		 */
		x1 = object_x;
		x2 = x1 * object_x;
		x3 = x1 * object_x;
		x4 = x1 * object_x;

		y1 = object_y;
		y2 = y1 * object_y;
		y3 = y1 * object_y;
		y4 = y1 * object_y;

		f =
			h->param[Pamdx1] * x1 +
			h->param[Pamdx2] * y1 +
		 	h->param[Pamdx3] +
			h->param[Pamdx4] * x2 +
			h->param[Pamdx5] * x1*y1 +
			h->param[Pamdx6] * y2 +
		   	h->param[Pamdx7] * (x2+y2) +
			h->param[Pamdx8] * x2*x1 +
			h->param[Pamdx9] * x2*y1 +
			h->param[Pamdx10] * x1*y2 +
			h->param[Pamdx11] * y3 +
			h->param[Pamdx12] * x1* (x2+y2) +
			h->param[Pamdx13] * x1 * (x2+y2) * (x2+y2) +
			h->param[Pamdx14] * mag +
			h->param[Pamdx15] * mag*mag +
			h->param[Pamdx16] * mag*mag*mag +
			h->param[Pamdx17] * mag*x1 +
			h->param[Pamdx18] * mag * (x2+y2) +
			h->param[Pamdx19] * mag*x1 * (x2+y2) +
			h->param[Pamdx20] * col;

		/*
		 *  Derivative of X model wrt x
		 */
		fx =
			h->param[Pamdx1] +
			h->param[Pamdx4] * 2*x1 +
			h->param[Pamdx5] * y1 +
			h->param[Pamdx7] * 2*x1 +
			h->param[Pamdx8] * 3*x2 +
			h->param[Pamdx9] * 2*x1*y1 +
			h->param[Pamdx10] * y2 +
			h->param[Pamdx12] * (3*x2+y2) +
			h->param[Pamdx13] * (5*x4 + 6*x2*y2 + y4) +
			h->param[Pamdx17] * mag +
			h->param[Pamdx18] * mag*2*x1 +
			h->param[Pamdx19] * mag*(3*x2+y2);

		/*
		 *  Derivative of X model wrt y
		 */
		fy =
			h->param[Pamdx2] +
			h->param[Pamdx5] * x1 +
			h->param[Pamdx6] * 2*y1 +
			h->param[Pamdx7] * 2*y1 +
			h->param[Pamdx9] * x2 +
			h->param[Pamdx10] * x1*2*y1 +
			h->param[Pamdx11] * 3*y2 +
			h->param[Pamdx12] * 2*x1*y1 +
			h->param[Pamdx13] * 4*x1*y1* (x2+y2) +
			h->param[Pamdx18] * mag*2*y1 +
			h->param[Pamdx19] * mag*2*x1*y1;
		/*
		 *  Y plate model
		 */
		g =
			h->param[Pamdy1] * y1 +
			h->param[Pamdy2] * x1 +
			h->param[Pamdy3] +
			h->param[Pamdy4] * y2 +
			h->param[Pamdy5] * y1*x1 +
			h->param[Pamdy6] * x2 +
			h->param[Pamdy7] * (x2+y2) +
			h->param[Pamdy8] * y3 +
			h->param[Pamdy9] * y2*x1 +
			h->param[Pamdy10] * y1*x3 +
			h->param[Pamdy11] * x3 +
			h->param[Pamdy12] * y1 * (x2+y2) +
			h->param[Pamdy13] * y1 * (x2+y2) * (x2+y2) +
			h->param[Pamdy14] * mag +
			h->param[Pamdy15] * mag*mag +
			h->param[Pamdy16] * mag*mag*mag +
			h->param[Pamdy17] * mag*y1 +
			h->param[Pamdy18] * mag * (x2+y2) +
			h->param[Pamdy19] * mag*y1 * (x2+y2) +
			h->param[Pamdy20] * col;

		/*
		 *  Derivative of Y model wrt x
		 */
		gx =
			h->param[Pamdy2] +
			h->param[Pamdy5] * y1 +
			h->param[Pamdy6] * 2*x1 +
			h->param[Pamdy7] * 2*x1 +
			h->param[Pamdy9] * y2 +
			h->param[Pamdy10] * y1*2*x1 +
			h->param[Pamdy11] * 3*x2 +
			h->param[Pamdy12] * 2*x1*y1 +
			h->param[Pamdy13] * 4*x1*y1 * (x2+y2) +
			h->param[Pamdy18] * mag*2*x1 +
			h->param[Pamdy19] * mag*y1*2*x1;

		/*
		 *  Derivative of Y model wrt y
		 */
		gy =
			h->param[Pamdy1] +
			h->param[Pamdy4] * 2*y1 +
			h->param[Pamdy5] * x1 +
			h->param[Pamdy7] * 2*y1 +
			h->param[Pamdy8] * 3*y2 +
			h->param[Pamdy9] * 2*y1*x1 +
			h->param[Pamdy10] * x2 +
			h->param[Pamdy12] * 3*y2 +
			h->param[Pamdy13] * (5*y4 + 6*x2*y2 + x4) +
			h->param[Pamdy17] * mag +
			h->param[Pamdy18] * mag*2*y1 +
			h->param[Pamdy19] * mag*(x2 + 3*y2);

		f = f - h->xi;
		g = g - h->eta;
		delta_x = (-f*gy+g*fy) / (fx*gy-fy*gx);
		delta_y = (-g*fx+f*gx) / (fx*gy-fy*gx);
		object_x = object_x + delta_x;
		object_y = object_y + delta_y;
		if((fabs(delta_x) < tolerance) && (fabs(delta_y) < tolerance))
			break;
	}

	/*
	 *  Convert mm from plate center to pixels
	 */
	h->x = (h->param[Pppo3]-object_x*1000.0)/h->param[Pxpixelsz];
	h->y = (h->param[Pppo6]+object_y*1000.0)/h->param[Pypixelsz];
}

void
ppoinv(Header *h, Angle ra, Angle dec)
{

	/*
	 *  Convert RA and Dec to standard coords.
	 */
	traneqstd(h, ra, dec);

	/*
	 *  Convert st.coords from arcsec to radians
	 */
	h->xi  /= ARCSECONDS_PER_RADIAN;
	h->eta /= ARCSECONDS_PER_RADIAN;

	/*
	 *  Compute PDS coordinates from solution
	 */
	h->x =
		h->param[Pppo1]*h->xi +
		h->param[Pppo2]*h->eta +
		h->param[Pppo3];
	h->y =
		h->param[Pppo4]*h->xi +
		h->param[Pppo5]*h->eta +
		h->param[Pppo6];
	/*
	 * Convert x,y from microns to pixels
	 */
	h->x /= h->param[Pxpixelsz];
	h->y /= h->param[Pypixelsz];

}

void
traneqstd(Header *h, Angle object_ra, Angle object_dec)
{
	float div;

	/*
	 *  Find divisor
	 */
	div =
		(sin(object_dec)*sin(h->param[Ppltdec]) +
		cos(object_dec)*cos(h->param[Ppltdec]) *
		cos(object_ra - h->param[Ppltra]));

	/*
	 *  Compute standard coords and convert to arcsec
	 */
	h->xi = cos(object_dec) *
		sin(object_ra - h->param[Ppltra]) *
		ARCSECONDS_PER_RADIAN/div;

	h->eta = (sin(object_dec)*cos(h->param[Ppltdec])-
		cos(object_dec)*sin(h->param[Ppltdec])*
		cos(object_ra - h->param[Ppltra]))*
		ARCSECONDS_PER_RADIAN/div;

}

void
xypos(Header *h, Angle ra, Angle dec, float mag, float col)
{
	if (h->amdflag) {
		amdinv(h, ra, dec, mag, col);
	} else {
		ppoinv(h, ra, dec);
	}
}
