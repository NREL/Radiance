/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Routines for tone-mapping LogLuv encoded pixels.
 */

#include <stdio.h>
#include <math.h>
#include "tiffio.h"
#include "tmprivat.h"
#include "uvcode.h"

#ifndef BSD
#define bzero(d,n)		(void)memset(d,0,n)
#endif
#define uvflgop(p,uv,op)	((p)->rgbflg[(uv)>>5] op (1L<<((uv)&0x1f)))
#define isuvset(p,uv)		uvflgop(p,uv,&)
#define setuv(p,uv)		uvflgop(p,uv,|=)
#define clruv(p,uv)		uvflgop(p,uv,&=~)
#define clruvall(p)		bzero((MEM_PTR)(p)->rgbflg,sizeof((p)->rgbflg))

#define U_NEU			0.210526316
#define V_NEU			0.473684211

static MEM_PTR	luv32Init();
static void	luv32NewSpace();
static MEM_PTR	luv24Init();
static void	luv24NewSpace();
extern void	free();

typedef struct {
	int	offset;			/* computed luminance offset */
	BYTE	rgbval[1<<16][3];	/* computed RGB value for given uv */
	uint32	rgbflg[1<<(16-5)];	/* flags for computed values */
} LUV32DATA;			/* LogLuv 32-bit conversion data */

#define UVSCALE			410.
#define UVNEU			((int)(UVSCALE*U_NEU)<<8 \
					| (int)(UVSCALE*V_NEU))

static struct tmPackage  luv32Pkg = {	/* 32-bit package functions */
	luv32Init, luv32NewSpace, free
};
static int	luv32Reg = -1;		/* 32-bit package reg. number */

typedef struct {
	int	offset;			/* computed luminance offset */
	BYTE	rgbval[1<<14][3];	/* computed rgb value for uv index */
	uint32	rgbflg[1<<(14-5)];	/* flags for computed values */
} LUV24DATA;			/* LogLuv 24-bit conversion data */

static struct tmPackage  luv24Pkg = {	/* 24-bit package functions */
	luv24Init, luv24NewSpace, free
};
static int	luv24Reg = -1;		/* 24-bit package reg. number */

static int	uv14neu = -1;		/* neutral index for 14-bit (u',v') */


static
uv2rgb(rgb, tm, uvp)			/* compute RGB from uv coordinate */
BYTE	rgb[3];
register struct tmStruct	*tm;
double	uvp[2];
{	/* Should check that tm->inppri==TM_XYZPRIM beforehand... */
	double	d, x, y;
	COLR	XYZ, RGB;
					/* convert to XYZ */
	d = 1./(6.*uvp[0] - 16.*uvp[1] + 12.);
	x = 9.*uvp[0] * d;
	y = 4.*uvp[1] * d;
	XYZ[CIEY] = 1./tm->inpsf;
	XYZ[CIEX] = x/y * XYZ[CIEY];
	XYZ[CIEZ] = (1.-x-y)/y * XYZ[CIEY];
					/* convert to RGB and clip */
	colortrans(RGB, tm->cmat, XYZ);
	clipgamut(RGB, XYZ[CIEY], CGAMUT_LOWER, cblack, cwhite);
					/* perform final scaling & gamma */
	d = tm->clf[RED] * RGB[RED];
	rgb[RED] = d>=.999 ? 255 : (int)(256.*pow(d, 1./tm->mongam));
	d = tm->clf[GRN] * RGB[GRN];
	rgb[GRN] = d>=.999 ? 255 : (int)(256.*pow(d, 1./tm->mongam));
	d = tm->clf[BLU] * RGB[BLU];
	rgb[BLU] = d>=.999 ? 255 : (int)(256.*pow(d, 1./tm->mongam));
}


static TMbright
compmeshift(li, uvp)			/* compute mesopic color shift */
TMbright	li;		/* encoded world luminance */
double	uvp[2];			/* world (u',v') -> returned desaturated */
{
	/* UNIMPLEMENTED */
	return(li);
}


static int
uvpencode(uvp)			/* encode (u',v') coordinates */
double	uvp[2];
{
	register int	vi, ui;

	if (uvp[1] < UV_VSTART)
		return(-1);
	vi = (uvp[1] - UV_VSTART)*(1./UV_SQSIZ);
	if (vi >= UV_NVS)
		return(-1);
	if (uvp[0] < uv_row[vi].ustart)
		return(-1);
	ui = (uvp[0] - uv_row[vi].ustart)*(1./UV_SQSIZ);
	if (ui >= uv_row[vi].nus)
		return(-1);
	return(uv_row[vi].ncum + ui);
}


static int
uvpdecode(uvp, c)		/* decode (u',v') index */
double	uvp[2];
int	c;
{
	int	upper, lower;
	register int	ui, vi;

	if (c < 0 || c >= UV_NDIVS)
		return(-1);
	lower = 0;			/* binary search */
	upper = UV_NVS;
	do {
		vi = (lower + upper) >> 1;
		ui = c - uv_row[vi].ncum;
		if (ui > 0)
			lower = vi;
		else if (ui < 0)
			upper = vi;
		else
			break;
	} while (upper - lower > 1);
	vi = lower;
	ui = c - uv_row[vi].ncum;
	uvp[0] = uv_row[vi].ustart + (ui+.5)*UV_SQSIZ;
	uvp[1] = UV_VSTART + (vi+.5)*UV_SQSIZ;
	return(0);
}


int
tmCvLuv32(ls, cs, luvs, len)		/* convert raw 32-bit LogLuv values */
TMbright	*ls;
BYTE	*cs;
uint32	*luvs;
int	len;
{
	static char	funcName[] = "tmCvLuv32";
	double	uvp[2];
	register LUV32DATA	*ld;
	register int	i, j;
					/* check arguments */
	if (tmTop == NULL)
		returnErr(TM_E_TMINVAL);
	if (ls == NULL | luvs == NULL | len < 0)
		returnErr(TM_E_ILLEGAL);
					/* check package registration */
	if (luv32Reg < 0 && (luv32Reg = tmRegPkg(&luv32Pkg)) < 0)
		returnErr(TM_E_CODERR1);
					/* get package data */
	if ((ld = (LUV32DATA *)tmPkgData(tmTop,luv32Reg)) == NULL)
		returnErr(TM_E_NOMEM);
					/* convert each pixel */
	for (i = len; i--; ) {
		j = luvs[i] >> 16;		/* get luminance */
		if (j & 0x8000)			/* negative luminance */
			ls[i] = MINBRT-1;	/* assign bogus value */
		else				/* else convert to lnL */
			ls[i] = (BRT2SCALE*j >> 8) - ld->offset;
		if (cs == TM_NOCHROM)		/* no color? */
			continue;
						/* get chrominance */
		if (tmTop->flags & TM_F_MESOPIC && ls[i] < BMESUPPER) {
			uvp[0] = 1./UVSCALE*((luvs[i]>>8 & 0xff) + .5);
			uvp[1] = 1./UVSCALE*((luvs[i] & 0xff) + .5);
			ls[i] = compmeshift(ls[i], uvp);
			j = tmTop->flags & TM_F_BW || ls[i] < BMESLOWER
					? UVNEU
					: (int)(uvp[0]*UVSCALE)<<8
						| (int)(uvp[1]*UVSCALE);
		} else {
			j = tmTop->flags&TM_F_BW ? UVNEU : luvs[i]&0xffff;
		}
		if (!isuvset(ld, j)) {
			uvp[0] = 1./UVSCALE*((luvs[i]>>8 & 0xff) + .5);
			uvp[1] = 1./UVSCALE*((luvs[i] & 0xff) + .5);
			uv2rgb(ld->rgbval[j], tmTop, uvp);
			setuv(ld, j);
		}
		cs[3*i  ] = ld->rgbval[j][RED];
		cs[3*i+1] = ld->rgbval[j][GRN];
		cs[3*i+2] = ld->rgbval[j][BLU];
	}
	returnOK;
}


int
tmCvLuv24(ls, cs, luvs, len)		/* convert raw 24-bit LogLuv values */
TMbright	*ls;
BYTE	*cs;
uint32	*luvs;
int	len;
{
	char	funcName[] = "tmCvLuv24";
	double	uvp[2];
	register LUV24DATA	*ld;
	register int	i, j;
					/* check arguments */
	if (tmTop == NULL)
		returnErr(TM_E_TMINVAL);
	if (ls == NULL | luvs == NULL | len < 0)
		returnErr(TM_E_ILLEGAL);
					/* check package registration */
	if (luv24Reg < 0 && (luv24Reg = tmRegPkg(&luv24Pkg)) < 0)
		returnErr(TM_E_CODERR1);
					/* get package data */
	if ((ld = (LUV24DATA *)tmPkgData(tmTop,luv24Reg)) == NULL)
		returnErr(TM_E_NOMEM);
					/* convert each pixel */
	for (i = len; i--; ) {
		j = luvs[i] >> 14;		/* get luminance */
		ls[i] = (BRT2SCALE*j >> 6) - ld->offset;
		if (cs == TM_NOCHROM)		/* no color? */
			continue;
						/* get chrominance */
		if (tmTop->flags & TM_F_MESOPIC && ls[i] < BMESUPPER) {
			if (uvpdecode(uvp, luvs[i]&0x3fff) < 0) {
				uvp[0] = U_NEU;		/* should barf? */
				uvp[1] = V_NEU;
			}
			ls[i] = compmeshift(ls[i], uvp);
			if (tmTop->flags & TM_F_BW || ls[i] < BMESLOWER
					|| (j = uvpencode(uvp)) < 0)
				j = uv14neu;
		} else {
			j = tmTop->flags&TM_F_BW ? uv14neu : luvs[i]&0x3fff;
		}
		if (!isuvset(ld, j)) {
			if (uvpdecode(uvp, j) < 0) {
				uvp[0] = U_NEU;
				uvp[1] = V_NEU;
			}
			uv2rgb(ld->rgbval[j], tmTop, uvp);
			setuv(ld, j);
		}
		cs[3*i  ] = ld->rgbval[j][RED];
		cs[3*i+1] = ld->rgbval[j][GRN];
		cs[3*i+2] = ld->rgbval[j][BLU];
	}
	returnOK;
}


int
tmCvL16(ls, l16s, len)			/* convert 16-bit LogL values */
TMbright	*ls;
uint16	*l16s;
int	len;
{
	static char	funcName[] = "tmCvL16";
	static double	lastsf;
	static int	offset;
	register int	i;
					/* check arguments */
	if (tmTop == NULL)
		returnErr(TM_E_TMINVAL);
	if (ls == NULL | l16s == NULL | len < 0)
		returnErr(TM_E_ILLEGAL);
					/* check scaling offset */
	if (!FEQ(tmTop->inpsf, lastsf)) {
		offset = BRT2SCALE*64;
		if (tmTop->inpsf > 1.0001)
			offset -= (int)(TM_BRTSCALE*log(tmTop->inpsf)+.5);
		else if (tmTop->inpsf < 0.9999)
			offset -= (int)(TM_BRTSCALE*log(tmTop->inpsf)-.5);
		lastsf = tmTop->inpsf;
	}
					/* convert each pixel */
	for (i = len; i--; ) {
		if (l16s[i] & 0x8000)		/* negative luminance */
			ls[i] = MINBRT-1;	/* assign bogus value */
		else				/* else convert to lnL */
			ls[i] = (BRT2SCALE*l16s[i] >> 8) - offset;
	}
	returnOK;
}


static void
luv32NewSpace(tm)		/* initialize 32-bit LogLuv color space */
struct tmStruct	*tm;
{
	register LUV32DATA	*ld;

	if (tm->inppri != TM_XYZPRIM) {		/* panic time! */
		fputs("Improper input color space in luv32NewSpace!\n", stderr);
		exit(1);
	}
	ld = (LUV32DATA *)tm->pd[luv32Reg];
	ld->offset = BRT2SCALE*64;
	if (tm->inpsf > 1.0001)
		ld->offset -= (int)(TM_BRTSCALE*log(tmTop->inpsf)+.5);
	else if (tm->inpsf < 0.9999)
		ld->offset -= (int)(TM_BRTSCALE*log(tmTop->inpsf)-.5);
	clruvall(ld);
}


static MEM_PTR
luv32Init(tm)			/* allocate data for 32-bit LogLuv decoder */
struct tmStruct	*tm;
{
	register LUV32DATA	*ld;

	ld = (LUV32DATA *)malloc(sizeof(LUV32DATA));
	if (ld == NULL)
		return(NULL);
	tm->pd[luv32Reg] = (MEM_PTR)ld;
	luv32NewSpace(tm);
	return((MEM_PTR)ld);
}


static void
luv24NewSpace(tm)		/* initialize 24-bit LogLuv color space */
struct tmStruct	*tm;
{
	register LUV24DATA	*ld;
	double	uvp[2];

	if (tm->inppri != TM_XYZPRIM) {		/* panic time! */
		fputs("Improper input color space in luv24NewSpace!\n", stderr);
		exit(1);
	}
	ld = (LUV24DATA *)tm->pd[luv24Reg];
	ld->offset = BRT2SCALE*12;
	if (tm->inpsf > 1.0001)
		ld->offset -= (int)(TM_BRTSCALE*log(tmTop->inpsf)+.5);
	else if (tm->inpsf < 0.9999)
		ld->offset -= (int)(TM_BRTSCALE*log(tmTop->inpsf)-.5);
	clruvall(ld);
}


static MEM_PTR
luv24Init(tm)			/* allocate data for 24-bit LogLuv decoder */
struct tmStruct	*tm;
{
	register LUV24DATA	*ld;

	ld = (LUV24DATA *)malloc(sizeof(LUV24DATA));
	if (ld == NULL)
		return(NULL);
	tm->pd[luv24Reg] = (MEM_PTR)ld;
	if (uv14neu < 0) {		/* initialize neutral color index */
		double	uvp[2];
		uvp[0] = U_NEU; uvp[1] = V_NEU;
		uv14neu = uvpencode(uvp);
	}
	luv24NewSpace(tm);
	return((MEM_PTR)ld);
}
