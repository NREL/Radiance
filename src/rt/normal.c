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

				/* specularity flags */
#define  SP_REFL	01		/* has reflected specular component */
#define  SP_TRAN	02		/* has transmitted specular */
#define  SP_PURE	010		/* purely specular (zero roughness) */
#define  SP_FLAT	020		/* flat reflecting surface */

typedef struct {
	OBJREC  *mp;		/* material pointer */
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
	double  dtmp;
	int	i;
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
		dtmp = 2.0*np->alpha2;
						/* + source if flat */
		if (np->specfl & SP_FLAT)
			dtmp += omega/(2.0*PI);
						/* gaussian */
		dtmp = exp((DOT(np->vrefl,ldir)-1.)/dtmp)/(2.*PI)/dtmp;
						/* worth using? */
		if (dtmp > FTINY) {
			copycolor(ctmp, np->scolor);
			dtmp *= omega / np->pdot;
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
		dtmp = np->alpha2 + omega/(2.0*PI);
						/* gaussian */
		dtmp = exp((DOT(np->prdir,ldir)-1.)/dtmp)/(2.*PI)/dtmp;
						/* worth using? */
		if (dtmp > FTINY) {
			copycolor(ctmp, np->mcolor);
			dtmp *= np->tspec * omega / np->pdot;
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
						/* compute reflected ray */
		for (i = 0; i < 3; i++)
			nd.vrefl[i] = r->rdir[i] + 2.0*nd.pdot*nd.pnorm[i];

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
			if (r->crtype & SHADOW ||
					DOT(r->pert,r->pert) <= FTINY*FTINY) {
				VCOPY(nd.prdir, r->rdir);
				transtest = 2;
			} else {
				for (i = 0; i < 3; i++)		/* perturb */
					nd.prdir[i] = r->rdir[i] -
							.75*r->pert[i];
				normalize(nd.prdir);
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
	}

	if (r->crtype & SHADOW)			/* the rest is shadow */
		return;
						/* diffuse reflection */
	nd.rdiff = 1.0 - nd.trans - nd.rspec;

	if (nd.specfl & SP_PURE && nd.rdiff <= FTINY && nd.tdiff <= FTINY)
		return;				/* 100% pure specular */

	if (r->ro->otype == OBJ_FACE || r->ro->otype == OBJ_RING)
		nd.specfl |= SP_FLAT;

	if (nd.specfl & (SP_REFL|SP_TRAN) && !(nd.specfl & SP_PURE))
		gaussamp(r, &nd);

	if (nd.rdiff > FTINY) {		/* ambient from this side */
		ambient(ctmp, r);
		scalecolor(ctmp, nd.rdiff);
		multcolor(ctmp, nd.mcolor);	/* modified by material color */
		addcolor(r->rcol, ctmp);	/* add to returned color */
	}
	if (nd.tdiff > FTINY) {		/* ambient from other side */
		flipsurface(r);
		ambient(ctmp, r);
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
	int  confuse;
	register int  i;
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
	if (np->specfl & SP_REFL &&
			rayorigin(&sr, r, SPECULAR, np->rspec) == 0) {
		confuse = 0;
		dimlist[ndims++] = (int)np->mp;
	refagain:
		dimlist[ndims] = confuse += 3601;
		d = urand(ilhash(dimlist,ndims+1)+samplendx);
		multisamp(rv, 2, d);
		d = 2.0*PI * rv[0];
		cosp = cos(d);
		sinp = sin(d);
		if (rv[1] <= FTINY)
			d = 1.0;
		else
			d = sqrt( np->alpha2 * -log(rv[1]) );
		for (i = 0; i < 3; i++)
			h[i] = np->pnorm[i] + d*(cosp*u[i] + sinp*v[i]);
		d = -2.0 * DOT(h, r->rdir) / (1.0 + d*d);
		for (i = 0; i < 3; i++)
			sr.rdir[i] = r->rdir[i] + d*h[i];
		if (DOT(sr.rdir, r->ron) <= FTINY)	/* oops! */
			goto refagain;
		rayvalue(&sr);
		multcolor(sr.rcol, np->scolor);
		addcolor(r->rcol, sr.rcol);
		ndims--;
	}
					/* compute transmission */
}
