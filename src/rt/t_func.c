/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  t_func.c - routine for procedural textures.
 *
 *     4/8/86
 */

#include  "ray.h"

/*
 *	A procedural texture perturbs the surface normal
 *  at the point of intersection with an object.  It has
 *  the form:
 *
 *	modifier texfunc name
 *	4+ xvarname yvarname zvarname filename xf
 *	0
 *	n A1 A2 ..
 *
 *  Filename is the name of the file where the variable definitions
 *  can be found.  The list of real arguments can be accessed by
 *  definitions in the file.  The xf is a transformation to get
 *  from the original coordinates to the current coordinates.
 */


t_func(m, r)			/* compute texture for ray */
register OBJREC  *m;
register RAY  *r;
{
	extern double  varvalue();
	extern int  errno;
	FVECT  disp;
	double  d;
	register FULLXF  *mxf;
	register int  i;
	register char  **sa;

	if (m->oargs.nsargs < 4)
		objerror(m, USER, "bad # arguments");
	sa = m->oargs.sarg;

	if ((mxf = (FULLXF *)m->os) == NULL) {
		mxf = (FULLXF *)malloc(sizeof(FULLXF));
		if (mxf == NULL)
			goto memerr;
		if (fullxf(mxf, m->oargs.nsargs-4, sa+4) != m->oargs.nsargs-4)
			objerror(m, USER, "bad transform");
		if (mxf->f.sca < 0.0)
			mxf->f.sca = -mxf->f.sca;
		if (mxf->b.sca < 0.0)
			mxf->b.sca = -mxf->b.sca;
		m->os = (char *)mxf;
	}

	setmap(m, r, &mxf->b);

	if (!vardefined(sa[0]))
		loadfunc(sa[3]);
	errno = 0;
	for (i = 0; i < 3; i++)
		disp[i] = varvalue(sa[i]);
	if (errno) {
		objerror(m, WARNING, "compute error");
		return;
	}
	multv3(disp, disp, mxf->f.xfm);
	if (r->rox != NULL) {
		multv3(disp, disp, r->rox->f.xfm);
		d = 1.0 / (mxf->f.sca * r->rox->f.sca);
	} else
		d = 1.0 / mxf->f.sca;
	for (i = 0; i < 3; i++)
		r->pert[i] += disp[i] * d;
	return;
memerr:
	error(SYSTEM, "out of memory in t_func");
}
