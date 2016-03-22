#ifndef lint
static const char RCSid[] = "$Id$";
#endif
/*
 *  Shading functions for anisotropic materials.
 */

#include "copyright.h"

#include  "ray.h"
#include  "ambient.h"
#include  "otypes.h"
#include  "rtotypes.h"
#include  "source.h"
#include  "func.h"
#include  "random.h"
#include  "pmapmat.h"

#ifndef  MAXITER
#define  MAXITER	10		/* maximum # specular ray attempts */
#endif

/*
 *	This routine implements the anisotropic Gaussian
 *  model described by Ward in Siggraph `92 article, updated with
 *  normalization and sampling adjustments due to Geisler-Moroder and Duer.
 *	We orient the surface towards the incoming ray, so a single
 *  surface can be used to represent an infinitely thin object.
 *
 *  Arguments for MAT_PLASTIC2 and MAT_METAL2 are:
 *  4+ ux	uy	uz	funcfile	[transform...]
 *  0
 *  6  red	grn	blu	specular-frac.	u-rough v-rough
 *
 *  Real arguments for MAT_TRANS2 are:
 *  8  red 	grn	blu	rspec	u-rough	v-rough	trans	tspec
 */

				/* specularity flags */
#define  SP_REFL	01		/* has reflected specular component */
#define  SP_TRAN	02		/* has transmitted specular */
#define  SP_FLAT	04		/* reflecting surface is flat */
#define  SP_RBLT	010		/* reflection below sample threshold */
#define  SP_TBLT	020		/* transmission below threshold */

typedef struct {
	OBJREC  *mp;		/* material pointer */
	RAY  *rp;		/* ray pointer */
	short  specfl;		/* specularity flags, defined above */
	COLOR  mcolor;		/* color of this material */
	COLOR  scolor;		/* color of specular component */
	FVECT  vrefl;		/* vector in reflected direction */
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

static void getacoords(ANISODAT  *np);
static void agaussamp(ANISODAT  *np);


static void
diraniso(		/* compute source contribution */
	COLOR  cval,			/* returned coefficient */
	void  *nnp,			/* material data */
	FVECT  ldir,			/* light source direction */
	double  omega			/* light source size */
)
{
	ANISODAT *np = nnp;
	double  ldot;
	double  dtmp, dtmp1, dtmp2;
	FVECT  h;
	double  au2, av2;
	COLOR  ctmp;

	setcolor(cval, 0.0, 0.0, 0.0);

	ldot = DOT(np->pnorm, ldir);

	if (ldot < 0.0 ? np->trans <= FTINY : np->trans >= 1.0-FTINY)
		return;		/* wrong side */

	if ((ldot > FTINY) & (np->rdiff > FTINY)) {
		/*
		 *  Compute and add diffuse reflected component to returned
		 *  color.  The diffuse reflected component will always be
		 *  modified by the color of the material.
		 */
		copycolor(ctmp, np->mcolor);
		dtmp = ldot * omega * np->rdiff * (1.0/PI);
		scalecolor(ctmp, dtmp);
		addcolor(cval, ctmp);
	}

	if ((ldot < -FTINY) & (np->tdiff > FTINY)) {
		/*
		 *  Compute diffuse transmission.
		 */
		copycolor(ctmp, np->mcolor);
		dtmp = -ldot * omega * np->tdiff * (1.0/PI);
		scalecolor(ctmp, dtmp);
		addcolor(cval, ctmp);
	}
	
	if (ambRayInPmap(np->rp))
		return;		/* specular accounted for in photon map */

	if (ldot > FTINY && np->specfl&SP_REFL) {
		/*
		 *  Compute specular reflection coefficient using
		 *  anisotropic Gaussian distribution model.
		 */
						/* add source width if flat */
		if (np->specfl & SP_FLAT)
			au2 = av2 = omega * (0.25/PI);
		else
			au2 = av2 = 0.0;
		au2 += np->u_alpha*np->u_alpha;
		av2 += np->v_alpha*np->v_alpha;
						/* half vector */
		VSUB(h, ldir, np->rp->rdir);
						/* ellipse */
		dtmp1 = DOT(np->u, h);
		dtmp1 *= dtmp1 / au2;
		dtmp2 = DOT(np->v, h);
		dtmp2 *= dtmp2 / av2;
						/* new W-G-M-D model */
		dtmp = DOT(np->pnorm, h);
		dtmp *= dtmp;
		dtmp1 = (dtmp1 + dtmp2) / dtmp;
		dtmp = exp(-dtmp1) * DOT(h,h) /
				(PI * dtmp*dtmp * sqrt(au2*av2));
						/* worth using? */
		if (dtmp > FTINY) {
			copycolor(ctmp, np->scolor);
			dtmp *= ldot * omega;
			scalecolor(ctmp, dtmp);
			addcolor(cval, ctmp);
		}
	}
	
	if (ldot < -FTINY && np->specfl&SP_TRAN) {
		/*
		 *  Compute specular transmission.  Specular transmission
		 *  is always modified by material color.
		 */
						/* roughness + source */
		au2 = av2 = omega * (1.0/PI);
		au2 += np->u_alpha*np->u_alpha;
		av2 += np->v_alpha*np->v_alpha;
						/* "half vector" */
		VSUB(h, ldir, np->prdir);
		dtmp = DOT(h,h);
		if (dtmp > FTINY*FTINY) {
			dtmp1 = DOT(h,np->pnorm);
			dtmp = 1.0 - dtmp1*dtmp1/dtmp;
			if (dtmp > FTINY*FTINY) {
				dtmp1 = DOT(h,np->u);
				dtmp1 *= dtmp1 / au2;
				dtmp2 = DOT(h,np->v);
				dtmp2 *= dtmp2 / av2;
				dtmp = (dtmp1 + dtmp2) / dtmp;
			}
		} else
			dtmp = 0.0;
						/* Gaussian */
		dtmp = exp(-dtmp) * (1.0/PI) * sqrt(-ldot/(np->pdot*au2*av2));
						/* worth using? */
		if (dtmp > FTINY) {
			copycolor(ctmp, np->mcolor);
			dtmp *= np->tspec * omega;
			scalecolor(ctmp, dtmp);
			addcolor(cval, ctmp);
		}
	}
}


int
m_aniso(			/* shade ray that hit something anisotropic */
	OBJREC  *m,
	RAY  *r
)
{
	ANISODAT  nd;
	COLOR  ctmp;
	int  i;
						/* easy shadow test */
	if (r->crtype & SHADOW)
		return(1);

	if (m->oargs.nfargs != (m->otype == MAT_TRANS2 ? 8 : 6))
		objerror(m, USER, "bad number of real arguments");
						/* check for back side */
	if (r->rod < 0.0) {
		if (!backvis) {
			raytrans(r);
			return(1);
		}
		raytexture(r, m->omod);
		flipsurface(r);			/* reorient if backvis */
	} else
		raytexture(r, m->omod);
						/* get material color */
	nd.mp = m;
	nd.rp = r;
	setcolor(nd.mcolor, m->oargs.farg[0],
			   m->oargs.farg[1],
			   m->oargs.farg[2]);
						/* get roughness */
	nd.specfl = 0;
	nd.u_alpha = m->oargs.farg[4];
	nd.v_alpha = m->oargs.farg[5];
	if ((nd.u_alpha <= FTINY) | (nd.v_alpha <= FTINY))
		objerror(m, USER, "roughness too small");

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
						/* check threshold */
		if (specthresh >= nd.rspec-FTINY)
			nd.specfl |= SP_RBLT;
						/* compute refl. direction */
		VSUM(nd.vrefl, r->rdir, nd.pnorm, 2.0*nd.pdot);
		if (DOT(nd.vrefl, r->ron) <= FTINY)	/* penetration? */
			VSUM(nd.vrefl, r->rdir, r->ron, 2.0*r->rod);
	}
						/* compute transmission */
	if (m->otype == MAT_TRANS2) {
		nd.trans = m->oargs.farg[6]*(1.0 - nd.rspec);
		nd.tspec = nd.trans * m->oargs.farg[7];
		nd.tdiff = nd.trans - nd.tspec;
		if (nd.tspec > FTINY) {
			nd.specfl |= SP_TRAN;
							/* check threshold */
			if (specthresh >= nd.tspec-FTINY)
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

	if (r->ro != NULL && isflat(r->ro->otype))
		nd.specfl |= SP_FLAT;

	getacoords(&nd);			/* set up coordinates */

	if (nd.specfl & (SP_REFL|SP_TRAN))
		agaussamp(&nd);

	if (nd.rdiff > FTINY) {		/* ambient from this side */
		copycolor(ctmp, nd.mcolor);	/* modified by material color */
		scalecolor(ctmp, nd.rdiff);
		if (nd.specfl & SP_RBLT)	/* add in specular as well? */
			addcolor(ctmp, nd.scolor);
		multambient(ctmp, r, nd.pnorm);
		addcolor(r->rcol, ctmp);	/* add to returned color */
	}
	
	if (nd.tdiff > FTINY) {		/* ambient from other side */
		FVECT  bnorm;

		flipsurface(r);
		bnorm[0] = -nd.pnorm[0];
		bnorm[1] = -nd.pnorm[1];
		bnorm[2] = -nd.pnorm[2];
		copycolor(ctmp, nd.mcolor);	/* modified by color */
		if (nd.specfl & SP_TBLT)
			scalecolor(ctmp, nd.trans);
		else
			scalecolor(ctmp, nd.tdiff);
		multambient(ctmp, r, bnorm);
		addcolor(r->rcol, ctmp);
		flipsurface(r);
	}
					/* add direct component */
	direct(r, diraniso, &nd);

	return(1);
}

static void
getacoords(		/* set up coordinate system */
	ANISODAT  *np
)
{
	MFUNC  *mf;
	int  i;

	mf = getfunc(np->mp, 3, 0x7, 1);
	setfunc(np->mp, np->rp);
	errno = 0;
	for (i = 0; i < 3; i++)
		np->u[i] = evalue(mf->ep[i]);
	if ((errno == EDOM) | (errno == ERANGE))
		np->u[0] = np->u[1] = np->u[2] = 0.0;
	if (mf->fxp != &unitxf)
		multv3(np->u, np->u, mf->fxp->xfm);
	fcross(np->v, np->pnorm, np->u);
	if (normalize(np->v) == 0.0) {
		if (fabs(np->u_alpha - np->v_alpha) > 0.001)
			objerror(np->mp, WARNING, "illegal orientation vector");
		getperpendicular(np->u, np->pnorm, 1);	/* punting */
		fcross(np->v, np->pnorm, np->u);
		np->u_alpha = np->v_alpha = sqrt( 0.5 *
			(np->u_alpha*np->u_alpha + np->v_alpha*np->v_alpha) );
	} else
		fcross(np->u, np->v, np->pnorm);
}


static void
agaussamp(		/* sample anisotropic Gaussian specular */
	ANISODAT  *np
)
{
	RAY  sr;
	FVECT  h;
	double  rv[2];
	double  d, sinp, cosp;
	COLOR	scol;
	int  maxiter, ntrials, nstarget, nstaken;
	int  i;
					/* compute reflection */
	if ((np->specfl & (SP_REFL|SP_RBLT)) == SP_REFL &&
			rayorigin(&sr, SPECULAR, np->rp, np->scolor) == 0) {
		nstarget = 1;
		if (specjitter > 1.5) {	/* multiple samples? */
			nstarget = specjitter*np->rp->rweight + .5;
			if (sr.rweight <= minweight*nstarget)
				nstarget = sr.rweight/minweight;
			if (nstarget > 1) {
				d = 1./nstarget;
				scalecolor(sr.rcoef, d);
				sr.rweight *= d;
			} else
				nstarget = 1;
		}
		setcolor(scol, 0., 0., 0.);
		dimlist[ndims++] = (int)(size_t)np->mp;
		maxiter = MAXITER*nstarget;
		for (nstaken = ntrials = 0; nstaken < nstarget &&
						ntrials < maxiter; ntrials++) {
			if (ntrials)
				d = frandom();
			else
				d = urand(ilhash(dimlist,ndims)+samplendx);
			multisamp(rv, 2, d);
			d = 2.0*PI * rv[0];
			cosp = tcos(d) * np->u_alpha;
			sinp = tsin(d) * np->v_alpha;
			d = 1./sqrt(cosp*cosp + sinp*sinp);
			cosp *= d;
			sinp *= d;
			if ((0. <= specjitter) & (specjitter < 1.))
				rv[1] = 1.0 - specjitter*rv[1];
			if (rv[1] <= FTINY)
				d = 1.0;
			else
				d = sqrt(-log(rv[1]) /
					(cosp*cosp/(np->u_alpha*np->u_alpha) +
					 sinp*sinp/(np->v_alpha*np->v_alpha)));
			for (i = 0; i < 3; i++)
				h[i] = np->pnorm[i] +
					d*(cosp*np->u[i] + sinp*np->v[i]);
			d = -2.0 * DOT(h, np->rp->rdir) / (1.0 + d*d);
			VSUM(sr.rdir, np->rp->rdir, h, d);
						/* sample rejection test */
			if ((d = DOT(sr.rdir, np->rp->ron)) <= FTINY)
				continue;
			checknorm(sr.rdir);
			if (nstarget > 1) {	/* W-G-M-D adjustment */
				if (nstaken) rayclear(&sr);
				rayvalue(&sr);
				d = 2./(1. + np->rp->rod/d);
				scalecolor(sr.rcol, d);
				addcolor(scol, sr.rcol);
			} else {
				rayvalue(&sr);
				multcolor(sr.rcol, sr.rcoef);
				addcolor(np->rp->rcol, sr.rcol);
			}
			++nstaken;
		}
		if (nstarget > 1) {		/* final W-G-M-D weighting */
			multcolor(scol, sr.rcoef);
			d = (double)nstarget/ntrials;
			scalecolor(scol, d);
			addcolor(np->rp->rcol, scol);
		}
		ndims--;
	}
					/* compute transmission */
	copycolor(sr.rcoef, np->mcolor);		/* modify by material color */
	scalecolor(sr.rcoef, np->tspec);
	if ((np->specfl & (SP_TRAN|SP_TBLT)) == SP_TRAN &&
			rayorigin(&sr, SPECULAR, np->rp, sr.rcoef) == 0) {
		nstarget = 1;
		if (specjitter > 1.5) {	/* multiple samples? */
			nstarget = specjitter*np->rp->rweight + .5;
			if (sr.rweight <= minweight*nstarget)
				nstarget = sr.rweight/minweight;
			if (nstarget > 1) {
				d = 1./nstarget;
				scalecolor(sr.rcoef, d);
				sr.rweight *= d;
			} else
				nstarget = 1;
		}
		dimlist[ndims++] = (int)(size_t)np->mp;
		maxiter = MAXITER*nstarget;
		for (nstaken = ntrials = 0; nstaken < nstarget &&
						ntrials < maxiter; ntrials++) {
			if (ntrials)
				d = frandom();
			else
				d = urand(ilhash(dimlist,ndims)+1823+samplendx);
			multisamp(rv, 2, d);
			d = 2.0*PI * rv[0];
			cosp = tcos(d) * np->u_alpha;
			sinp = tsin(d) * np->v_alpha;
			d = 1./sqrt(cosp*cosp + sinp*sinp);
			cosp *= d;
			sinp *= d;
			if ((0. <= specjitter) & (specjitter < 1.))
				rv[1] = 1.0 - specjitter*rv[1];
			if (rv[1] <= FTINY)
				d = 1.0;
			else
				d = sqrt(-log(rv[1]) /
					(cosp*cosp/(np->u_alpha*np->u_alpha) +
					 sinp*sinp/(np->v_alpha*np->v_alpha)));
			for (i = 0; i < 3; i++)
				sr.rdir[i] = np->prdir[i] +
						d*(cosp*np->u[i] + sinp*np->v[i]);
			if (DOT(sr.rdir, np->rp->ron) >= -FTINY)
				continue;
			normalize(sr.rdir);	/* OK, normalize */
			if (nstaken)		/* multi-sampling */
				rayclear(&sr);
			rayvalue(&sr);
			multcolor(sr.rcol, sr.rcoef);
			addcolor(np->rp->rcol, sr.rcol);
			++nstaken;
		}
		ndims--;
	}
}
