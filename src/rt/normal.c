/* Copyright (c) 1991 Regents of the University of California */

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

extern double  exp();

typedef struct {
	OBJREC  *mp;		/* material pointer */
	RAY  *pr;		/* intersected ray */
	COLOR  mcolor;		/* color of this material */
	COLOR  scolor;		/* color of specular component */
	FVECT  vrefl;		/* vector in direction of reflected ray */
	double  alpha2;		/* roughness squared times 2 */
	double  rdiff, rspec;	/* reflected specular, diffuse */
	double  trans;		/* transmissivity */
	double  tdiff, tspec;	/* transmitted specular, diffuse */
	FVECT  pnorm;		/* perturbed surface normal */
	double  pdot;		/* perturbed dot product */
}  NORMDAT;		/* normal material data */


dirnorm(cval, np, ldir, omega)		/* compute source contribution */
COLOR  cval;			/* returned coefficient */
register NORMDAT  *np;		/* material data */
FVECT  ldir;			/* light source direction */
double  omega;			/* light source size */
{
	double  ldot;
	double  dtmp;
	COLOR  ctmp;

	setcolor(cval, 0.0, 0.0, 0.0);

	ldot = DOT(np->pnorm, ldir);

	if (ldot < 0.0 ? np->trans <= FTINY : np->trans >= 1.0-FTINY)
		return;		/* wrong side */

	if (ldot > FTINY && np->rdiff > FTINY) {
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
	if (ldot > FTINY && np->rspec > FTINY && np->alpha2 > FTINY) {
		/*
		 *  Compute specular reflection coefficient using
		 *  gaussian distribution model.
		 */
						/* roughness + source */
		dtmp = np->alpha2 + omega/(2.0*PI);
						/* gaussian */
		dtmp = exp((DOT(np->vrefl,ldir)-1.)/dtmp)/(2.*PI)/dtmp;
						/* worth using? */
		if (dtmp > FTINY) {
			copycolor(ctmp, np->scolor);
			dtmp *= omega;
			scalecolor(ctmp, dtmp);
			addcolor(cval, ctmp);
		}
	}
	if (ldot < -FTINY && np->tdiff > FTINY) {
		/*
		 *  Compute diffuse transmission.
		 */
		copycolor(ctmp, np->mcolor);
		dtmp = -ldot * omega * np->tdiff / PI;
		scalecolor(ctmp, dtmp);
		addcolor(cval, ctmp);
	}
	if (ldot < -FTINY && np->tspec > FTINY && np->alpha2 > FTINY) {
		/*
		 *  Compute specular transmission.  Specular transmission
		 *  is unaffected by material color.
		 */
						/* roughness + source */
		dtmp = np->alpha2 + omega/(2.0*PI);
						/* gaussian */
		dtmp = exp((DOT(np->pr->rdir,ldir)-1.)/dtmp)/(2.*PI)/dtmp;
						/* worth using? */
		if (dtmp > FTINY) {
			dtmp *= np->tspec * omega;
			setcolor(ctmp, dtmp, dtmp, dtmp);
			addcolor(cval, ctmp);
		}
	}
}


m_normal(m, r)			/* color a ray which hit something normal */
register OBJREC  *m;
register RAY  *r;
{
	NORMDAT  nd;
	double  transtest, transdist;
	double  dtmp;
	COLOR  ctmp;
	register int  i;

	if (m->oargs.nfargs != (m->otype == MAT_TRANS ? 7 : 5))
		objerror(m, USER, "bad # arguments");
						/* easy shadow test */
	if (r->crtype & SHADOW && m->otype != MAT_TRANS)
		return;
	nd.mp = m;
	nd.pr = r;
						/* get material color */
	setcolor(nd.mcolor, m->oargs.farg[0],
			   m->oargs.farg[1],
			   m->oargs.farg[2]);
						/* get roughness */
	nd.alpha2 = m->oargs.farg[4];
	nd.alpha2 *= 2.0 * nd.alpha2;
						/* reorient if necessary */
	if (r->rod < 0.0)
		flipsurface(r);
						/* get modifiers */
	raytexture(r, m->omod);
	nd.pdot = raynormal(nd.pnorm, r);	/* perturb normal */
	multcolor(nd.mcolor, r->pcol);		/* modify material color */
	transtest = 0;
						/* get specular component */
	nd.rspec = m->oargs.farg[3];

	if (nd.rspec > FTINY) {			/* has specular component */
						/* compute specular color */
		if (m->otype == MAT_METAL)
			copycolor(nd.scolor, nd.mcolor);
		else
			setcolor(nd.scolor, 1.0, 1.0, 1.0);
		scalecolor(nd.scolor, nd.rspec);
						/* improved model */
		dtmp = exp(-BSPEC(m)*nd.pdot);
		for (i = 0; i < 3; i++)
			colval(nd.scolor,i) += (1.0-colval(nd.scolor,i))*dtmp;
		nd.rspec += (1.0-nd.rspec)*dtmp;
						/* compute reflected ray */
		for (i = 0; i < 3; i++)
			nd.vrefl[i] = r->rdir[i] + 2.0*nd.pdot*nd.pnorm[i];

		if (nd.alpha2 <= FTINY && !(r->crtype & SHADOW)) {
			RAY  lr;
			if (rayorigin(&lr, r, REFLECTED, nd.rspec) == 0) {
				VCOPY(lr.rdir, nd.vrefl);
				rayvalue(&lr);
				multcolor(lr.rcol, nd.scolor);
				addcolor(r->rcol, lr.rcol);
			}
		}
	}
						/* compute transmission */
	if (m->otype == MAT_TRANS) {
		nd.trans = m->oargs.farg[5]*(1.0 - nd.rspec);
		nd.tspec = nd.trans * m->oargs.farg[6];
		nd.tdiff = nd.trans - nd.tspec;
	} else
		nd.tdiff = nd.tspec = nd.trans = 0.0;
						/* transmitted ray */
	if (nd.tspec > FTINY && nd.alpha2 <= FTINY) {
		RAY  lr;
		if (rayorigin(&lr, r, TRANS, nd.tspec) == 0) {
			if (!(r->crtype & SHADOW) &&
					DOT(r->pert,r->pert) > FTINY*FTINY) {
				for (i = 0; i < 3; i++)	/* perturb direction */
					lr.rdir[i] = r->rdir[i] -
							.75*r->pert[i];
				normalize(lr.rdir);
			} else {
				VCOPY(lr.rdir, r->rdir);
				transtest = 2;
			}
			rayvalue(&lr);
			scalecolor(lr.rcol, nd.tspec);
			multcolor(lr.rcol, nd.mcolor);	/* modified by color */
			addcolor(r->rcol, lr.rcol);
			transtest *= bright(lr.rcol);
			transdist = r->rot + lr.rt;
		}
	}
	if (r->crtype & SHADOW)			/* the rest is shadow */
		return;
						/* diffuse reflection */
	nd.rdiff = 1.0 - nd.trans - nd.rspec;

	if (nd.rdiff <= FTINY && nd.tdiff <= FTINY && nd.alpha2 <= FTINY)
		return;				/* purely specular */

	if (nd.rdiff > FTINY) {		/* ambient from this side */
		ambient(ctmp, r);
		if (nd.alpha2 <= FTINY)
			scalecolor(ctmp, nd.rdiff);
		else
			scalecolor(ctmp, 1.0-nd.trans);
		multcolor(ctmp, nd.mcolor);	/* modified by material color */
		addcolor(r->rcol, ctmp);	/* add to returned color */
	}
	if (nd.tdiff > FTINY) {		/* ambient from other side */
		flipsurface(r);
		ambient(ctmp, r);
		if (nd.alpha2 <= FTINY)
			scalecolor(ctmp, nd.tdiff);
		else
			scalecolor(ctmp, nd.trans);
		multcolor(ctmp, nd.mcolor);
		addcolor(r->rcol, ctmp);
		flipsurface(r);
	}
					/* add direct component */
	direct(r, dirnorm, &nd);
					/* check distance */
	if (transtest > bright(r->rcol))
		r->rt = transdist;
}
