/* Copyright (c) 1989 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  t_data.c - routine for stored textures
 *
 *     6/4/86
 */

#include  "ray.h"

#include  "data.h"

/*
 *	A stored texture is specified as follows:
 *
 *	modifier texdata name
 *	8+ xfunc yfunc zfunc xdfname ydfname zdfname vfname v0 v1 .. xf
 *	0
 *	n A1 A2 .. An
 *
 *  Vfname is the name of the file where the variable definitions
 *  can be found.  The list of real arguments can be accessed by
 *  definitions in the file.  The dfnames are the data file
 *  names.  The dimensions of the data files and the number
 *  of variables must match.  The funcs take three arguments to produce
 *  interpolated values from the file.  The xf is a transformation
 *  to get from the original coordinates to the current coordinates.
 */

typedef struct {
	struct {
		double  sca;		/* scale factor */
		double  xfm[4][4];	/* transformation matrix */
	}  fore, back;
}  XFORM;


t_data(m, r)			/* interpolate texture data */
register OBJREC  *m;
RAY  *r;
{
	extern double  varvalue(), funvalue(), datavalue();
	extern int  errno;
	int  nv;
	FVECT  dval, disp;
	double  pt[MAXDIM];
	double  d;
	DATARRAY  *dp;
	register XFORM  *mxf;
	register char  **sa;
	register int  i;

	if (m->oargs.nsargs < 8)
		objerror(m, USER, "bad # arguments");
	sa = m->oargs.sarg;

	for (i = 7; i < m->oargs.nsargs && sa[i][0] != '-'; i++)
		;
	nv = i-7;
	if ((mxf = (XFORM *)m->os) == NULL) {
		mxf = (XFORM *)malloc(sizeof(XFORM));
		if (mxf == NULL)
			goto memerr;
		mxf->fore.sca = 1.0;
		setident4(mxf->fore.xfm);
		if (xf(mxf->fore.xfm, &mxf->fore.sca,
			m->oargs.nsargs-i, sa+i) != m->oargs.nsargs-i)
			objerror(m, USER, "bad transform");
		if (mxf->fore.sca < 0.0)
			mxf->fore.sca = -mxf->fore.sca;
		mxf->back.sca = 1.0;
		setident4(mxf->back.xfm);
		invxf(mxf->back.xfm, &mxf->back.sca,
				m->oargs.nsargs-i, sa+i);
		if (mxf->back.sca < 0.0)
			mxf->back.sca = -mxf->back.sca;
		m->os = (char *)mxf;
	}

	setmap(m, r, mxf->back.xfm, mxf->back.sca);

	if (nv > MAXDIM)
		goto dimerr;
	if (!vardefined(sa[7]))
		loadfunc(sa[6]);
	errno = 0;
	for (i = 0; i < nv; i++)
		pt[i] = varvalue(sa[i+7]);
	if (errno)
		goto computerr;
	for (i = 0; i < 3; i++) {
		dp = getdata(sa[i+3]);
		if (dp->nd != nv)
			goto dimerr;
		dval[i] = datavalue(dp, pt);
	}
	errno = 0;
	for (i = 0; i < 3; i++)
		disp[i] = funvalue(sa[i], 3, dval);
	if (errno)
		goto computerr;

	multv3(disp, disp, mxf->fore.xfm);
	multv3(disp, disp, r->rofx);
	d = 1.0 / (mxf->fore.sca * r->rofs);
	for (i = 0; i < 3; i++)
		r->pert[i] += disp[i] * d;
	return;
dimerr:
	objerror(m, USER, "dimension error");
memerr:
	error(SYSTEM, "out of memory in t_data");
computerr:
	objerror(m, WARNING, "compute error");
	return;
}
