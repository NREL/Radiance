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
		scompile("Dx=$1;Dy=$2;Dz=$3;", NULL, 0);
		scompile("Nx=$4;Ny=$5;Nz=$6;", NULL, 0);
		scompile("Px=$7;Py=$8;Pz=$9;", NULL, 0);
		scompile("T=$10;Rdot=$11;", NULL, 0);
		scompile("S=$12;Tx=$13;Ty=$14;Tz=$15;", NULL, 0);
		scompile("Ix=$16;Iy=$17;Iz=$18;", NULL, 0);
		scompile("Jx=$19;Jy=$20;Jz=$21;", NULL, 0);
		scompile("Kx=$22;Ky=$23;Kz=$24;", NULL, 0);
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

		return( (	fray->rdir[0]*fxf.xfm[0][n] +
				fray->rdir[1]*fxf.xfm[1][n] +
				fray->rdir[2]*fxf.xfm[2][n]	)
			 / fxf.sca );

	if (n < 6)			/* surface normal */

		return( (	fray->ron[0]*fxf.xfm[0][n-3] +
				fray->ron[1]*fxf.xfm[1][n-3] +
				fray->ron[2]*fxf.xfm[2][n-3]	)
			 / fxf.sca );

	if (n < 9)			/* intersection */

		return( fray->rop[0]*fxf.xfm[0][n-6] +
				fray->rop[1]*fxf.xfm[1][n-6] +
				fray->rop[2]*fxf.xfm[2][n-6] +
					     fxf.xfm[3][n-6] );

	if (n == 9) {			/* distance */

		sum = fray->rot;
		for (r = fray->parent; r != NULL; r = r->parent)
			sum += r->rot;
		return(sum * fxf.sca);

	}
	if (n == 10)			/* dot product */
		return(fray->rod);

	if (n == 11)			/* scale */
		return(fxf.sca);

	if (n < 15)			/* origin */
		return(fxf.xfm[3][n-12]);

	if (n < 18)			/* i unit vector */
		return(fxf.xfm[0][n-15] / fxf.sca);

	if (n < 21)			/* j unit vector */
		return(fxf.xfm[1][n-15] / fxf.sca);

	if (n < 24)			/* k unit vector */
		return(fxf.xfm[2][n-21] / fxf.sca);
badchan:
	error(USER, "illegal channel number");
}
