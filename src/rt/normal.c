/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  normal.c - shading function for normal materials.
 *
 *     8/19/85
 *     12/19/85 - added stuff for metals.
 *     6/26/87 - improved specular model.
 *     9/28/87 - added model for translucent materials.
 */

#include  "ray.h"

#include  "source.h"

#include  "otypes.h"

/*
 *	This routine uses portions of the reflection
 *  model described by Cook and Torrance.
 *	The computation of specular components has been simplified by
 *  numerous approximations and ommisions to improve speed.
 *	We orient the surface towards the incoming ray, so a single
 *  surface can be used to represent an infinitely thin object.
 *
 *  Arguments for MAT_PLASTIC and MAT_METAL are:
 *	red	grn	blu	specular-frac.	facet-slope
 *
 *  Arguments for MAT_TRANS are:
 *	red 	grn	blu	rspec	rough	trans	tspec
 */

#define  BSPEC(m)	(6.0)		/* specularity parameter b */


m_normal(m, r)			/* color a ray which hit something normal */
register OBJREC  *m;
register RAY  *r;
{
	double  exp();
	COLOR  mcolor;		/* color of this material */
	COLOR  scolor;		/* color of specular component */
	FVECT  vrefl;		/* vector in direction of reflected ray */
	double  alpha2;		/* roughness squared times 2 */
	RAY  lr;		/* ray to illumination source */
	double  rdiff, rspec;	/* reflected specular, diffuse */
	double  trans;		/* transmissivity */
	double  tdiff, tspec;	/* transmitted specular, diffuse */
	FVECT  pnorm;		/* perturbed surface normal */
	double  pdot;		/* perturbed dot product */
	double  ldot;
	double  omega;
	double  dtmp;
	COLOR  ctmp;
	register int  i;

	if (m->oargs.nfargs != (m->otype == MAT_TRANS ? 7 : 5))
		objerror(m, USER, "bad # arguments");
						/* easy shadow test */
	if (r->crtype & SHADOW && m->otype != MAT_TRANS)
		return;
						/* get material color */
	setcolor(mcolor, m->oargs.farg[0],
			   m->oargs.farg[1],
			   m->oargs.farg[2]);
						/* get roughness */
	alpha2 = m->oargs.farg[4];
	alpha2 *= 2.0 * alpha2;
						/* reorient if necessary */
	if (r->rod < 0.0)
		flipsurface(r);
						/* get modifiers */
	raytexture(r, m->omod);
	pdot = raynormal(pnorm, r);		/* perturb normal */
	multcolor(mcolor, r->pcol);		/* modify material color */
						/* get specular component */
	rspec = m->oargs.farg[3];

	if (rspec > FTINY) {			/* has specular component */
						/* compute specular color */
		if (m->otype == MAT_METAL)
			copycolor(scolor, mcolor);
		else
			setcolor(scolor, 1.0, 1.0, 1.0);
		scalecolor(scolor, rspec);
						/* improved model */
		dtmp = exp(-BSPEC(m)*pdot);
		for (i = 0; i < 3; i++)
			colval(scolor,i) += (1.0-colval(scolor,i))*dtmp;
		rspec += (1.0-rspec)*dtmp;
						/* compute reflected ray */
		for (i = 0; i < 3; i++)
			vrefl[i] = r->rdir[i] + 2.0*pdot*pnorm[i];

		if (alpha2 <= FTINY && !(r->crtype & SHADOW))
			if (rayorigin(&lr, r, REFLECTED, rspec) == 0) {
				VCOPY(lr.rdir, vrefl);
				rayvalue(&lr);
				multcolor(lr.rcol, scolor);
				addcolor(r->rcol, lr.rcol);
			}
	}

	if (m->otype == MAT_TRANS) {
		trans = m->oargs.farg[5]*(1.0 - rspec);
		tspec = trans * m->oargs.farg[6];
		tdiff = trans - tspec;
	} else
		tdiff = tspec = trans = 0.0;
						/* transmitted ray */
	if (tspec > FTINY && alpha2 <= FTINY)
		if (rayorigin(&lr, r, TRANS, tspec) == 0) {
			VCOPY(lr.rdir, r->rdir);
			rayvalue(&lr);
			scalecolor(lr.rcol, tspec);
			addcolor(r->rcol, lr.rcol);
		}
	if (r->crtype & SHADOW)			/* the rest is shadow */
		return;
						/* diffuse reflection */
	rdiff = 1.0 - trans - rspec;

	if (rdiff <= FTINY && tdiff <= FTINY && alpha2 <= FTINY)
		return;				/* purely specular */

	ambient(ctmp, r);		/* compute ambient component */
	scalecolor(ctmp, 1.0-trans);	/* from this side */
	multcolor(ctmp, mcolor);	/* modified by material color */
	addcolor(r->rcol, ctmp);	/* add to returned color */

	if (trans > FTINY) {		/* ambient from other side */
		flipsurface(r);
		scalecolor(ctmp, trans);
		multcolor(ctmp, mcolor);
		addcolor(r->rcol, ctmp);
		flipsurface(r);
	}
	
	for (i = 0; i < nsources; i++) {	/* add specular and diffuse */

		if ((omega = srcray(&lr, r, i)) == 0.0)
			continue;		/* bad source */

		ldot = DOT(pnorm, lr.rdir);
	
		if (ldot < 0.0 ? trans <= FTINY : trans >= 1.0-FTINY)
			continue;		/* wrong side */
	
		rayvalue(&lr);			/* compute light ray value */
	
		if (intens(lr.rcol) <= FTINY)
			continue;		/* didn't hit light source */

		if (ldot > FTINY && rdiff > FTINY) {
			/*
			 *  Compute and add diffuse component to returned color.
			 *  The diffuse component will always be modified by the
			 *  color of the material.
			 */
			copycolor(ctmp, lr.rcol);
			dtmp = ldot * omega * rdiff / PI;
			scalecolor(ctmp, dtmp);
			multcolor(ctmp, mcolor);
			addcolor(r->rcol, ctmp);
		}
		if (ldot > FTINY && rspec > FTINY && alpha2 > FTINY) {
			/*
			 *  Compute specular reflection coefficient using
			 *  gaussian distribution model.
			 */
							/* roughness + source */
			dtmp = alpha2 + omega/(2.0*PI);
							/* gaussian */
			dtmp = exp((DOT(vrefl,lr.rdir)-1.)/dtmp)/(2.*PI)/dtmp;
							/* worth using? */
			if (dtmp > FTINY) {
				copycolor(ctmp, lr.rcol);
				dtmp *= omega;
				scalecolor(ctmp, dtmp);
				multcolor(ctmp, scolor);
				addcolor(r->rcol, ctmp);
			}
		}
		if (ldot < -FTINY && tdiff > FTINY) {
			/*
			 *  Compute diffuse transmission.
			 */
			copycolor(ctmp, lr.rcol);
			dtmp = -ldot * omega * tdiff / PI;
			scalecolor(ctmp, dtmp);
			multcolor(ctmp, mcolor);
			addcolor(r->rcol, ctmp);
		}
		if (ldot < -FTINY && tspec > FTINY && alpha2 > FTINY) {
			/*
			 *  Compute specular transmission.
			 */
							/* roughness + source */
			dtmp = alpha2 + omega/(2.0*PI);
							/* gaussian */
			dtmp = exp((DOT(r->rdir,lr.rdir)-1.)/dtmp)/(2.*PI)/dtmp;
							/* worth using? */
			if (dtmp > FTINY) {
				copycolor(ctmp, lr.rcol);
				dtmp *= tspec * omega;
				scalecolor(ctmp, dtmp);
				addcolor(r->rcol, ctmp);
			}
		}
	}
}
