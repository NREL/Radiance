/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for mirror material supporting virtual light sources
 */

#include  "ray.h"

#include  "otypes.h"

#include  "source.h"

/*
 * The real arguments for MAT_MIRROR are simply:
 *
 *	3 rrefl grefl brefl
 *
 * Additionally, the user may specify a single string argument
 * which is interpreted as the name of the material to use
 * instead of the mirror if the ray being considered is not
 * part of the direct calculation.
 */


int  mir_proj();
VSMATERIAL  mirror_vs = {mir_proj, 1};


m_mirror(m, r)			/* shade mirrored ray */
register OBJREC  *m;
register RAY  *r;
{
	COLOR  mcolor;
	RAY  nr;
	register int  i;
					/* check arguments */
	if (m->oargs.nfargs != 3 || m->oargs.nsargs > 1)
		objerror(m, USER, "bad number of arguments");
					/* check for substitute material */
	if (m->oargs.nsargs > 0 &&
			(r->rsrc < 0 || source[r->rsrc].so != r->ro)) {
		if (!strcmp(m->oargs.sarg[0], VOIDID)) {
			raytrans(r);
			return(1);
		}
		return(rayshade(r, modifier(m->oargs.sarg[0])));
	}
					/* check for bad source ray */
	if (r->rsrc >= 0 && source[r->rsrc].so != r->ro)
		return(1);

	if (r->rod < 0.)		/* back is black */
		return(1);
					/* get modifiers */
	raytexture(r, m->omod);
					/* assign material color */
	setcolor(mcolor, m->oargs.farg[0],
			m->oargs.farg[1],
			m->oargs.farg[2]);
	multcolor(mcolor, r->pcol);
					/* compute reflected ray */
	if (r->rsrc >= 0) {			/* relayed light source */
		rayorigin(&nr, r, REFLECTED, 1.);
					/* ignore textures */
		for (i = 0; i < 3; i++)
			nr.rdir[i] = r->rdir[i] + 2.*r->rod*r->ron[i];
					/* source we're aiming for next */
		nr.rsrc = source[r->rsrc].sa.sv.sn;
	} else {				/* ordinary reflection */
		FVECT  pnorm;
		double  pdot;

		if (rayorigin(&nr, r, REFLECTED, bright(mcolor)) < 0)
			return(1);
		pdot = raynormal(pnorm, r);	/* use textures */
		for (i = 0; i < 3; i++)
			nr.rdir[i] = r->rdir[i] + 2.*pdot*pnorm[i];
						/* check for penetration */
		if (DOT(nr.rdir, r->ron) <= FTINY)
			for (i = 0; i < 3; i++)
				nr.rdir[i] = r->rdir[i] + 2.*r->rod*r->ron[i];
	}
	rayvalue(&nr);
	multcolor(nr.rcol, mcolor);
	addcolor(r->rcol, nr.rcol);
	return(1);
}


mir_proj(pm, o, s, n)		/* compute a mirror's projection */
MAT4  pm;
register OBJREC  *o;
SRCREC  *s;
int  n;
{
	FVECT  nv;
	double  od;
				/* get surface normal and offset */
	od = getplaneq(nv, o);
				/* check for behind */
	if (DOT(s->sloc, nv) <= (s->sflags & SDISTANT ? FTINY : od+FTINY))
		return(0);
				/* everything OK -- compute projection */
	mirrorproj(pm, nv, od);
	return(1);
}


mirrorproj(m, nv, offs)		/* get mirror projection for surface */
register MAT4  m;
FVECT  nv;
double  offs;
{
	register int  i, j;
					/* assign matrix */
	setident4(m);
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			m[i][j] -= 2.*nv[i]*nv[j];
		m[3][j] = 2.*offs*nv[j];
	}
}
