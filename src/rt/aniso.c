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

extern double  specthresh;		/* specular sampling threshold */
extern double  specjitter;		/* specular sampling jitter */

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
#define  SP_FLAT	04		/* reflecting surface is flat */
#define  SP_RBLT	010		/* reflection below sample threshold */
#define  SP_TBLT	020		/* transmission below threshold */
#define  SP_BADU	040		/* bad u direction calculation */

typedef struct {
	OBJREC  *mp;		/* material pointer */
	RAY  *rp;		/* ray pointer */
	short  specfl;		/* specularity flags, defined above */
	COLOR  mcolor;		/* color of this material */
	COLOR  scolor;		/* color of specular component */
	FVECT  vrefl;		/* vector in reflected direction */
	FVECT  prdir;		/* vector in transmitted direction */
	FVECT  u, v;		/* u and v vectors orienting anisotropy */
	double  u_alpha2;	/* u roughness squared */
	double  v_alpha2;	/* v roughness squared */
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
	double  dtmp, dtmp1, dtmp2;
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
	if (ldot > FTINY && (np->specfl&(SP_REFL|SP_BADU)) == SP_REFL) {
		/*
		 *  Compute specular reflection coefficient using
		 *  anisotropic gaussian distribution model.
		 */
						/* add source width if flat */
		if (np->specfl & SP_FLAT)
			au2 = av2 = omega/(4.0*PI);
		else
			au2 = av2 = 0.0;
		au2 += np->u_alpha2;
		av2 += np->v_alpha2;
						/* half vector */
		h[0] = ldir[0] - np->rp->rdir[0];
		h[1] = ldir[1] - np->rp->rdir[1];
		h[2] = ldir[2] - np->rp->rdir[2];
		normalize(h);
						/* ellipse */
		dtmp1 = DOT(np->u, h);
		dtmp1 *= dtmp1 / au2;
		dtmp2 = DOT(np->v, h);
		dtmp2 *= dtmp2 / av2;
						/* gaussian */
		dtmp = (dtmp1 + dtmp2) / (1.0 + DOT(np->pnorm, h));
		dtmp = exp(-2.0*dtmp) * 1.0/(4.0*PI)
				* sqrt(ldot/(np->pdot*au2*av2));
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
	if (ldot < -FTINY && (np->specfl&(SP_TRAN|SP_BADU)) == SP_TRAN) {
		/*
		 *  Compute specular transmission.  Specular transmission
		 *  is always modified by material color.
		 */
						/* roughness + source */
		au2 = av2 = omega / PI;
		au2 += np->u_alpha2;
		av2 += np->v_alpha2;
						/* "half vector" */
		h[0] = ldir[0] - np->prdir[0];
		h[1] = ldir[1] - np->prdir[1];
		h[2] = ldir[2] - np->prdir[2];
		dtmp = DOT(h,np->pnorm);
		dtmp = DOT(h,h) - dtmp*dtmp;
		if (dtmp > FTINY*FTINY) {
			dtmp1 = DOT(h,np->u);
			dtmp1 = dtmp1*dtmp1 / (au2*dtmp);
			dtmp2 = DOT(h,np->v);
			dtmp2 = dtmp2*dtmp2 / (av2*dtmp);
			dtmp = 2. - 2.*DOT(ldir,np->prdir);
			dtmp *= dtmp1 + dtmp2;
		} else
			dtmp = 0.0;
						/* gaussian */
		dtmp = exp(-dtmp) * 1.0/(4.0*PI)
				* sqrt(-ldot/(np->pdot*au2*av2));
						/* worth using? */
		if (dtmp > FTINY) {
			copycolor(ctmp, np->mcolor);
			dtmp *= np->tspec * omega;
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
	double  dtmp;
	COLOR  ctmp;
	register int  i;
						/* easy shadow test */
	if (r->crtype & SHADOW)
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
	nd.u_alpha2 = m->oargs.farg[4];
	nd.u_alpha2 *= nd.u_alpha2;
	nd.v_alpha2 = m->oargs.farg[5];
	nd.v_alpha2 *= nd.v_alpha2;
	if (nd.u_alpha2 < FTINY*FTINY || nd.v_alpha2 <= FTINY*FTINY)
		objerror(m, USER, "roughness too small");
						/* reorient if necessary */
	if (r->rod < 0.0)
		flipsurface(r);
						/* get modifiers */
	raytexture(r, m->omod);
	nd.pdot = raynormal(nd.pnorm, r);	/* perturb normal */
	if (nd.pdot < .001)
		nd.pdot = .001;			/* non-zero for diraniso() */
	multcolor(nd.mcolor, r->pcol);		/* modify material color */
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
						/* check threshold */
		if (specthresh > FTINY &&
				(specthresh >= 1.-FTINY ||
				specthresh + .05 - .1*frandom() > nd.rspec))
			nd.specfl |= SP_RBLT;
						/* compute refl. direction */
		for (i = 0; i < 3; i++)
			nd.vrefl[i] = r->rdir[i] + 2.0*nd.pdot*nd.pnorm[i];
		if (DOT(nd.vrefl, r->ron) <= FTINY)	/* penetration? */
			for (i = 0; i < 3; i++)		/* safety measure */
				nd.vrefl[i] = r->rdir[i] + 2.*r->rod*r->ron[i];
	}
						/* compute transmission */
	if (m->otype == MAT_TRANS2) {
		nd.trans = m->oargs.farg[6]*(1.0 - nd.rspec);
		nd.tspec = nd.trans * m->oargs.farg[7];
		nd.tdiff = nd.trans - nd.tspec;
		if (nd.tspec > FTINY) {
			nd.specfl |= SP_TRAN;
							/* check threshold */
			if (specthresh > FTINY &&
					(specthresh >= 1.-FTINY ||
				specthresh + .05 - .1*frandom() > nd.tspec))
				nd.specfl |= SP_TBLT;
			if (DOT(r->pert,r->pert) <= FTINY*FTINY) {
				VCOPY(nd.prdir, r->rdir);
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

						/* diffuse reflection */
	nd.rdiff = 1.0 - nd.trans - nd.rspec;

	if (r->ro != NULL && (r->ro->otype == OBJ_FACE ||
			r->ro->otype == OBJ_RING))
		nd.specfl |= SP_FLAT;

	getacoords(r, &nd);			/* set up coordinates */

	if (nd.specfl & (SP_REFL|SP_TRAN) && !(nd.specfl & SP_BADU))
		agaussamp(r, &nd);

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
	direct(r, diraniso, &nd);
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
	if (mf->f != &unitxf)
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
	register int  i;
					/* compute reflection */
	if ((np->specfl & (SP_REFL|SP_RBLT)) == SP_REFL &&
			rayorigin(&sr, r, SPECULAR, np->rspec) == 0) {
		dimlist[ndims++] = (int)np->mp;
		d = urand(ilhash(dimlist,ndims)+samplendx);
		multisamp(rv, 2, d);
		d = 2.0*PI * rv[0];
		cosp = cos(d);
		sinp = sin(d);
		d = sqrt(np->u_alpha2*cosp*cosp + np->v_alpha2*sinp*sinp);
		cosp /= d;
		sinp /= d;
		rv[1] = 1.0 - specjitter*rv[1];
		if (rv[1] <= FTINY)
			d = 1.0;
		else
			d = sqrt(-log(rv[1]) /
				(cosp*cosp/np->u_alpha2 +
				 sinp*sinp/np->v_alpha2));
		for (i = 0; i < 3; i++)
			h[i] = np->pnorm[i] +
				d*(cosp*np->u[i] + sinp*np->v[i]);
		d = -2.0 * DOT(h, r->rdir) / (1.0 + d*d);
		for (i = 0; i < 3; i++)
			sr.rdir[i] = r->rdir[i] + d*h[i];
		if (DOT(sr.rdir, r->ron) <= FTINY)	/* penetration? */
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
			d = sqrt(-log(rv[1]) /
				(cosp*cosp*4./np->u_alpha2 +
				 sinp*sinp*4./np->v_alpha2));
		for (i = 0; i < 3; i++)
			sr.rdir[i] = np->prdir[i] +
					d*(cosp*np->u[i] + sinp*np->v[i]);
		if (DOT(sr.rdir, r->ron) < -FTINY)
			normalize(sr.rdir);		/* OK, normalize */
		else
			VCOPY(sr.rdir, np->prdir);	/* else no jitter */
		rayvalue(&sr);
		scalecolor(sr.rcol, np->tspec);
		multcolor(sr.rcol, np->mcolor);		/* modify by color */
		addcolor(r->rcol, sr.rcol);
		ndims--;
	}
}
