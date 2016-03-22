#ifndef lint
static const char RCSid[] = "$Id$";
#endif
/*
 *  o_instance.c - routines for computing ray intersections with octrees.
 */

#include "copyright.h"

#include  "ray.h"
#include  "instance.h"
#include  "rtotypes.h"


int
o_instance(		/* compute ray intersection with octree */
	OBJREC  *o,
	RAY  *r
)
{
	RAY  rcont;
	double  d;
	INSTANCE  *ins;
	int  i;
					/* get the octree */
	ins = getinstance(o, IO_ALL);
					/* copy and transform ray */
	rcont = *r;
	multp3(rcont.rorg, r->rorg, ins->x.b.xfm);
	multv3(rcont.rdir, r->rdir, ins->x.b.xfm);
	for (i = 0; i < 3; i++)
		rcont.rdir[i] /= ins->x.b.sca;
	rcont.rmax *= ins->x.b.sca;
					/* clear and trace it */
	rayclear(&rcont);
	if (!localhit(&rcont, &ins->obj->scube))
		return(0);			/* missed */
	if (rcont.rot * ins->x.f.sca >= r->rot)
		return(0);			/* not close enough */

	if (o->omod != OVOID) {		/* if we have modifier, use it */
		r->ro = o;
		r->rox = NULL;
	} else {			/* else use theirs */
		r->ro = rcont.ro;
		if (rcont.rox != NULL) {
			newrayxf(r);		/* allocate transformation */
					/* NOTE: r->rox may equal rcont.rox! */
			multmat4(r->rox->f.xfm, rcont.rox->f.xfm, ins->x.f.xfm);
			r->rox->f.sca = rcont.rox->f.sca * ins->x.f.sca;
			multmat4(r->rox->b.xfm, ins->x.b.xfm, rcont.rox->b.xfm);
			r->rox->b.sca = ins->x.b.sca * rcont.rox->b.sca;
		} else
			r->rox = &ins->x;
	}
					/* transform it back */
	r->rot = rcont.rot * ins->x.f.sca;
	multp3(r->rop, rcont.rop, ins->x.f.xfm);
	multv3(r->ron, rcont.ron, ins->x.f.xfm);
	multv3(r->pert, rcont.pert, ins->x.f.xfm);
	d = 1./ins->x.f.sca;
	for (i = 0; i < 3; i++) {
		r->ron[i] *= d;
		r->pert[i] *= d;
	}
	r->rod = rcont.rod;
	r->uv[0] = rcont.uv[0];
	r->uv[1] = rcont.uv[1];
					/* return hit */
	return(1);
}
