#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 *  dielectric.c - shading function for transparent materials.
 */

#include "copyright.h"

#include  "ray.h"
#include  "otypes.h"
#include  "rtotypes.h"
#include  "pmapmat.h"

#ifdef  DISPERSE
#include  "source.h"
static int disperse(OBJREC *m,RAY *r,FVECT vt,double tr,COLOR cet,COLOR abt);
static int lambda(OBJREC  *m, FVECT  v2, FVECT  dv, FVECT  lr);
#endif

static double mylog(double  x);


/*
 *     Explicit calculations for Fresnel's equation are performed,
 *  but only one square root computation is necessary.
 *     The index of refraction is given as a Hartmann equation
 *  with lambda0 equal to zero.  If the slope of Hartmann's
 *  equation is non-zero, the material disperses light upon
 *  refraction.  This condition is examined on rays traced to
 *  light sources.  If a ray is exiting a dielectric material, we
 *  check the sources to see if any would cause bright color to be
 *  directed to the viewer due to dispersion.  This gives colorful
 *  sparkle to crystals, etc.  (Only if DISPERSE is defined!)
 *
 *  Arguments for MAT_DIELECTRIC are:
 *	red	grn	blu	rndx	Hartmann
 *
 *  Arguments for MAT_INTERFACE are:
 *	red1	grn1	blu1	rndx1	red2	grn2	blu2	rndx2
 *
 *  The primaries are material transmission per unit length.
 *  MAT_INTERFACE uses dielectric1 for inside and dielectric2 for
 *  outside.
 */


#define  MLAMBDA	500		/* mean lambda */
#define  MAXLAMBDA	779		/* maximum lambda */
#define  MINLAMBDA	380		/* minimum lambda */

#define  MINCOS		0.997		/* minimum dot product for dispersion */

static double
mylog(		/* special log for extinction coefficients */
	double  x
)
{
	if (x < 1e-40)
		return(-100.);
	if (x >= 1.)
		return(0.);
	return(log(x));
}


int
m_dielectric(	/* color a ray which hit a dielectric interface */
	OBJREC  *m,
	RAY  *r
)
{
	double  cos1, cos2, nratio;
	COLOR  ctrans;
	COLOR  talb;
	int  hastexture;
	double  transdist=0, transtest=0;
	double  mirdist=0, mirtest=0;
	int	flatsurface;
	double  refl, trans;
	FVECT  dnorm;
	double  d1, d2;
	RAY  p;
	int  i;

	/* PMAP: skip refracted shadow or ambient ray if accounted for in
	   photon map */
	if (shadowRayInPmap(r) || ambRayInPmap(r))
		return(1);
	
	if (m->oargs.nfargs != (m->otype==MAT_DIELECTRIC ? 5 : 8))
		objerror(m, USER, "bad arguments");

	raytexture(r, m->omod);			/* get modifiers */

	if ( (hastexture = DOT(r->pert,r->pert) > FTINY*FTINY) )
		cos1 = raynormal(dnorm, r);	/* perturb normal */
	else {
		VCOPY(dnorm, r->ron);
		cos1 = r->rod;
	}
	flatsurface = r->ro != NULL && isflat(r->ro->otype) &&
			!hastexture | (r->crtype & AMBIENT);

						/* index of refraction */
	if (m->otype == MAT_DIELECTRIC)
		nratio = m->oargs.farg[3] + m->oargs.farg[4]/MLAMBDA;
	else
		nratio = m->oargs.farg[3] / m->oargs.farg[7];
	
	if (cos1 < 0.0) {			/* inside */
		hastexture = -hastexture;
		cos1 = -cos1;
		dnorm[0] = -dnorm[0];
		dnorm[1] = -dnorm[1];
		dnorm[2] = -dnorm[2];
		setcolor(r->cext, -mylog(m->oargs.farg[0]*colval(r->pcol,RED)),
				 -mylog(m->oargs.farg[1]*colval(r->pcol,GRN)),
				 -mylog(m->oargs.farg[2]*colval(r->pcol,BLU)));
		setcolor(r->albedo, 0., 0., 0.);
		r->gecc = 0.;
		if (m->otype == MAT_INTERFACE) {
			setcolor(ctrans,
				-mylog(m->oargs.farg[4]*colval(r->pcol,RED)),
				-mylog(m->oargs.farg[5]*colval(r->pcol,GRN)),
				-mylog(m->oargs.farg[6]*colval(r->pcol,BLU)));
			setcolor(talb, 0., 0., 0.);
		} else {
			copycolor(ctrans, cextinction);
			copycolor(talb, salbedo);
		}
	} else {				/* outside */
		nratio = 1.0 / nratio;

		setcolor(ctrans, -mylog(m->oargs.farg[0]*colval(r->pcol,RED)),
				 -mylog(m->oargs.farg[1]*colval(r->pcol,GRN)),
				 -mylog(m->oargs.farg[2]*colval(r->pcol,BLU)));
		setcolor(talb, 0., 0., 0.);
		if (m->otype == MAT_INTERFACE) {
			setcolor(r->cext,
				-mylog(m->oargs.farg[4]*colval(r->pcol,RED)),
				-mylog(m->oargs.farg[5]*colval(r->pcol,GRN)),
				-mylog(m->oargs.farg[6]*colval(r->pcol,BLU)));
			setcolor(r->albedo, 0., 0., 0.);
			r->gecc = 0.;
		}
	}

	d2 = 1.0 - nratio*nratio*(1.0 - cos1*cos1);	/* compute cos theta2 */

	if (d2 < FTINY)			/* total reflection */

		refl = 1.0;

	else {				/* refraction occurs */
					/* compute Fresnel's equations */
		cos2 = sqrt(d2);
		d1 = cos1;
		d2 = nratio*cos2;
		d1 = (d1 - d2) / (d1 + d2);
		refl = d1 * d1;

		d1 = 1.0 / cos1;
		d2 = nratio / cos2;
		d1 = (d1 - d2) / (d1 + d2);
		refl += d1 * d1;

		refl *= 0.5;
		trans = 1.0 - refl;

		trans *= nratio*nratio;		/* solid angle ratio */

		setcolor(p.rcoef, trans, trans, trans);

		if (rayorigin(&p, REFRACTED, r, p.rcoef) == 0) {

						/* compute refracted ray */
			d1 = nratio*cos1 - cos2;
			for (i = 0; i < 3; i++)
				p.rdir[i] = nratio*r->rdir[i] + d1*dnorm[i];
						/* accidental reflection? */
			if (hastexture &&
				DOT(p.rdir,r->ron)*hastexture >= -FTINY) {
				d1 *= (double)hastexture;
				for (i = 0; i < 3; i++)	/* ignore texture */
					p.rdir[i] = nratio*r->rdir[i] +
							d1*r->ron[i];
				normalize(p.rdir);	/* not exact */
			} else
				checknorm(p.rdir);
#ifdef  DISPERSE
			if (m->otype != MAT_DIELECTRIC
					|| r->rod > 0.0
					|| r->crtype & SHADOW
					|| !directvis
					|| m->oargs.farg[4] == 0.0
					|| !disperse(m, r, p.rdir,
							trans, ctrans, talb))
#endif
			{
				copycolor(p.cext, ctrans);
				copycolor(p.albedo, talb);
				rayvalue(&p);
				multcolor(p.rcol, p.rcoef);
				addcolor(r->rcol, p.rcol);
						/* virtual distance */
				if (flatsurface ||
					(1.-FTINY <= nratio) &
						(nratio <= 1.+FTINY)) {
					transtest = 2*bright(p.rcol);
					transdist = r->rot + p.rt;
				}
			}
		}
	}
	setcolor(p.rcoef, refl, refl, refl);

	if (!(r->crtype & SHADOW) &&
			rayorigin(&p, REFLECTED, r, p.rcoef) == 0) {

					/* compute reflected ray */
		VSUM(p.rdir, r->rdir, dnorm, 2.*cos1);
					/* accidental penetration? */
		if (hastexture && DOT(p.rdir,r->ron)*hastexture <= FTINY)
			VSUM(p.rdir, r->rdir, r->ron, 2.*r->rod);
		checknorm(p.rdir);
		rayvalue(&p);			/* reflected ray value */

		multcolor(p.rcol, p.rcoef);	/* color contribution */
		addcolor(r->rcol, p.rcol);
						/* virtual distance */
		if (flatsurface) {
			mirtest = 2*bright(p.rcol);
			mirdist = r->rot + p.rt;
		}
	}
				/* check distance to return */
	d1 = bright(r->rcol);
	if (transtest > d1)
		r->rt = transdist;
	else if (mirtest > d1)
		r->rt = mirdist;
				/* rayvalue() computes absorption */
	return(1);
}


#ifdef  DISPERSE

static int
disperse(  /* check light sources for dispersion */
	OBJREC  *m,
	RAY  *r,
	FVECT  vt,
	double  tr,
	COLOR  cet,
	COLOR  abt
)
{
	RAY  sray;
	const RAY  *entray;
	FVECT  v1, v2, n1, n2;
	FVECT  dv, v2Xdv;
	double  v2Xdvv2Xdv;
	int  success = 0;
	SRCINDEX  si;
	FVECT  vtmp1, vtmp2;
	double  dtmp1, dtmp2;
	int  l1, l2;
	COLOR  ctmp;
	int  i;
	
	/*
	 *     This routine computes dispersion to the first order using
	 *  the following assumptions:
	 *
	 *	1) The dependency of the index of refraction on wavelength
	 *		is approximated by Hartmann's equation with lambda0
	 *		equal to zero.
	 *	2) The entry and exit locations are constant with respect
	 *		to dispersion.
	 *
	 *     The second assumption permits us to model dispersion without
	 *  having to sample refracted directions.  We assume that the
	 *  geometry inside the material is constant, and concern ourselves
	 *  only with the relationship between the entering and exiting ray.
	 *  We compute the first derivatives of the entering and exiting
	 *  refraction with respect to the index of refraction.  This 
	 *  is then used in a first order Taylor series to determine the
	 *  index of refraction necessary to send the exiting ray to each
	 *  light source.
	 *     If an exiting ray hits a light source within the refraction
	 *  boundaries, we sum all the frequencies over the disc of the
	 *  light source to determine the resulting color.  A smaller light
	 *  source will therefore exhibit a sharper spectrum.
	 */

	if (!(r->crtype & REFRACTED)) {		/* ray started in material */
		VCOPY(v1, r->rdir);
		n1[0] = -r->rdir[0]; n1[1] = -r->rdir[1]; n1[2] = -r->rdir[2];
	} else {
						/* find entry point */
		for (entray = r; entray->rtype != REFRACTED;
				entray = entray->parent)
			;
		entray = entray->parent;
		if (entray->crtype & REFRACTED)		/* too difficult */
			return(0);
		VCOPY(v1, entray->rdir);
		VCOPY(n1, entray->ron);
	}
	VCOPY(v2, vt);			/* exiting ray */
	VCOPY(n2, r->ron);

					/* first order dispersion approx. */
	dtmp1 = 1./DOT(n1, v1);
	dtmp2 = 1./DOT(n2, v2);
	for (i = 0; i < 3; i++)
		dv[i] = v1[i] + v2[i] - n1[i]*dtmp1 - n2[i]*dtmp2;
		
	if (DOT(dv, dv) <= FTINY)	/* null effect */
		return(0);
					/* compute plane normal */
	fcross(v2Xdv, v2, dv);
	v2Xdvv2Xdv = DOT(v2Xdv, v2Xdv);

					/* check sources */
	initsrcindex(&si);
	while (srcray(&sray, r, &si)) {

		if (DOT(sray.rdir, v2) < MINCOS)
			continue;			/* bad source */
						/* adjust source ray */

		dtmp1 = DOT(v2Xdv, sray.rdir) / v2Xdvv2Xdv;
		sray.rdir[0] -= dtmp1 * v2Xdv[0];
		sray.rdir[1] -= dtmp1 * v2Xdv[1];
		sray.rdir[2] -= dtmp1 * v2Xdv[2];

		l1 = lambda(m, v2, dv, sray.rdir);	/* mean lambda */

		if (l1 > MAXLAMBDA || l1 < MINLAMBDA)	/* not visible */
			continue;
						/* trace source ray */
		copycolor(sray.cext, cet);
		copycolor(sray.albedo, abt);
		normalize(sray.rdir);
		rayvalue(&sray);
		if (bright(sray.rcol) <= FTINY)	/* missed it */
			continue;
		
		/*
		 *	Compute spectral sum over diameter of source.
		 *  First find directions for rays going to opposite
		 *  sides of source, then compute wavelengths for each.
		 */
		 
		fcross(vtmp1, v2Xdv, sray.rdir);
		dtmp1 = sqrt(si.dom  / v2Xdvv2Xdv / PI);

							/* compute first ray */
		VSUM(vtmp2, sray.rdir, vtmp1, dtmp1);

		l1 = lambda(m, v2, dv, vtmp2);		/* first lambda */
		if (l1 < 0)
			continue;
							/* compute second ray */
		VSUM(vtmp2, sray.rdir, vtmp1, -dtmp1);

		l2 = lambda(m, v2, dv, vtmp2);		/* second lambda */
		if (l2 < 0)
			continue;
					/* compute color from spectrum */
		if (l1 < l2)
			spec_rgb(ctmp, l1, l2);
		else
			spec_rgb(ctmp, l2, l1);
		multcolor(ctmp, sray.rcol);
		scalecolor(ctmp, tr);
		addcolor(r->rcol, ctmp);
		success++;
	}
	return(success);
}


static int
lambda(			/* compute lambda for material */
	OBJREC  *m,
	FVECT  v2,
	FVECT  dv,
	FVECT  lr
)
{
	FVECT  lrXdv, v2Xlr;
	double  dtmp, denom;
	int  i;

	fcross(lrXdv, lr, dv);
	for (i = 0; i < 3; i++)	
		if ((lrXdv[i] > FTINY) | (lrXdv[i] < -FTINY))
			break;
	if (i >= 3)
		return(-1);

	fcross(v2Xlr, v2, lr);

	dtmp = m->oargs.farg[4] / MLAMBDA;
	denom = dtmp + v2Xlr[i]/lrXdv[i] * (m->oargs.farg[3] + dtmp);

	if (denom < FTINY)
		return(-1);

	return(m->oargs.farg[4] / denom);
}

#endif  /* DISPERSE */
