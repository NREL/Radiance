/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  o_instance.c - routines for computing ray intersections with octrees.
 *
 *     11/11/88
 */

#include  "ray.h"

#include  "instance.h"


o_instance(o, r)		/* compute ray intersection with octree */
OBJREC  *o;
register RAY  *r;
{
	extern long  nrays;
	RAY  rcont;
	register INSTANCE  *in;
	register int  i;
					/* get the octree */
	in = getinstance(o, GET_ALL);
					/* copy old ray */
	bcopy(r, &rcont, sizeof(RAY));
					/* transform it */
	rcont.rno = nrays;
	rcont.ro = NULL;
	rcont.rot = FHUGE;
	multp3(rcont.rorg, r->rorg, in->b.xfm);
	multv3(rcont.rdir, r->rdir, in->b.xfm);
	for (i = 0; i < 3; i++)
		rcont.rdir[i] /= in->b.sca;
					/* trace it */
	if (!localhit(&rcont, &in->obj->scube))
		return(0);		/* missed */
	if (rcont.rot * in->f.sca >= r->rot)
		return(0);		/* not close enough */

	if (o->omod != OVOID) {		/* if we have modifier, use it */
		r->ro = o;
		r->rofs = 1.0; setident4(r->rofx);
		r->robs = 1.0; setident4(r->robx);
	} else {			/* else use theirs */
		r->ro = rcont.ro;
		multmat4(r->rofx, rcont.rofx, in->f.xfm);
		r->rofs = rcont.rofs * in->f.sca;
		multmat4(r->robx, in->b.xfm, rcont.robx);
		r->robs = in->b.sca * rcont.robs;
	}
					/* transform it back */
	r->rot = rcont.rot * in->f.sca;
	multp3(r->rop, rcont.rop, in->f.xfm);
	multv3(r->ron, rcont.ron, in->f.xfm);
	for (i = 0; i < 3; i++)
		r->ron[i] /= in->f.sca;
	r->rod = rcont.rod;
					/* return hit */
	return(1);
}
