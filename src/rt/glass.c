/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  glass.c - simpler shading function for thin glass surfaces.
 *
 *     11/14/86
 */

#include  "ray.h"

/*
 *  This definition of glass provides for a quick calculation
 *  using a single surface where two closely spaced parallel
 *  dielectric surfaces would otherwise be used.  The chief
 *  advantage to using this material is speed, since internal
 *  reflections are avoided.
 *
 *  The specification for glass is as follows:
 *
 *	modifier glass id
 *	0
 *	0
 *	3 red grn blu
 *
 *  The color is used for the transmission at normal incidence.
 *  To compute transmission (tn) from transmissivity (Tn) use:
 *
 *	tn = (sqrt(.8402528435+.0072522239*Tn*Tn)-.9166530661)/.0036261119/Tn
 *
 *  The transmission of standard 88% transmissivity glass is 0.96.
 *  If we appear to hit the back side of the surface, then we
 *  turn the normal around.
 */

#define  RINDEX		1.52		/* refractive index of glass */


m_glass(m, r)		/* color a ray which hit a thin glass surface */
OBJREC  *m;
register RAY  *r;
{
	double  sqrt(), pow();
	COLOR  mcolor;
	double  pdot;
	FVECT  pnorm;
	double  cos2;
	COLOR  trans, refl;
	double  d, r1;
	double  transtest, transdist;
	RAY  p;
	register int  i;

	if (m->oargs.nfargs != 3)
		objerror(m, USER, "bad arguments");

	setcolor(mcolor, m->oargs.farg[0], m->oargs.farg[1], m->oargs.farg[2]);

	if (r->rod < 0.0)			/* reorient if necessary */
		flipsurface(r);
	r->rt = r->rot;				/* default ray length */
	transtest = 0;
						/* get modifiers */
	raytexture(r, m->omod);
	pdot = raynormal(pnorm, r);
						/* angular transmission */
	cos2 = sqrt( (1.0-1.0/RINDEX/RINDEX) +
		     pdot*pdot/(RINDEX*RINDEX) );
	setcolor(mcolor, pow(colval(mcolor,RED), 1.0/cos2),
			 pow(colval(mcolor,GRN), 1.0/cos2),
			 pow(colval(mcolor,BLU), 1.0/cos2));

						/* compute reflection */
	r1 = (pdot - RINDEX*cos2) / (pdot + RINDEX*cos2);
	d = (1.0/pdot - RINDEX/cos2) / (1.0/pdot + RINDEX/cos2);
	r1 = (r1*r1 + d*d) / 2.0;
						/* compute transmittance */
	for (i = 0; i < 3; i++) {
		d = colval(mcolor, i);
		colval(trans,i) = (1.0-r1)*(1.0-r1)*d / (1.0 - r1*r1*d*d);
	}
						/* transmitted ray */
	if (rayorigin(&p, r, TRANS, bright(trans)) == 0) {
		if (DOT(r->pert,r->pert) > FTINY*FTINY) {
			for (i = 0; i < 3; i++)		/* perturb direction */
				p.rdir[i] = r->rdir[i] - r->pert[i]/RINDEX;
			normalize(p.rdir);
		} else
			transtest = 2;
		rayvalue(&p);
		multcolor(p.rcol, r->pcol);	/* modify */
		multcolor(p.rcol, trans);
		addcolor(r->rcol, p.rcol);
		transtest *= bright(p.rcol);
		transdist = r->rot + p.rt;
	}

	if (r->crtype & SHADOW)			/* skip reflected ray */
		return;
						/* compute reflectance */
	for (i = 0; i < 3; i++) {
		d = colval(mcolor, i);
		d *= d;
		colval(refl,i) = r1 * (1.0 + (1.0-2.0*r1)*d) / (1.0 - r1*r1*d);
	}
						/* reflected ray */
	if (rayorigin(&p, r, REFLECTED, bright(refl)) == 0) {
		for (i = 0; i < 3; i++)
			p.rdir[i] = r->rdir[i] + 2.0*pdot*pnorm[i];
		rayvalue(&p);
		multcolor(p.rcol, refl);
		addcolor(r->rcol, p.rcol);
	}
	if (transtest > bright(r->rcol))
		r->rt = transdist;
}
