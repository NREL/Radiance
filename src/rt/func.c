/* Copyright (c) 1986 Regents of the University of California */

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


typedef struct {
	double  xfm[4][4];		/* transform matrix */
	double  sca;			/* scalefactor */
}  XF;

static OBJREC  *fobj;		/* current function object */
static RAY  *fray;		/* current function ray */
static XF  fxf;			/* current transformation */


setmap(m, r, xfm, sca)		/* set channels for function call */
OBJREC  *m;
register RAY  *r;
double  xfm[4][4];
double  sca;
{
	extern double  l_noise3(), l_noise3a(), l_noise3b(), l_noise3c();
	extern double  l_hermite(), l_fnoise3(), l_arg();
	extern long  eclock;
	static char  *initfile = "rayinit.cal";

	if (initfile != NULL) {
		loadfunc(initfile);
		scompile(NULL, "Dx=$1;Dy=$2;Dz=$3;");
		scompile(NULL, "Nx=$4;Ny=$5;Nz=$6;");
		scompile(NULL, "Px=$7;Py=$8;Pz=$9;");
		scompile(NULL, "T=$10;Rdot=$11;");
		funset("arg", 1, l_arg);
		funset("noise3", 3, l_noise3);
		funset("noise3a", 3, l_noise3a);
		funset("noise3b", 3, l_noise3b);
		funset("noise3c", 3, l_noise3c);
		funset("hermite", 5, l_hermite);
		funset("fnoise3", 3, l_fnoise3);
		initfile = NULL;
	}
	fobj = m;
	fray = r;
	fxf.sca = r->robs * sca;
	multmat4(fxf.xfm, r->robx, xfm);
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
		mxf = (XF *)malloc(sizeof(XF));
		if (mxf == NULL)
			goto memerr;
		if (invxf(mxf->xfm, &mxf->sca, n, sa) != n)
			objerror(m, USER, "bad transform");
		if (mxf->sca < 0.0)
			mxf->sca = -mxf->sca;
		m->os = (char *)mxf;
	}
	setmap(m, r, mxf->xfm, mxf->sca);
	return;
memerr:
	error(SYSTEM, "out of memory in setfunc");
#undef  mxf
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
	double  res;
	register RAY  *r;

	n--;					/* for convenience */

	if (n < 0 || n > 10)
		error(USER, "illegal channel number");

	if (n == 9) {				/* distance */

		res = fray->rot;
		for (r = fray->parent; r != NULL; r = r->parent)
			res += r->rot;
		res *= fxf.sca;

	} else if (n == 10) {			/* dot product */

		res = fray->rod;

	} else if (n < 3) {			/* ray direction */
			res = (	fray->rdir[0]*fxf.xfm[0][n] +
					fray->rdir[1]*fxf.xfm[1][n] +
					fray->rdir[2]*fxf.xfm[2][n]	)
				 / fxf.sca ;
	} else if (n < 6) {			/* surface normal */
			res = (	fray->ron[0]*fxf.xfm[0][n-3] +
					fray->ron[1]*fxf.xfm[1][n-3] +
					fray->ron[2]*fxf.xfm[2][n-3]	)
				 / fxf.sca ;
	} else {				/* intersection */
			res =	fray->rop[0]*fxf.xfm[0][n-6] +
					fray->rop[1]*fxf.xfm[1][n-6] +
					fray->rop[2]*fxf.xfm[2][n-6] +
						     fxf.xfm[3][n-6] ;
	}

	return(res);
}
