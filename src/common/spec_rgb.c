/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Convert colors and spectral ranges.
 */

#include "color.h"

/*
 *	The following table contains the CIE tristimulus integrals
 *  for X, Y, and Z.  The table is cumulative, so that
 *  each color coordinate integrates to 1.
 */

#define  STARTWL	380		/* starting wavelength (nanometers) */
#define  INCWL		10		/* wavelength increment */
#define  NINC		40		/* # of values */

static BYTE  chroma[3][NINC] = {
	{							/* X */
		0,   0,   0,   2,   6,   13,  22,  30,  36,  41,
		42,  43,  43,  44,  46,  52,  60,  71,  87,  106,
		128, 153, 178, 200, 219, 233, 243, 249, 252, 254,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255
	}, {							/* Y */
		0,   0,   0,   0,   0,   1,   2,   4,   7,   11,
		17,  24,  34,  48,  64,  84,  105, 127, 148, 169,
		188, 205, 220, 232, 240, 246, 250, 253, 254, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255
	}, {							/* Z */
		0,   0,   2,   10,  32,  66,  118, 153, 191, 220,
		237, 246, 251, 253, 254, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255
	}
};

static float  xyz2rgbmat[3][3] = {	/* XYZ to RGB */
	{(CIE_y_g - CIE_y_b - CIE_x_b*CIE_y_g + CIE_y_b*CIE_x_g)/CIE_C_rD,
	 (CIE_x_b - CIE_x_g - CIE_x_b*CIE_y_g + CIE_x_g*CIE_y_b)/CIE_C_rD,
	 (CIE_x_g*CIE_y_b - CIE_x_b*CIE_y_g)/CIE_C_rD},
	{(CIE_y_b - CIE_y_r - CIE_y_b*CIE_x_r + CIE_y_r*CIE_x_b)/CIE_C_gD,
	 (CIE_x_r - CIE_x_b - CIE_x_r*CIE_y_b + CIE_x_b*CIE_y_r)/CIE_C_gD,
	 (CIE_x_b*CIE_y_r - CIE_x_r*CIE_y_b)/CIE_C_gD},
	{(CIE_y_r - CIE_y_g - CIE_y_r*CIE_x_g + CIE_y_g*CIE_x_r)/CIE_C_bD,
	 (CIE_x_g - CIE_x_r - CIE_x_g*CIE_y_r + CIE_x_r*CIE_y_g)/CIE_C_bD,
	 (CIE_x_r*CIE_y_g - CIE_x_g*CIE_y_r)/CIE_C_bD}
};



spec_rgb(col, s, e)		/* compute RGB color from spectral range */
COLOR  col;
int  s, e;
{
	COLOR  ciecolor;

	spec_cie(ciecolor, s, e);
	cie_rgb(col, ciecolor);
}


spec_cie(col, s, e)		/* compute a color from a spectral range */
COLOR  col;		/* returned color */
int  s, e;		/* starting and ending wavelengths */
{
	register int  i, d, r;
	
	s -= STARTWL;
	if (s < 0)
		s = 0;

	e -= STARTWL;
	if (e <= s) {
		col[RED] = col[GRN] = col[BLU] = 0.0;
		return;
	}
	if (e >= INCWL*(NINC - 1))
		e = INCWL*(NINC - 1) - 1;

	d = e / INCWL;			/* interpolate values */
	r = e % INCWL;
	for (i = 0; i < 3; i++)
		col[i] = chroma[i][d]*(INCWL - r) + chroma[i][d + 1]*r;

	d = s / INCWL;
	r = s % INCWL;
	for (i = 0; i < 3; i++)
		col[i] -= chroma[i][d]*(INCWL - r) - chroma[i][d + 1]*r;

	col[RED] = (col[RED] + 0.5) / (256*INCWL);
	col[GRN] = (col[GRN] + 0.5) / (256*INCWL);
	col[BLU] = (col[BLU] + 0.5) / (256*INCWL);
}


cie_rgb(rgbcolor, ciecolor)		/* convert CIE to RGB */
register COLOR  rgbcolor, ciecolor;
{
	register int  i;

	for (i = 0; i < 3; i++) {
		rgbcolor[i] =	xyz2rgbmat[i][0]*ciecolor[0] +
				xyz2rgbmat[i][1]*ciecolor[1] +
				xyz2rgbmat[i][2]*ciecolor[2] ;
		if (rgbcolor[i] < 0.0)
			rgbcolor[i] = 0.0;
	}
}
