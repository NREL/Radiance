/* Copyright (c) 1992 Regents of the University of California */

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
 *     Later changes described in delta comments.
 */

#include  "ray.h"

#include  "otypes.h"

#include  "random.h"

extern double  specthresh;		/* specular sampling threshold */
extern double  specjitter;		/* specular sampling jitter */

/*
 *	This routine implements the isotropic Gaussian
 *  model described by Ward in Siggraph `92 article.
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

				/* specularity flags */
#define  SP_REFL	01		/* has reflected specular component */
#define  SP_TRAN	02		/* has transmitted specular */
#define  SP_PURE	04		/* purely specular (zero roughness) */
#define  SP_FLAT	010		/* flat reflecting surface */
#define  SP_RBLT	020		/* reflection below sample threshold */
#define  SP_TBLT	040		/* transmission below threshold */

typedef struct {
	OBJREC  *mp;		/* material pointer */
	RAY  *rp;		/* ray pointer */
	short  specfl;		/* specularity flags, defined above */
	COLOR  mcolor;		/* color of this material */
	COLOR  scolor;		/* color of specular component */
	FVECT  vrefl;		/* vector in direction of reflected ray */
	FVECT  prdir;		/* vector in transmitted direction */
	double  alpha2;		/* roughness squared */
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
	double  dtmp, d2;
	FVECT  vtmp;
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
	if (ldot > FTINY && (np->specfl&(SP_REFL|SP_PURE)) == SP_REFL) {
		/*
		 *  Compute specular reflection coefficient using
		 *  gaussian distribution model.
		 */
						/* roughness */
		dtmp = np->alpha2;
						/* + source if flat */
		if (np->specfl & SP_FLAT)
			dtmp += omega/(4.0*PI);
						/* delta */
		vtmp[0] = ldir[0] - np->rp->rdir[0];
		vtmp[1] = ldir[1] - np->rp->rdir[1];
		vtmp[2] = ldir[2] - np->rp->rdir[2];
		d2 = DOT(vtmp, np->pnorm);
		d2 = 2.0 - 2.0*d2/sqrt(DOT(vtmp,vtmp));
						/* gaussian */
		dtmp = exp(-d2/dtmp)/(4.*PI*dtmp);
						/* worth using? */
		if (dtmp > FTINY) {
			copycolor(ctmp, np->scolor);
			dtmp *= omega * sqrt(ldot/np->pdot);
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
	if (ldot < -FTINY && (np->specfl&(SP_TRAN|SP_PURE)) == SP_TRAN) {
		/*
		 *  Compute specular transmission.  Specular transmission
		 *  is always modified by material color.
		 */
						/* roughness + source */
		dtmp = np->alpha2 + omega/PI;
						/* gaussian */
		dtmp = exp((2.*DOT(np->prdir,ldir)-2.)/dtmp)/(PI*dtmp);
						/* worth using? */
		if (dtmp > FTINY) {
			copycolor(ctmp, np->mcolor);
			dtmp *= np->tspec * omega * sqrt(-ldot/np->pdot);
			scalecolor(ctmp, dtmp);
			addcolor(cval, ctmp);
		}
	}
}


m_normal(m, r)			/* color a ray that hit something normal */
register OBJREC  *m;
register RAY  *r;
{
	NORMDAT  nd;
	double  transtest, transdist;
	double  dtmp;
	COLOR  ctmp;
	register int  i;
						/* easy shadow test */
	if (r->crtype & SHADOW && m->otype != MAT_TRANS)
		return;

	if (m->oargs.nfargs != (m->otype == MAT_TRANS ? 7 : 5))
		objerror(m, USER, "bad number of arguments");
	nd.mp = m;
	nd.rp = r;
						/* get material color */
	setcolor(nd.mcolor, m->oargs.farg[0],
			   m->oargs.farg[1],
			   m->oargs.farg[2]);
						/* get roughness */
	nd.specfl = 0;
	nd.alpha2 = m->oargs.farg[4];
	if ((nd.alpha2 *= nd.alpha2) <= FTINY)
		nd.specfl |= SP_PURE;
						/* reorient if necessary */
	if (r->rod < 0.0)
		flipsurface(r);
						/* get modifiers */
	raytexture(r, m->omod);
	nd.pdot = raynormal(nd.pnorm, r);	/* perturb normal */
	if (nd.pdot < .001)
		nd.pdot = .001;			/* non-zero for dirnorm() */
	multcolor(nd.mcolor, r->pcol);		/* modify material color */
	transtest = 0;
						/* get specular component */
	if ((nd.rspec = m->oargs.farg[3]) > FTINY) {
		nd.specfl |= SP_REFL;
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
						/* check threshold */
		if (!(nd.specfl & SP_PURE) &&
				specthresh > FTINY &&
				(specthresh >= 1.-FTINY ||
				specthresh + .05 - .1*frandom() > nd.rspec))
			nd.specfl |= SP_RBLT;
						/* compute reflected ray */
		for (i = 0; i < 3; i++)
			nd.vrefl[i] = r->rdir[i] + 2.0*nd.pdot*nd.pnorm[i];
		if (DOT(nd.vrefl, r->ron) <= FTINY)	/* penetration? */
			for (i = 0; i < 3; i++)		/* safety measure */
				nd.vrefl[i] = r->rdir[i] + 2.*r->rod*r->ron[i];

		if (!(r->crtype & SHADOW) && nd.specfl & SP_PURE) {
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
		if (nd.tspec > FTINY) {
			nd.specfl |= SP_TRAN;
							/* check threshold */
			if (!(nd.specfl & SP_PURE) && specthresh > FTINY &&
					(specthresh >= 1.-FTINY ||
				specthresh + .05 - .1*frandom() > nd.tspec))
				nd.specfl |= SP_TBLT;
			if (r->crtype & SHADOW ||
					DOT(r->pert,r->pert) <= FTINY*FTINY) {
				VCOPY(nd.prdir, r->rdir);
				transtest = 2;
			} else {
				for (i = 0; i < 3; i++)		/* perturb */
					nd.prdir[i] = r->rdir[i] - r->pert[i];
				if (DOT(nd.prdir, r->ron) < -FTINY)
					normalize(nd.prdir);	/* OK */
				else
					VCOPY(nd.prdir, r->rdir);
			}
		}
	} else
		nd.tdiff = nd.tspec = nd.trans = 0.0;
						/* transmitted ray */
	if ((nd.specfl&(SP_TRAN|SP_PURE)) == (SP_TRAN|SP_PURE)) {
		RAY  lr;
		if (rayorigin(&lr, r, TRANS, nd.tspec) == 0) {
			VCOPY(lr.rdir, nd.prdir);
			rayvalue(&lr);
			scalecolor(lr.rcol, nd.tspec);
			multcolor(lr.rcol, nd.mcolor);	/* modified by color */
			addcolor(r->rcol, lr.rcol);
			transtest *= bright(lr.rcol);
			transdist = r->rot + lr.rt;
		}
	} else
		transtest = 0;

	if (r->crtype & SHADOW)			/* the rest is shadow */
		return;
						/* diffuse reflection */
	nd.rdiff = 1.0 - nd.trans - nd.rspec;

	if (nd.specfl & SP_PURE && nd.rdiff <= FTINY && nd.tdiff <= FTINY)
		return;				/* 100% pure specular */

	if (r->ro != NULL && (r->ro->otype == OBJ_FACE ||
			r->ro->otype == OBJ_RING))
		nd.specfl |= SP_FLAT;

	if (nd.specfl & (SP_REFL|SP_TRAN) && !(nd.specfl & SP_PURE))
		gaussamp(r, &nd);

	if (nd.rdiff > FTINY) {		/* ambient from this side */
		ambient(ctmp, r);
		if (nd.specfl & SP_RBLT)
			scalecolor(ctmp, 1.0-nd.trans);
		else
			scalecolor(ctmp, nd.rdiff);
		multcolor(ctmp, nd.mcolor);	/* modified by material color */
		addcolor(r->rcol, ctmp);	/* add to returned color */
	}
	if (nd.tdiff > FTINY) {		/* ambient from other side */
		flipsurface(r);
		ambient(ctmp, r);
		if (nd.specfl & SP_TBLT)
			scalecolor(ctmp, nd.trans);
		else
			scalecolor(ctmp, nd.tdiff);
		multcolor(ctmp, nd.mcolor);	/* modified by color */
		addcolor(r->rcol, ctmp);
		flipsurface(r);
	}
					/* add direct component */
	direct(r, dirnorm, &nd);
					/* check distance */
	if (transtest > bright(r->rcol))
		r->rt = transdist;
}


static
gaussamp(r, np)			/* sample gaussian specular */
RAY  *r;
register NORMDAT  *np;
{
	RAY  sr;
	FVECT  u, v, h;
	double  rv[2];
	double  d, sinp, cosp;
	register int  i;
					/* quick test */
	if ((np->specfl & (SP_REFL|SP_RBLT)) != SP_REFL &&
			(np->specfl & (SP_TRAN|SP_TBLT)) != SP_TRAN)
		return;
					/* set up sample coordinates */
	v[0] = v[1] = v[2] = 0.0;
	for (i = 0; i < 3; i++)
		if (np->pnorm[i] < 0.6 && np->pnorm[i] > -0.6)
			break;
	v[i] = 1.0;
	fcross(u, v, np->pnorm);
	normalize(u);
	fcross(v, np->pnorm, u);
					/* compute reflection */
	if ((np->specfl & (SP_REFL|SP_RBLT)) == SP_REFL &&
			rayorigin(&sr, r, SPECULAR, np->rspec) == 0) {
		dimlist[ndims++] = (int)np->mp;
		d = urand(ilhash(dimlist,ndims)+samplendx);
		multisamp(rv, 2, d);
		d = 2.0*PI * rv[0];
		cosp = cos(d);
		sinp = sin(d);
		rv[1] = 1.0 - specjitter*rv[1];
		if (rv[1] <= FTINY)
			d = 1.0;
		else
			d = sqrt( np->alpha2 * -log(rv[1]) );
		for (i = 0; i < 3; i++)
			h[i] = np->pnorm[i] + d*(cosp*u[i] + sinp*v[i]);
		d = -2.0 * DOT(h, r->rdir) / (1.0 + d*d);
		for (i = 0; i < 3; i++)
			sr.rdir[i] = r->rdir[i] + d*h[i];
		if (DOT(sr.rdir, r->ron) <= FTINY)
			VCOPY(sr.rdir, np->vrefl);	/* jitter no good */
		rayvalue(&sr);
		multcolor(sr.rcol, np->scolor);
		addcolor(r->rcol, sr.rcol);
		ndims--;
	}
					/* compute transmission */
	if ((np->specfl & (SP_TRAN|SP_TBLT)) == SP_TRAN &&
			rayorigin(&sr, r, SPECULAR, np->tspec) == 0) {
		dimlist[ndims++] = (int)np->mp;
		d = urand(ilhash(dimlist,ndims)+1823+samplendx);
		multisamp(rv, 2, d);
		d = 2.0*PI * rv[0];
		cosp = cos(d);
		sinp = sin(d);
		rv[1] = 1.0 - specjitter*rv[1];
		if (rv[1] <= FTINY)
			d = 1.0;
		else
			d = sqrt( -log(rv[1]) * np->alpha2 );
		for (i = 0; i < 3; i++)
			sr.rdir[i] = np->prdir[i] + d*(cosp*u[i] + sinp*v[i]);
		if (DOT(sr.rdir, r->ron) < -FTINY)
			normalize(sr.rdir);		/* OK, normalize */
		else
			VCOPY(sr.rdir, np->prdir);	/* else no jitter */
		rayvalue(&sr);
		scalecolor(sr.rcol, np->tspec);
		multcolor(sr.rcol, np->mcolor);		/* modified by color */
		addcolor(r->rcol, sr.rcol);
		ndims--;
	}
}
