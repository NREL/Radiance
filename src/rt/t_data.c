/* Copyright (c) 1990 Regents of the University of California */

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
	register FULLXF  *mxf;
	register char  **sa;
	register int  i;

	if (m->oargs.nsargs < 8)
		objerror(m, USER, "bad # arguments");
	sa = m->oargs.sarg;

	for (i = 7; i < m->oargs.nsargs && sa[i][0] != '-'; i++)
		;
	nv = i-7;
	if ((mxf = (FULLXF *)m->os) == NULL) {
		mxf = (FULLXF *)malloc(sizeof(FULLXF));
		if (mxf == NULL)
			goto memerr;
		if (fullxf(mxf, m->oargs.nsargs-i, sa+i) != m->oargs.nsargs-i)
			objerror(m, USER, "bad transform");
		if (mxf->f.sca < 0.0)
			mxf->f.sca = -mxf->f.sca;
		if (mxf->b.sca < 0.0)
			mxf->b.sca = -mxf->b.sca;
		m->os = (char *)mxf;
	}

	setmap(m, r, &mxf->b);

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

	multv3(disp, disp, mxf->f.xfm);
	if (r->rox != NULL) {
		multv3(disp, disp, r->rox->f.xfm);
		d = 1.0 / (mxf->f.sca * r->rox->f.sca);
	} else
		d = 1.0 / mxf->f.sca;
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
