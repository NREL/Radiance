/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Shading for materials with arbitrary BRDF's
 */

#include  "ray.h"

#include  "data.h"

#include  "otypes.h"

/*
 *	Arguments to this material include the color and specularity.
 *  String arguments include the reflection function and files.
 *  The BRDF is currently used just for the specular component to light
 *  sources.  Reflectance values or data coordinates are functions
 *  of the direction to the light source.
 *	We orient the surface towards the incoming ray, so a single
 *  surface can be used to represent an infinitely thin object.
 *
 *  Arguments for MAT_PFUNC and MAT_MFUNC are:
 *	2+	func	funcfile	transform ..
 *	0
 *	4+	red	grn	blu	specularity	args ..
 *
 *  Arguments for MAT_PDATA and MAT_MDATA are:
 *	4+	func	datafile	funcfile	v0 ..	transform ..
 *	0
 *	4+	red	grn	blu	specularity	args ..
 */

extern double	funvalue(), varvalue();

#define  BSPEC(m)		(6.0)		/* specular parameter b */

typedef struct {
	OBJREC  *mp;		/* material pointer */
	RAY  *pr;		/* intersected ray */
	DATARRAY  *dp;		/* data array for PDATA or MDATA */
	COLOR  mcolor;		/* color of this material */
	COLOR  scolor;		/* color of specular component */
	double  rspec;		/* specular reflection */
	double	rdiff;		/* diffuse reflection */
	FVECT  pnorm;		/* perturbed surface normal */
	double  pdot;		/* perturbed dot product */
}  BRDFDAT;		/* BRDF material data */


dirbrdf(cval, np, ldir, omega)		/* compute source contribution */
COLOR  cval;			/* returned coefficient */
register BRDFDAT  *np;		/* material data */
FVECT  ldir;			/* light source direction */
double  omega;			/* light source size */
{
	double  ldot;
	double  dtmp;
	COLOR  ctmp;
	double  pt[MAXDIM];
	register int	i;

	setcolor(cval, 0.0, 0.0, 0.0);
	
	ldot = DOT(np->pnorm, ldir);

	if (ldot < 0.0)
		return;		/* wrong side */

	if (np->rdiff > FTINY) {
		/*
		 *  Compute and add diffuse reflected component to returned
		 *  color.  The diffuse reflected component will always be
		 *  modified by the color of the material.
		 */
		copycolor(ctmp, np->mcolor);
		dtmp = ldot * omega * np->rdiff / PI;
		scalecolor(ctmp, dtmp);
		addcolor(cval, ctmp);
	}
	if (np->rspec > FTINY) {
		/*
		 *  Compute specular component.
		 */
		setfunc(np->mp, np->pr);
		errno = 0;
		if (np->dp == NULL)
			dtmp = funvalue(np->mp->oargs.sarg[0], 3, ldir);
		else {
			for (i = 0; i < np->dp->nd; i++)
				pt[i] = funvalue(np->mp->oargs.sarg[3+i],
						3, ldir);
			dtmp = datavalue(np->dp, pt);
			dtmp = funvalue(np->mp->oargs.sarg[0], 1, &dtmp);
		}
		if (errno)
			goto computerr;
		if (dtmp > FTINY) {
			copycolor(ctmp, np->scolor);
			dtmp *= ldot * omega;
			scalecolor(ctmp, dtmp);
			addcolor(cval, ctmp);
		}
	}
	return;
computerr:
	objerror(np->mp, WARNING, "compute error");
	return;
}


m_brdf(m, r)			/* color a ray which hit a BRDF material */
register OBJREC  *m;
register RAY  *r;
{
	BRDFDAT  nd;
	double  dtmp;
	COLOR  ctmp;
	register int  i;

	if (m->oargs.nsargs < 2 || m->oargs.nfargs < 4)
		objerror(m, USER, "bad # arguments");
						/* easy shadow test */
	if (r->crtype & SHADOW)
		return;
	nd.mp = m;
	nd.pr = r;
						/* load auxiliary files */
	if (m->otype == MAT_PDATA || m->otype == MAT_MDATA) {
		nd.dp = getdata(m->oargs.sarg[1]);
		for (i = 3; i < m->oargs.nsargs; i++)
			if (m->oargs.sarg[i][0] == '-')
				break;
		if (i-3 != nd.dp->nd)
			objerror(m, USER, "dimension error");
		if (!fundefined(m->oargs.sarg[3]))
			loadfunc(m->oargs.sarg[2]);
	} else {
		nd.dp = NULL;
		if (!fundefined(m->oargs.sarg[0]))
			loadfunc(m->oargs.sarg[1]);
	}
						/* get material color */
	setcolor(nd.mcolor, m->oargs.farg[0],
			   m->oargs.farg[1],
			   m->oargs.farg[2]);
						/* get roughness */
	if (r->rod < 0.0)
		flipsurface(r);
						/* get modifiers */
	raytexture(r, m->omod);
	nd.pdot = raynormal(nd.pnorm, r);	/* perturb normal */
	multcolor(nd.mcolor, r->pcol);		/* modify material color */
	r->rt = r->rot;				/* default ray length */
						/* get specular component */
	nd.rspec = m->oargs.farg[3];

	if (nd.rspec > FTINY) {			/* has specular component */
						/* compute specular color */
		if (m->otype == MAT_MFUNC || m->otype == MAT_MDATA)
			copycolor(nd.scolor, nd.mcolor);
		else
			setcolor(nd.scolor, 1.0, 1.0, 1.0);
		scalecolor(nd.scolor, nd.rspec);
						/* improved model */
		dtmp = exp(-BSPEC(m)*nd.pdot);
		for (i = 0; i < 3; i++)
			colval(nd.scolor,i) += (1.0-colval(nd.scolor,i))*dtmp;
		nd.rspec += (1.0-nd.rspec)*dtmp;
	}
						/* diffuse reflection */
	nd.rdiff = 1.0 - nd.rspec;
						/* compute ambient */
	if (nd.rdiff > FTINY) {
		ambient(ctmp, r);
		multcolor(ctmp, nd.mcolor);	/* modified by material color */
		addcolor(r->rcol, ctmp);	/* add to returned color */
	}
						/* add direct component */
	direct(r, dirbrdf, &nd);
}
