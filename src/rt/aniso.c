/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Shading functions for anisotropic materials.
 */

#include  "ray.h"

#include  "otypes.h"

#include  "func.h"

#include  "random.h"

/*
 *	This anisotropic reflection model uses a variant on the
 *  exponential Gaussian used in normal.c.
 *	We orient the surface towards the incoming ray, so a single
 *  surface can be used to represent an infinitely thin object.
 *
 *  Arguments for MAT_PLASTIC2 and MAT_METAL2 are:
 *  4+ ux	uy	uz	funcfile	[transform...]
 *  0
 *  6  red	grn	blu	specular-frac.	u-facet-slope v-facet-slope
 *
 *  Real arguments for MAT_TRANS2 are:
 *  8  red 	grn	blu	rspec	u-rough	v-rough	trans	tspec
 */

#define  BSPEC(m)	(6.0)		/* specularity parameter b */

				/* specularity flags */
#define  SP_REFL	01		/* has reflected specular component */
#define  SP_TRAN	02		/* has transmitted specular */
#define  SP_PURE	010		/* purely specular (zero roughness) */
#define  SP_BADU	020		/* bad u direction calculation */
#define  SP_FLAT	040		/* reflecting surface is flat */

typedef struct {
	OBJREC  *mp;		/* material pointer */
	RAY  *rp;		/* ray pointer */
	short  specfl;		/* specularity flags, defined above */
	COLOR  mcolor;		/* color of this material */
	COLOR  scolor;		/* color of specular component */
	FVECT  prdir;		/* vector in transmitted direction */
	FVECT  u, v;		/* u and v vectors orienting anisotropy */
	double  u_alpha;	/* u roughness */
	double  v_alpha;	/* v roughness */
	double  rdiff, rspec;	/* reflected specular, diffuse */
	double  trans;		/* transmissivity */
	double  tdiff, tspec;	/* transmitted specular, diffuse */
	FVECT  pnorm;		/* perturbed surface normal */
	double  pdot;		/* perturbed dot product */
}  ANISODAT;		/* anisotropic material data */


diraniso(cval, np, ldir, omega)		/* compute source contribution */
COLOR  cval;			/* returned coefficient */
register ANISODAT  *np;		/* material data */
FVECT  ldir;			/* light source direction */
double  omega;			/* light source size */
{
	double  ldot;
	double  dtmp, dtmp2;
	FVECT  h;
	double  au2, av2;
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
	if (ldot > FTINY && (np->specfl&(SP_REFL|SP_PURE|SP_BADU)) == SP_REFL) {
		/*
		 *  Compute specular reflection coefficient using
		 *  anisotropic gaussian distribution model.
		 */
						/* add source width if flat */
		if (np->specfl & SP_FLAT)
			au2 = av2 = omega/(4.0*PI);
		else
			au2 = av2 = 0.0;
		au2 += np->u_alpha * np->u_alpha;
		av2 += np->v_alpha * np->v_alpha;
						/* half vector */
		h[0] = ldir[0] - np->rp->rdir[0];
		h[1] = ldir[1] - np->rp->rdir[1];
		h[2] = ldir[2] - np->rp->rdir[2];
		normalize(h);
						/* ellipse */
		dtmp = DOT(np->u, h);
		dtmp *= dtmp / au2;
		dtmp2 = DOT(np->v, h);
		dtmp2 *= dtmp2 / av2;
						/* gaussian */
		dtmp = (dtmp + dtmp2) / (1.0 + DOT(np->pnorm, h));
		dtmp = exp(-2.0*dtmp) / (4.0*PI * sqrt(au2*av2));
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
	if (ldot < -FTINY && (np->specfl&(SP_TRAN|SP_PURE|SP_BADU)) == SP_TRAN) {
		/*
		 *  Compute specular transmission.  Specular transmission
		 *  is always modified by material color.
		 */
						/* roughness + source */
						/* gaussian */
		dtmp = 0.0;
						/* worth using? */
		if (dtmp > FTINY) {
			copycolor(ctmp, np->mcolor);
			dtmp *= np->tspec * omega / np->pdot;
			scalecolor(ctmp, dtmp);
			addcolor(cval, ctmp);
		}
	}
}


m_aniso(m, r)			/* shade ray that hit something anisotropic */
register OBJREC  *m;
register RAY  *r;
{
	ANISODAT  nd;
	double  transtest, transdist;
	double  dtmp;
	COLOR  ctmp;
	register int  i;
						/* easy shadow test */
	if (r->crtype & SHADOW && m->otype != MAT_TRANS2)
		return;

	if (m->oargs.nfargs != (m->otype == MAT_TRANS2 ? 8 : 6))
		objerror(m, USER, "bad number of real arguments");
	nd.mp = m;
	nd.rp = r;
						/* get material color */
	setcolor(nd.mcolor, m->oargs.farg[0],
			   m->oargs.farg[1],
			   m->oargs.farg[2]);
						/* get roughness */
	nd.specfl = 0;
	nd.u_alpha = m->oargs.farg[4];
	nd.v_alpha = m->oargs.farg[5];
	if (nd.u_alpha <= FTINY || nd.v_alpha <= FTINY)
		nd.specfl |= SP_PURE;
						/* reorient if necessary */
	if (r->rod < 0.0)
		flipsurface(r);
						/* get modifiers */
	raytexture(r, m->omod);
	nd.pdot = raynormal(nd.pnorm, r);	/* perturb normal */
	if (nd.pdot < .001)
		nd.pdot = .001;			/* non-zero for diraniso() */
	multcolor(nd.mcolor, r->pcol);		/* modify material color */
	transtest = 0;
						/* get specular component */
	if ((nd.rspec = m->oargs.farg[3]) > FTINY) {
		nd.specfl |= SP_REFL;
						/* compute specular color */
		if (m->otype == MAT_METAL2)
			copycolor(nd.scolor, nd.mcolor);
		else
			setcolor(nd.scolor, 1.0, 1.0, 1.0);
		scalecolor(nd.scolor, nd.rspec);
						/* improved model */
		dtmp = exp(-BSPEC(m)*nd.pdot);
		for (i = 0; i < 3; i++)
			colval(nd.scolor,i) += (1.0-colval(nd.scolor,i))*dtmp;
		nd.rspec += (1.0-nd.rspec)*dtmp;

		if (!(r->crtype & SHADOW) && nd.specfl & SP_PURE) {
			RAY  lr;
			if (rayorigin(&lr, r, REFLECTED, nd.rspec) == 0) {
				for (i = 0; i < 3; i++)
					lr.rdir[i] = r->rdir[i] +
						2.0*nd.pdot*nd.pnorm[i];
				rayvalue(&lr);
				multcolor(lr.rcol, nd.scolor);
				addcolor(r->rcol, lr.rcol);
			}
		}
	}
						/* compute transmission */
	if (m->otype == MAT_TRANS) {
		nd.trans = m->oargs.farg[6]*(1.0 - nd.rspec);
		nd.tspec = nd.trans * m->oargs.farg[7];
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

	getacoords(r, &nd);			/* set up coordinates */

	if (nd.specfl & (SP_REFL|SP_TRAN) && !(nd.specfl & (SP_PURE|SP_BADU)))
		agaussamp(r, &nd);

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
	direct(r, diraniso, &nd);
					/* check distance */
	if (transtest > bright(r->rcol))
		r->rt = transdist;
}


static
getacoords(r, np)		/* set up coordinate system */
RAY  *r;
register ANISODAT  *np;
{
	register MFUNC  *mf;
	register int  i;

	mf = getfunc(np->mp, 3, 0x7, 1);
	setfunc(np->mp, r);
	errno = 0;
	for (i = 0; i < 3; i++)
		np->u[i] = evalue(mf->ep[i]);
	if (errno) {
		objerror(np->mp, WARNING, "compute error");
		np->specfl |= SP_BADU;
		return;
	}
	multv3(np->u, np->u, mf->f->xfm);
	fcross(np->v, np->pnorm, np->u);
	if (normalize(np->v) == 0.0) {
		objerror(np->mp, WARNING, "illegal orientation vector");
		np->specfl |= SP_BADU;
		return;
	}
	fcross(np->u, np->v, np->pnorm);
}


static
agaussamp(r, np)		/* sample anisotropic gaussian specular */
RAY  *r;
register ANISODAT  *np;
{
	RAY  sr;
	FVECT  h;
	double  rv[2];
	double  d, sinp, cosp;
	int  ntries;
	register int  i;
					/* compute reflection */
	if (np->specfl & SP_REFL &&
			rayorigin(&sr, r, SPECULAR, np->rspec) == 0) {
		dimlist[ndims++] = (int)np->mp;
		for (ntries = 0; ntries < 10; ntries++) {
			dimlist[ndims] = ntries * 3601;
			d = urand(ilhash(dimlist,ndims+1)+samplendx);
			multisamp(rv, 2, d);
			d = 2.0*PI * rv[0];
			cosp = np->u_alpha * cos(d);
			sinp = np->v_alpha * sin(d);
			d = sqrt(cosp*cosp + sinp*sinp);
			cosp /= d;
			sinp /= d;
			if (rv[1] <= FTINY)
				d = 1.0;
			else
				d = sqrt(-log(rv[1]) /
					(cosp*cosp/(np->u_alpha*np->u_alpha) +
					 sinp*sinp/(np->v_alpha*np->v_alpha)));
			for (i = 0; i < 3; i++)
				h[i] = np->pnorm[i] +
					d*(cosp*np->u[i] + sinp*np->v[i]);
			d = -2.0 * DOT(h, r->rdir) / (1.0 + d*d);
			for (i = 0; i < 3; i++)
				sr.rdir[i] = r->rdir[i] + d*h[i];
			if (DOT(sr.rdir, r->ron) > FTINY) {
				rayvalue(&sr);
				multcolor(sr.rcol, np->scolor);
				addcolor(r->rcol, sr.rcol);
				break;
			}
		}
		ndims--;
	}
					/* compute transmission */
}
