/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  p_data.c - routine for stored patterns.
 *
 *     6/4/86
 */

#include  "ray.h"

#include  "data.h"

/*
 *	A stored pattern can either be brightness or
 *  color data.  Brightness data is specified as:
 *
 *	modifier brightdata name
 *	4+ func dfname vfname v0 v1 .. xf
 *	0
 *	n A1 A2 ..
 *
 *  Color data is specified as:
 *
 *	modifier colordata name
 *	8+ rfunc gfunc bfunc rdfname gdfname bdfname vfname v0 v1 .. xf
 *	0
 *	n A1 A2 ..
 *
 *  Color picture data is specified as:
 *
 *	modifier colorpict name
 *	7+ rfunc gfunc bfunc pfname vfname vx vy xf
 *	0
 *	n A1 A2 ..
 *
 *  Vfname is the name of the file where the variable definitions
 *  can be found.  The list of real arguments can be accessed by
 *  definitions in the file.  The dfnames are the data file
 *  names.  The dimensions of the data files and the number
 *  of variables must match.  The funcs take a single argument
 *  for brightdata, and three for colordata and colorpict to produce
 *  interpolated values from the file.  The xf is a transformation
 *  to get from the original coordinates to the current coordinates.
 */


p_bdata(m, r)			/* interpolate brightness data */
register OBJREC  *m;
RAY  *r;
{
	extern double  varvalue(), funvalue(), datavalue();
	extern int  errno;
	int  nv;
	double  bval;
	double  pt[MAXDIM];
	DATARRAY  *dp;
	register char  **sa;

	setfunc(m, r);

	sa = m->oargs.sarg;

	if (m->oargs.nsargs < 4)
		objerror(m, USER, "bad # arguments");
	funcfile(sa[2]);
	errno = 0;
	for (nv = 0; nv+3 < m->oargs.nsargs &&
			sa[nv+3][0] != '-'; nv++) {
		if (nv >= MAXDIM)
			goto dimerr;
		pt[nv] = varvalue(sa[nv+3]);
	}
	if (errno)
		goto computerr;
	dp = getdata(sa[1]);
	if (dp->nd != nv)
		goto dimerr;
	bval = datavalue(dp, pt);
	errno = 0;
	bval = funvalue(sa[0], 1, &bval);
	if (errno)
		goto computerr;
	scalecolor(r->pcol, bval);
	return;

dimerr:
	objerror(m, USER, "dimension error");

computerr:
	objerror(m, WARNING, "compute error");
	return;
}


p_cdata(m, r)			/* interpolate color data */
register OBJREC  *m;
RAY  *r;
{
	extern double  varvalue(), funvalue(), datavalue();
	extern int  errno;
	int  i, nv;
	double  col[3];
	COLOR  cval;
	double  pt[MAXDIM];
	DATARRAY  *dp;
	register char  **sa;

	setfunc(m, r);

	sa = m->oargs.sarg;

	if (m->oargs.nsargs < 8)
		objerror(m, USER, "bad # arguments");
	funcfile(sa[6]);
	for (nv = 0; nv+7 < m->oargs.nsargs &&
			sa[nv+7][0] != '-'; nv++) {
		if (nv >= MAXDIM)
			goto dimerr;
		errno = 0;
		pt[nv] = varvalue(sa[nv+7]);
		if (errno)
			goto computerr;
	}
	for (i = 0; i < 3; i++) {
		dp = getdata(sa[i+3]);
		if (dp->nd != nv)
			goto dimerr;
		col[i] = datavalue(dp, pt);
	}
	errno = 0;
	setcolor(cval,	funvalue(sa[0], 3, col),
			funvalue(sa[1], 3, col),
			funvalue(sa[2], 3, col));
	if (errno)
		goto computerr;
	multcolor(r->pcol, cval);
	return;

dimerr:
	objerror(m, USER, "dimension error");

computerr:
	objerror(m, WARNING, "compute error");
	return;
}


p_pdata(m, r)			/* interpolate picture data */
register OBJREC  *m;
RAY  *r;
{
	extern double  varvalue(), funvalue(), datavalue();
	extern int  errno;
	int  i;
	double  col[3];
	COLOR  cval;
	double  pt[2];
	DATARRAY  *dp;
	register char  **sa;

	setfunc(m, r);

	sa = m->oargs.sarg;

	if (m->oargs.nsargs < 7)
		objerror(m, USER, "bad # arguments");
	funcfile(sa[4]);
	errno = 0;
	pt[1] = varvalue(sa[5]);	/* y major ordering */
	pt[0] = varvalue(sa[6]);
	if (errno)
		goto computerr;
	dp = getpict(sa[3]);
	for (i = 0; i < 3; i++)
		col[i] = datavalue(dp+i, pt);
	errno = 0;
	setcolor(cval,	funvalue(sa[0], 3, col),
			funvalue(sa[1], 3, col),
			funvalue(sa[2], 3, col));
	if (errno)
		goto computerr;
	multcolor(r->pcol, cval);
	return;

computerr:
	objerror(m, WARNING, "compute error");
	return;
}
