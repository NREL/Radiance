/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  func.c - interface to calcomp functions.
 *
 *     4/7/86
 */

#include  "ray.h"

#include  "otypes.h"


XF  unitxf = {			/* identity transform */
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0,
	1.0
};

XF  funcxf;			/* current transformation */
static OBJREC  *fobj = NULL;	/* current function object */
static RAY  *fray = NULL;	/* current function ray */


setmap(m, r, bx)		/* set channels for function call */
OBJREC  *m;
register RAY  *r;
XF  *bx;
{
	extern double  l_arg();
	extern long  eclock;
	static char  *initfile = "rayinit.cal";
	static long  lastrno = -1;
					/* check to see if already set */
	if (m == fobj && r->rno == lastrno)
		return;
					/* initialize if first call */
	if (initfile != NULL) {
		loadfunc(initfile);
		scompile("Dx=$1;Dy=$2;Dz=$3;", NULL, 0);
		scompile("Nx=$4;Ny=$5;Nz=$6;", NULL, 0);
		scompile("Px=$7;Py=$8;Pz=$9;", NULL, 0);
		scompile("T=$10;Rdot=$11;", NULL, 0);
		scompile("S=$12;Tx=$13;Ty=$14;Tz=$15;", NULL, 0);
		scompile("Ix=$16;Iy=$17;Iz=$18;", NULL, 0);
		scompile("Jx=$19;Jy=$20;Jz=$21;", NULL, 0);
		scompile("Kx=$22;Ky=$23;Kz=$24;", NULL, 0);
		funset("arg", 1, '=', l_arg);
		setnoisefuncs();
		initfile = NULL;
	}
	fobj = m;
	fray = r;
	if (r->rox != NULL)
		if (bx != &unitxf) {
			funcxf.sca = r->rox->b.sca * bx->sca;
			multmat4(funcxf.xfm, r->rox->b.xfm, bx->xfm);
		} else
			copystruct(&funcxf, &r->rox->b);
	else
		copystruct(&funcxf, bx);
	lastrno = r->rno;
	eclock++;		/* notify expression evaluator */
}


setfunc(m, r)				/* simplified interface to setmap */
register OBJREC  *m;
RAY  *r;
{
	register XF  *mxf;

	if ((mxf = (XF *)m->os) == NULL) {
		register int  n;
		register char  **sa;

		for (n = m->oargs.nsargs, sa = m->oargs.sarg;
				n > 0 && **sa != '-'; n--, sa++)
			;
		if (n == 0)
			mxf = &unitxf;
		else {
			mxf = (XF *)malloc(sizeof(XF));
			if (mxf == NULL)
				goto memerr;
			if (invxf(mxf, n, sa) != n)
				objerror(m, USER, "bad transform");
			if (mxf->sca < 0.0)
				mxf->sca = -mxf->sca;
		}
		m->os = (char *)mxf;
	}
	setmap(m, r, mxf);
	return;
memerr:
	error(SYSTEM, "out of memory in setfunc");
}


loadfunc(fname)			/* load definition file */
char  *fname;
{
	extern char  *libpath;		/* library search path */
	char  *ffname;

	if ((ffname = getpath(fname, libpath, R_OK)) == NULL) {
		sprintf(errmsg, "cannot find function file \"%s\"", fname);
		error(USER, errmsg);
	}
	fcompile(ffname);
}


double
l_arg()				/* return nth real argument */
{
	extern double  argument();
	register int  n;

	n = argument(1) + .5;		/* round to integer */

	if (n < 1)
		return(fobj->oargs.nfargs);

	if (n > fobj->oargs.nfargs) {
		sprintf(errmsg, "missing real argument %d", n);
		objerror(fobj, USER, errmsg);
	}
	return(fobj->oargs.farg[n-1]);
}


double
chanvalue(n)			/* return channel n to calcomp */
register int  n;
{
	double  sum;
	register RAY  *r;

	if (--n < 0)
		goto badchan;

	if (n < 3)			/* ray direction */

		return( (	fray->rdir[0]*funcxf.xfm[0][n] +
				fray->rdir[1]*funcxf.xfm[1][n] +
				fray->rdir[2]*funcxf.xfm[2][n]	)
			 / funcxf.sca );

	if (n < 6)			/* surface normal */

		return( (	fray->ron[0]*funcxf.xfm[0][n-3] +
				fray->ron[1]*funcxf.xfm[1][n-3] +
				fray->ron[2]*funcxf.xfm[2][n-3]	)
			 / funcxf.sca );

	if (n < 9)			/* intersection */

		return( fray->rop[0]*funcxf.xfm[0][n-6] +
				fray->rop[1]*funcxf.xfm[1][n-6] +
				fray->rop[2]*funcxf.xfm[2][n-6] +
					     funcxf.xfm[3][n-6] );

	if (n == 9) {			/* distance */

		sum = fray->rot;
		for (r = fray->parent; r != NULL; r = r->parent)
			sum += r->rot;
		return(sum * funcxf.sca);

	}
	if (n == 10)			/* dot product */
		return(fray->rod);

	if (n == 11)			/* scale */
		return(funcxf.sca);

	if (n < 15)			/* origin */
		return(funcxf.xfm[3][n-12]);

	if (n < 18)			/* i unit vector */
		return(funcxf.xfm[0][n-15] / funcxf.sca);

	if (n < 21)			/* j unit vector */
		return(funcxf.xfm[1][n-15] / funcxf.sca);

	if (n < 24)			/* k unit vector */
		return(funcxf.xfm[2][n-21] / funcxf.sca);
badchan:
	error(USER, "illegal channel number");
}
