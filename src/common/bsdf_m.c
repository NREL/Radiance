#ifndef lint
static const char RCSid[] = "$Id: bsdf_m.c,v 3.42 2021/03/27 20:08:30 greg Exp $";
#endif
/*
 *  bsdf_m.c
 *  
 *  Definitions supporting BSDF matrices
 *
 *  Created by Greg Ward on 2/2/11.
 *  Copyright 2011 Anyhere Software. All rights reserved.
 *
 */

#define	_USE_MATH_DEFINES
#include "rtio.h"
#include <math.h>
#include <ctype.h>
#include "ezxml.h"
#include "bsdf.h"
#include "bsdf_m.h"

/* Function return codes */
#define	RC_GOOD		1
#define RC_FAIL		0
#define RC_FORMERR	(-1)
#define RC_DATERR	(-2)
#define RC_UNSUPP	(-3)
#define RC_INTERR	(-4)
#define RC_MEMERR	(-5)

ANGLE_BASIS	abase_list[MAXABASES] = {
	{
		"LBNL/Klems Full", 145,
		{ {0., 1},
		{5., 8},
		{15., 16},
		{25., 20},
		{35., 24},
		{45., 24},
		{55., 24},
		{65., 16},
		{75., 12},
		{90., 0} }
	}, {
		"LBNL/Klems Half", 73,
		{ {0., 1},
		{6.5, 8},
		{19.5, 12},
		{32.5, 16},
		{46.5, 20},
		{61.5, 12},
		{76.5, 4},
		{90., 0} }
	}, {
		"LBNL/Klems Quarter", 41,
		{ {0., 1},
		{9., 8},
		{27., 12},
		{46., 12},
		{66., 8},
		{90., 0} }
	}
};

int		nabases = 3;		/* current number of defined bases */

C_COLOR		mtx_RGB_prim[3];	/* our RGB primaries  */
float		mtx_RGB_coef[3];	/* corresponding Y coefficients */

enum {mtx_Y, mtx_X, mtx_Z};		/* matrix components (mtx_Y==0) */

/* check if two real values are near enough to equal */
static int
fequal(double a, double b)
{
	if (b != 0)
		a = a/b - 1.;
	return (a <= 1e-6) & (a >= -1e-6);
}

/* convert error to standard BSDF code */
static SDError
convert_errcode(int ec)
{
	switch (ec) {
	case RC_GOOD:
		return SDEnone;
	case RC_FORMERR:
		return SDEformat;
	case RC_DATERR:
		return SDEdata;
	case RC_UNSUPP:
		return SDEsupport;
	case RC_INTERR:
		return SDEinternal;
	case RC_MEMERR:
		return SDEmemory;
	}
	return SDEunknown;
}

/* allocate a BSDF matrix of the given size */
static SDMat *
SDnewMatrix(int ni, int no)
{
	SDMat		*sm;

	if ((ni <= 0) | (no <= 0)) {
		strcpy(SDerrorDetail, "Empty BSDF matrix request");
		return NULL;
	}
	sm = (SDMat *)malloc(sizeof(SDMat) + (ni*no - 1)*sizeof(float));
	if (sm == NULL) {
		sprintf(SDerrorDetail, "Cannot allocate %dx%d BSDF matrix",
				ni, no);
		return NULL;
	}
	memset(sm, 0, sizeof(SDMat)-sizeof(float));
	sm->ninc = ni;
	sm->nout = no;
	
	return sm;
}

/* Free a BSDF matrix */
void
SDfreeMatrix(void *ptr)
{
	SDMat	*mp = (SDMat *)ptr;

	if (mp->chroma != NULL) free(mp->chroma);
	free(ptr);
}

/* compute square of real value */
static double sq(double x) { return x*x; }

/* Get vector for this angle basis index (front exiting) */
int
fo_getvec(FVECT v, double ndxr, void *p)
{
	ANGLE_BASIS	*ab = (ANGLE_BASIS *)p;
	int		ndx = (int)ndxr;
	double		randX = ndxr - ndx;
	double		rx[2];
	int		li;
	double		azi, d;
	
	if ((ndxr < 0) | (ndx >= ab->nangles))
		return RC_FAIL;
	for (li = 0; ndx >= ab->lat[li].nphis; li++)
		ndx -= ab->lat[li].nphis;
	SDmultiSamp(rx, 2, randX);
	d = (1. - rx[0])*sq(cos(M_PI/180.*ab->lat[li].tmin)) +
		rx[0]*sq(cos(M_PI/180.*ab->lat[li+1].tmin));
	v[2] = d = sqrt(d);	/* cos(pol) */
	azi = 2.*M_PI*(ndx + rx[1] - .5)/ab->lat[li].nphis;
	d = sqrt(1. - d*d);	/* sin(pol) */
	v[0] = cos(azi)*d;
	v[1] = sin(azi)*d;
	return RC_GOOD;
}

/* Get index corresponding to the given vector (front exiting) */
int
fo_getndx(const FVECT v, void *p)
{
	ANGLE_BASIS	*ab = (ANGLE_BASIS *)p;
	int	li, ndx;
	double	pol, azi;

	if (v == NULL)
		return -1;
	if ((v[2] < 0) | (v[2] > 1.))
		return -1;
	pol = 180.0/M_PI*Acos(v[2]);
	azi = 180.0/M_PI*atan2(v[1], v[0]);
	if (azi < 0.0) azi += 360.0;
	for (li = 1; ab->lat[li].tmin <= pol; li++)
		if (!ab->lat[li].nphis)
			return -1;
	--li;
	ndx = (int)((1./360.)*azi*ab->lat[li].nphis + 0.5);
	if (ndx >= ab->lat[li].nphis) ndx = 0;
	while (li--)
		ndx += ab->lat[li].nphis;
	return ndx;
}

/* Get projected solid angle for this angle basis index (universal) */
double
io_getohm(int ndx, void *p)
{
	static void	*last_p = NULL;
	static int	last_li = -1;
	static double	last_ohm;
	ANGLE_BASIS	*ab = (ANGLE_BASIS *)p;
	int		li;
	double		theta, theta1;
	
	if ((ndx < 0) | (ndx >= ab->nangles))
		return -1.;
	for (li = 0; ndx >= ab->lat[li].nphis; li++)
		ndx -= ab->lat[li].nphis;
	if ((p == last_p) & (li == last_li))		/* cached latitude? */
		return last_ohm;
	last_p = p;
	last_li = li;
	theta = M_PI/180. * ab->lat[li].tmin;
	theta1 = M_PI/180. * ab->lat[li+1].tmin;
	return last_ohm = M_PI*(sq(cos(theta)) - sq(cos(theta1))) /
				(double)ab->lat[li].nphis;
}

/* Get vector for this angle basis index (back incident) */
int
bi_getvec(FVECT v, double ndxr, void *p)
{
	if (!fo_getvec(v, ndxr, p))
		return RC_FAIL;

	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];

	return RC_GOOD;
}

/* Get index corresponding to the vector (back incident) */
int
bi_getndx(const FVECT v, void *p)
{
	FVECT  v2;
	
	v2[0] = -v[0];
	v2[1] = -v[1];
	v2[2] = -v[2];

	return fo_getndx(v2, p);
}

/* Get vector for this angle basis index (back exiting) */
int
bo_getvec(FVECT v, double ndxr, void *p)
{
	if (!fo_getvec(v, ndxr, p))
		return RC_FAIL;

	v[2] = -v[2];

	return RC_GOOD;
}

/* Get index corresponding to the vector (back exiting) */
int
bo_getndx(const FVECT v, void *p)
{
	FVECT  v2;
	
	v2[0] = v[0];
	v2[1] = v[1];
	v2[2] = -v[2];

	return fo_getndx(v2, p);
}

/* Get vector for this angle basis index (front incident) */
int
fi_getvec(FVECT v, double ndxr, void *p)
{
	if (!fo_getvec(v, ndxr, p))
		return RC_FAIL;

	v[0] = -v[0];
	v[1] = -v[1];

	return RC_GOOD;
}

/* Get index corresponding to the vector (front incident) */
int
fi_getndx(const FVECT v, void *p)
{
	FVECT  v2;
	
	v2[0] = -v[0];
	v2[1] = -v[1];
	v2[2] = v[2];

	return fo_getndx(v2, p);
}

/* Get color or grayscale value for BSDF for the given direction pair */
int
mBSDF_color(float coef[], const SDMat *dp, int i, int o)
{
	C_COLOR	cxy;

	coef[0] = mBSDF_value(dp, o, i);
	if (dp->chroma == NULL)
		return 1;	/* grayscale */

	c_decodeChroma(&cxy, mBSDF_chroma(dp,o,i));
	c_toSharpRGB(&cxy, coef[0], coef);
	coef[0] *= mtx_RGB_coef[0];
	coef[1] *= mtx_RGB_coef[1];
	coef[2] *= mtx_RGB_coef[2];
	return 3;		/* RGB color */
}

/* load custom BSDF angle basis */
static int
load_angle_basis(ezxml_t wab)
{
	char	*abname = ezxml_txt(ezxml_child(wab, "AngleBasisName"));
	ezxml_t	wbb;
	int	i;
	
	if (!abname || !*abname)
		return RC_FAIL;
	for (i = nabases; i--; )
		if (!strcasecmp(abname, abase_list[i].name))
			return RC_GOOD;	/* assume it's the same */
	if (nabases >= MAXABASES) {
		sprintf(SDerrorDetail, "Out of angle bases reading '%s'",
				abname);
		return RC_INTERR;
	}
	strcpy(abase_list[nabases].name, abname);
	abase_list[nabases].nangles = 0;
	for (i = 0, wbb = ezxml_child(wab, "AngleBasisBlock");
			wbb != NULL; i++, wbb = wbb->next) {
		if (i >= MAXLATS) {
			sprintf(SDerrorDetail, "Too many latitudes for '%s'",
				abname);
			return RC_INTERR;
		}
		abase_list[nabases].lat[i+1].tmin = atof(ezxml_txt(
				ezxml_child(ezxml_child(wbb,
					"ThetaBounds"), "UpperTheta")));
		if (!i)
			abase_list[nabases].lat[0].tmin = 0;
		else if (!fequal(atof(ezxml_txt(ezxml_child(ezxml_child(wbb,
					"ThetaBounds"), "LowerTheta"))),
				abase_list[nabases].lat[i].tmin)) {
			sprintf(SDerrorDetail, "Theta values disagree in '%s'",
					abname);
			return RC_DATERR;
		}
		abase_list[nabases].nangles +=
			abase_list[nabases].lat[i].nphis =
				atoi(ezxml_txt(ezxml_child(wbb, "nPhis")));
		if (abase_list[nabases].lat[i].nphis <= 0 ||
				(abase_list[nabases].lat[i].nphis == 1 &&
				abase_list[nabases].lat[i].tmin > FTINY)) {
			sprintf(SDerrorDetail, "Illegal phi count in '%s'",
					abname);
			return RC_DATERR;
		}
	}
	abase_list[nabases++].lat[i].nphis = 0;
	return RC_GOOD;
}

/* compute min. proj. solid angle and max. direct hemispherical scattering */
static int
get_extrema(SDSpectralDF *df)
{
	SDMat	*dp = (SDMat *)df->comp[0].dist;
	double	*ohma;
	int	i, o;
					/* initialize extrema */
	df->minProjSA = M_PI;
	df->maxHemi = .0;
	ohma = (double *)malloc(dp->nout*sizeof(double));
	if (ohma == NULL)
		return RC_MEMERR;
					/* get outgoing solid angles */
	for (o = dp->nout; o--; )
		if ((ohma[o] = mBSDF_outohm(dp,o)) < df->minProjSA)
			df->minProjSA = ohma[o];
					/* compute hemispherical sums */
	for (i = dp->ninc; i--; ) {
		double	hemi = .0;
		for (o = dp->nout; o--; )
			hemi += ohma[o] * mBSDF_value(dp, o, i);
		if (hemi > df->maxHemi)
			df->maxHemi = hemi;
	}
	free(ohma);
					/* need incoming solid angles, too? */
	if ((dp->ib_ohm != dp->ob_ohm) | (dp->ib_priv != dp->ob_priv)) {
		double	ohm;
		for (i = dp->ninc; i--; )
			if ((ohm = mBSDF_incohm(dp,i)) < df->minProjSA)
				df->minProjSA = ohm;
	}
	return (df->maxHemi <= 1.01);
}

/* load BSDF distribution for this wavelength */
static int
load_bsdf_data(SDData *sd, ezxml_t wdb, int ct, int rowinc)
{
	SDSpectralDF	*df;
	SDMat		*dp;
	char		*sdata;
	int		inbi, outbi;
	int		i;
					/* allocate BSDF component */
	sdata = ezxml_txt(ezxml_child(wdb, "WavelengthDataDirection"));
	if (!sdata)
		return RC_FAIL;
	/*
	 * Remember that front and back are reversed from WINDOW 6 orientations
	 */
	if (!strcasecmp(sdata, "Transmission Front")) {
		if (sd->tb == NULL && (sd->tb = SDnewSpectralDF(3)) == NULL)
			return RC_MEMERR;
		df = sd->tb;
	} else if (!strcasecmp(sdata, "Transmission Back")) {
		if (sd->tf == NULL && (sd->tf = SDnewSpectralDF(3)) == NULL)
			return RC_MEMERR;
		df = sd->tf;
	} else if (!strcasecmp(sdata, "Reflection Front")) {
		if (sd->rb == NULL && (sd->rb = SDnewSpectralDF(3)) == NULL)
			return RC_MEMERR;
		df = sd->rb;
	} else if (!strcasecmp(sdata, "Reflection Back")) {
		if (sd->rf == NULL && (sd->rf = SDnewSpectralDF(3)) == NULL)
			return RC_MEMERR;
		df = sd->rf;
	} else
		return RC_FAIL;
					/* free previous matrix if any */
	if (df->comp[ct].dist != NULL) {
		SDfreeMatrix(df->comp[ct].dist);
		df->comp[ct].dist = NULL;
	}
					/* get angle bases */
	sdata = ezxml_txt(ezxml_child(wdb,"ColumnAngleBasis"));
	if (!sdata || !*sdata) {
		sprintf(SDerrorDetail, "Missing column basis for BSDF '%s'",
				sd->name);
		return RC_FORMERR;
	}
	for (inbi = nabases; inbi--; )
		if (!strcasecmp(sdata, abase_list[inbi].name))
			break;
	if (inbi < 0) {
		sprintf(SDerrorDetail, "Undefined ColumnAngleBasis '%s'", sdata);
		return RC_FORMERR;
	}
	sdata = ezxml_txt(ezxml_child(wdb,"RowAngleBasis"));
	if (!sdata || !*sdata) {
		sprintf(SDerrorDetail, "Missing row basis for BSDF '%s'",
				sd->name);
		return RC_FORMERR;
	}
	for (outbi = nabases; outbi--; )
		if (!strcasecmp(sdata, abase_list[outbi].name))
			break;
	if (outbi < 0) {
		sprintf(SDerrorDetail, "Undefined RowAngleBasis '%s'", sdata);
		return RC_FORMERR;
	}
					/* allocate BSDF matrix */
	dp = SDnewMatrix(abase_list[inbi].nangles, abase_list[outbi].nangles);
	if (dp == NULL)
		return RC_MEMERR;
	dp->ib_priv = &abase_list[inbi];
	dp->ob_priv = &abase_list[outbi];
	if (df == sd->tf) {
		dp->ib_vec = &fi_getvec;
		dp->ib_ndx = &fi_getndx;
		dp->ob_vec = &bo_getvec;
		dp->ob_ndx = &bo_getndx;
	} else if (df == sd->tb) {
		dp->ib_vec = &bi_getvec;
		dp->ib_ndx = &bi_getndx;
		dp->ob_vec = &fo_getvec;
		dp->ob_ndx = &fo_getndx;
	} else if (df == sd->rf) {
		dp->ib_vec = &fi_getvec;
		dp->ib_ndx = &fi_getndx;
		dp->ob_vec = &fo_getvec;
		dp->ob_ndx = &fo_getndx;
	} else /* df == sd->rb */ {
		dp->ib_vec = &bi_getvec;
		dp->ib_ndx = &bi_getndx;
		dp->ob_vec = &bo_getvec;
		dp->ob_ndx = &bo_getndx;
	}
	dp->ib_ohm = &io_getohm;
	dp->ob_ohm = &io_getohm;
	df->comp[ct].dist = dp;
	df->comp[ct].func = &SDhandleMtx;
					/* read BSDF data */
	sdata = ezxml_txt(ezxml_child(wdb, "ScatteringData"));
	if (!sdata || !*sdata) {
		sprintf(SDerrorDetail, "Missing BSDF ScatteringData in '%s'",
				sd->name);
		return RC_FORMERR;
	}
	for (i = 0; i < dp->ninc*dp->nout; i++) {
		char	*sdnext = fskip(sdata);
		double	val;
		if (sdnext == NULL) {
			sprintf(SDerrorDetail,
				"Bad/missing BSDF ScatteringData in '%s'",
					sd->name);
			return RC_FORMERR;
		}
		while (isspace(*sdnext))
			sdnext++;
		if (*sdnext == ',') sdnext++;
		if ((val = atof(sdata)) < 0)
			val = 0;	/* don't allow negative values */
		if (rowinc) {
			int	r = i/dp->nout;
			int	c = i - r*dp->nout;
			mBSDF_value(dp,c,r) = val;
		} else
			dp->bsdf[i] = val;
		sdata = sdnext;
	}
	return (ct == mtx_Y) ? get_extrema(df) : RC_GOOD;
}

/* copy our RGB (x,y) primary chromaticities */
static void
copy_RGB_prims(C_COLOR cspec[])
{
	if (mtx_RGB_coef[1] < .001) {	/* need to initialize */
		int	i = 3;
		while (i--) {
			float	rgb[3];
			rgb[0] = rgb[1] = rgb[2] = .0f;
			rgb[i] = 1.f;
			mtx_RGB_coef[i] = c_fromSharpRGB(rgb, &mtx_RGB_prim[i]);
		}
	}
	memcpy(cspec, mtx_RGB_prim, sizeof(mtx_RGB_prim));
}

/* encode chromaticity if XYZ -- reduce to one channel in any case */
static SDSpectralDF *
encode_chroma(SDSpectralDF *df)
{
	SDMat	*mpx, *mpy, *mpz;
	int	n;

	if (df == NULL || df->ncomp != 3)
		return df;

	mpy = (SDMat *)df->comp[mtx_Y].dist;
	if (mpy == NULL) {
		free(df);
		return NULL;
	}
	mpx = (SDMat *)df->comp[mtx_X].dist;
	mpz = (SDMat *)df->comp[mtx_Z].dist;
	if (mpx == NULL || (mpx->ninc != mpy->ninc) | (mpx->nout != mpy->nout))
		goto done;
	if (mpz == NULL || (mpz->ninc != mpy->ninc) | (mpz->nout != mpy->nout))
		goto done;
	mpy->chroma = (C_CHROMA *)malloc(sizeof(C_CHROMA)*mpy->ninc*mpy->nout);
	if (mpy->chroma == NULL)
		goto done;		/* XXX punt */
					/* encode chroma values */
	for (n = mpy->ninc*mpy->nout; n--; ) {
		const double	sum = mpx->bsdf[n] + mpy->bsdf[n] + mpz->bsdf[n];
		C_COLOR		cxy;
		if (sum > .0)
			c_cset(&cxy, mpx->bsdf[n]/sum, mpy->bsdf[n]/sum);
		else
			c_cset(&cxy, 1./3., 1./3.);
		mpy->chroma[n] = c_encodeChroma(&cxy);
	}
done:					/* free X & Z channels */
	if (mpx != NULL) SDfreeMatrix(mpx);
	if (mpz != NULL) SDfreeMatrix(mpz);
	if (mpy->chroma == NULL)	/* grayscale after all? */
		df->comp[0].cspec[0] = c_dfcolor;
	else				/* else copy RGB primaries */
		copy_RGB_prims(df->comp[0].cspec);
	df->ncomp = 1;			/* return resized struct */
	return (SDSpectralDF *)realloc(df, sizeof(SDSpectralDF));
}

/* subtract minimum (diffuse) scattering amount from BSDF */
static double
subtract_min(C_COLOR *cs, SDMat *sm)
{
	const int	ncomp = 1 + 2*(sm->chroma != NULL);
	float		min_coef[3], ymin, coef[3];
	int		i, o, c;
	
	min_coef[0] = min_coef[1] = min_coef[2] = FHUGE;
	for (i = 0; i < sm->ninc; i++)
		for (o = 0; o < sm->nout; o++) {
			c = mBSDF_color(coef, sm, i, o);
			while (c--)
				if (coef[c] < min_coef[c])
					min_coef[c] = coef[c];
		}
	ymin = 0;
	for (c = ncomp; c--; )
		ymin += min_coef[c];
	if (ymin <= .01/M_PI)		/* not worth bothering about? */
		return .0;
	if (ncomp == 1) {		/* subtract grayscale minimum */
		for (i = sm->ninc*sm->nout; i--; )
			sm->bsdf[i] -= ymin;
		*cs = c_dfcolor;
		return M_PI*ymin;
	}
					/* else subtract colored minimum */
	for (i = 0; i < sm->ninc; i++)
		for (o = 0; o < sm->nout; o++) {
			C_COLOR	cxy;
			c = mBSDF_color(coef, sm, i, o);
			while (c--)
				coef[c] = (coef[c] - min_coef[c]) /
						mtx_RGB_coef[c];
			if (c_fromSharpRGB(coef, &cxy) > 1e-5)
				mBSDF_chroma(sm,o,i) = c_encodeChroma(&cxy);
			mBSDF_value(sm,o,i) -= ymin;
		}
					/* return colored minimum */
	for (i = 3; i--; )
		coef[i] = min_coef[i]/mtx_RGB_coef[i];
	c_fromSharpRGB(coef, cs);

	return M_PI*ymin;
}

/* Extract and separate diffuse portion of BSDF & convert color */
static SDSpectralDF *
extract_diffuse(SDValue *dv, SDSpectralDF *df)
{

	df = encode_chroma(df);		/* reduce XYZ to Y + chroma */
	if (df == NULL || df->ncomp <= 0) {
		dv->spec = c_dfcolor;
		dv->cieY = .0;
		return df;
	}
					/* subtract minimum value */
	dv->cieY = subtract_min(&dv->spec, (SDMat *)df->comp[0].dist);
	df->maxHemi -= dv->cieY;	/* adjust maximum hemispherical */
				
	c_ccvt(&dv->spec, C_CSXY);	/* make sure (x,y) is set */
	return df;
}

/* Load a BSDF matrix from an open XML file */
SDError
SDloadMtx(SDData *sd, ezxml_t wtl)
{
	ezxml_t		wld, wdb;
	int		rowIn;
	char		*txt;
	int		rval;
					/* basic checks and data ordering */
	txt = ezxml_txt(ezxml_child(ezxml_child(wtl,
			"DataDefinition"), "IncidentDataStructure"));
	if (txt == NULL || !*txt) {
		sprintf(SDerrorDetail,
			"BSDF \"%s\": missing IncidentDataStructure",
				sd->name);
		return SDEformat;
	}
	if (!strcasecmp(txt, "Rows"))
		rowIn = 1;
	else if (!strcasecmp(txt, "Columns"))
		rowIn = 0;
	else {
		sprintf(SDerrorDetail,
			"BSDF \"%s\": unsupported IncidentDataStructure",
				sd->name);
		return SDEsupport;
	}
					/* get angle bases */
	for (wld = ezxml_child(ezxml_child(wtl, "DataDefinition"), "AngleBasis");
				wld != NULL; wld = wld->next) {
		rval = load_angle_basis(wld);
		if (rval < 0)
			return convert_errcode(rval);
	}
					/* load BSDF components */
	for (wld = ezxml_child(wtl, "WavelengthData");
				wld != NULL; wld = wld->next) {
		const char	*cnm = ezxml_txt(ezxml_child(wld,"Wavelength"));
		int		ct = -1;
		if (!strcasecmp(cnm, "Visible"))
			ct = mtx_Y;
		else if (!strcasecmp(cnm, "CIE-X"))
			ct = mtx_X;
		else if (!strcasecmp(cnm, "CIE-Z"))
			ct = mtx_Z;
		else
			continue;
		for (wdb = ezxml_child(wld, "WavelengthDataBlock");
					wdb != NULL; wdb = wdb->next)
			if ((rval = load_bsdf_data(sd, wdb, ct, rowIn)) < 0)
				return convert_errcode(rval);
	}
					/* separate diffuse components */
	sd->rf = extract_diffuse(&sd->rLambFront, sd->rf);
	sd->rb = extract_diffuse(&sd->rLambBack, sd->rb);
	sd->tf = extract_diffuse(&sd->tLambFront, sd->tf);
	if (sd->tb != NULL) {
		sd->tb = extract_diffuse(&sd->tLambBack, sd->tb);
		if (sd->tf == NULL)
			sd->tLambFront = sd->tLambBack;
	} else if (sd->tf != NULL)
		sd->tLambBack = sd->tLambFront;
					/* return success */
	return SDEnone;
}

/* Get Matrix BSDF value */
static int
SDgetMtxBSDF(float coef[SDmaxCh], const FVECT outVec,
				const FVECT inVec, SDComponent *sdc)
{
	const SDMat	*dp;
	int		i_ndx, o_ndx;
					/* check arguments */
	if ((coef == NULL) | (outVec == NULL) | (inVec == NULL) | (sdc == NULL)
			|| (dp = (SDMat *)sdc->dist) == NULL)
		return 0;
					/* get angle indices */
	i_ndx = mBSDF_incndx(dp, inVec);
	o_ndx = mBSDF_outndx(dp, outVec);
					/* try reciprocity if necessary */
	if ((i_ndx < 0) & (o_ndx < 0)) {
		i_ndx = mBSDF_incndx(dp, outVec);
		o_ndx = mBSDF_outndx(dp, inVec);
	}
	if ((i_ndx < 0) | (o_ndx < 0))
		return 0;		/* nothing from this component */

	return mBSDF_color(coef, dp, i_ndx, o_ndx);
}

/* Query solid angle for vector(s) */
static SDError
SDqueryMtxProjSA(double *psa, const FVECT v1, const RREAL *v2,
					int qflags, SDComponent *sdc)
{
	const SDMat	*dp;
	double		inc_psa, out_psa;
					/* check arguments */
	if ((psa == NULL) | (v1 == NULL) | (sdc == NULL) ||
			(dp = (SDMat *)sdc->dist) == NULL)
		return SDEargument;
	if (v2 == NULL)
		v2 = v1;
					/* get projected solid angles */
	out_psa = mBSDF_outohm(dp, mBSDF_outndx(dp, v1));
	inc_psa = mBSDF_incohm(dp, mBSDF_incndx(dp, v2));
	if ((v1 != v2) & (out_psa <= 0) & (inc_psa <= 0)) {
		inc_psa = mBSDF_outohm(dp, mBSDF_outndx(dp, v2));
		out_psa = mBSDF_incohm(dp, mBSDF_incndx(dp, v1));
	}

	switch (qflags) {		/* record based on flag settings */
	case SDqueryMax:
		if (inc_psa > psa[0])
			psa[0] = inc_psa;
		if (out_psa > psa[0])
			psa[0] = out_psa;
		break;
	case SDqueryMin+SDqueryMax:
		if (inc_psa > psa[1])
			psa[1] = inc_psa;
		if (out_psa > psa[1])
			psa[1] = out_psa;
		/* fall through */
	case SDqueryVal:
		if (qflags == SDqueryVal)
			psa[0] = M_PI;
		/* fall through */
	case SDqueryMin:
		if ((inc_psa > 0) & (inc_psa < psa[0]))
			psa[0] = inc_psa;
		if ((out_psa > 0) & (out_psa < psa[0]))
			psa[0] = out_psa;
		break;
	}
					/* make sure it's legal */
	return (psa[0] <= 0) ? SDEinternal : SDEnone;
}

/* Compute new cumulative distribution from BSDF */
static int
make_cdist(SDMatCDst *cd, const FVECT inVec, SDMat *dp, int rev)
{
	const unsigned	maxval = ~0;
	double		*cmtab, scale;
	int		o;

	cmtab = (double *)malloc((cd->calen+1)*sizeof(double));
	if (cmtab == NULL)
		return 0;
	cmtab[0] = .0;
	for (o = 0; o < cd->calen; o++) {
		if (rev)
			cmtab[o+1] = mBSDF_value(dp, cd->indx, o) *
					(*dp->ib_ohm)(o, dp->ib_priv);
		else
			cmtab[o+1] = mBSDF_value(dp, o, cd->indx) *
					(*dp->ob_ohm)(o, dp->ob_priv);
		cmtab[o+1] += cmtab[o];
	}
	cd->cTotal = cmtab[cd->calen];
	scale = (double)maxval / cd->cTotal;
	cd->carr[0] = 0;
	for (o = 1; o < cd->calen; o++)
		cd->carr[o] = scale*cmtab[o] + .5;
	cd->carr[cd->calen] = maxval;
	free(cmtab);
	return 1;
}

/* Get cumulative distribution for matrix BSDF */
static const SDCDst *
SDgetMtxCDist(const FVECT inVec, SDComponent *sdc)
{
	SDMat		*dp;
	int		reverse;
	SDMatCDst	myCD;
	SDMatCDst	*cd, *cdlast;
					/* check arguments */
	if ((inVec == NULL) | (sdc == NULL) ||
			(dp = (SDMat *)sdc->dist) == NULL)
		return NULL;
	memset(&myCD, 0, sizeof(myCD));
	myCD.indx = mBSDF_incndx(dp, inVec);
	if (myCD.indx >= 0) {
		myCD.ob_priv = dp->ob_priv;
		myCD.ob_vec = dp->ob_vec;
		myCD.calen = dp->nout;
		reverse = 0;
	} else {			/* try reciprocity */
		myCD.indx = mBSDF_outndx(dp, inVec);
		if (myCD.indx < 0)
			return NULL;
		myCD.ob_priv = dp->ib_priv;
		myCD.ob_vec = dp->ib_vec;
		myCD.calen = dp->ninc;
		reverse = 1;
	}
	cdlast = NULL;			/* check for it in cache list */
	/* PLACE MUTEX LOCK HERE FOR THREAD-SAFE */
	for (cd = (SDMatCDst *)sdc->cdList; cd != NULL;
					cdlast = cd, cd = cd->next)
		if (cd->indx == myCD.indx && (cd->calen == myCD.calen) &
					(cd->ob_priv == myCD.ob_priv) &
					(cd->ob_vec == myCD.ob_vec))
			break;
	if (cd == NULL) {		/* need to allocate new entry */
		cd = (SDMatCDst *)malloc(sizeof(SDMatCDst) +
					sizeof(myCD.carr[0])*myCD.calen);
		if (cd == NULL)
			return NULL;
		*cd = myCD;		/* compute cumulative distribution */
		if (!make_cdist(cd, inVec, dp, reverse)) {
			free(cd);
			return NULL;
		}
		cdlast = cd;
	}
	if (cdlast != NULL) {		/* move entry to head of cache list */
		cdlast->next = cd->next;
		cd->next = (SDMatCDst *)sdc->cdList;
		sdc->cdList = (SDCDst *)cd;
	}
	/* END MUTEX LOCK */
	return (SDCDst *)cd;		/* ready to go */
}

/* Sample cumulative distribution */
static SDError
SDsampMtxCDist(FVECT ioVec, double randX, const SDCDst *cdp)
{
	const unsigned	maxval = ~0;
	const SDMatCDst	*mcd = (const SDMatCDst *)cdp;
	const unsigned	target = randX*maxval;
	int		i, iupper, ilower;
					/* check arguments */
	if ((ioVec == NULL) | (mcd == NULL))
		return SDEargument;
					/* binary search to find index */
	ilower = 0; iupper = mcd->calen;
	while ((i = (iupper + ilower) >> 1) != ilower)
		if (target >= mcd->carr[i])
			ilower = i;
		else
			iupper = i;
					/* localize random position */
	randX = (randX*maxval - mcd->carr[ilower]) /
			(double)(mcd->carr[iupper] - mcd->carr[ilower]);
					/* convert index to vector */
	if ((*mcd->ob_vec)(ioVec, i+randX, mcd->ob_priv))
		return SDEnone;
	strcpy(SDerrorDetail, "Matrix BSDF sampling fault");
	return SDEinternal;
}

/* Fixed resolution BSDF methods */
const SDFunc		SDhandleMtx = {
				&SDgetMtxBSDF,
				&SDqueryMtxProjSA,
				&SDgetMtxCDist,
				&SDsampMtxCDist,
				&SDfreeMatrix,
			};
