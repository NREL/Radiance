/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  source.c - routines dealing with illumination sources.
 *
 *     8/20/85
 */

#include  "ray.h"

#include  "source.h"

#include  "otypes.h"

#include  "cone.h"

#include  "face.h"

#include  "random.h"


extern double  dstrsrc;			/* source distribution amount */

SOURCE  srcval[MAXSOURCE];		/* our array of sources */
int  nsources = 0;			/* the number of sources */


marksources()			/* find and mark source objects */
{
	register OBJREC  *o, *m;
	register int  i;

	for (i = 0; i < nobjects; i++) {
	
		o = objptr(i);

		if (o->omod == OVOID)
			continue;

		m = objptr(o->omod);

		if (m->otype != MAT_LIGHT &&
				m->otype != MAT_ILLUM &&
				m->otype != MAT_GLOW &&
				m->otype != MAT_SPOT)
			continue;
	
		if (m->oargs.nfargs != (m->otype == MAT_GLOW ? 4 :
				m->otype == MAT_SPOT ? 7 : 3))
			objerror(m, USER, "bad # arguments");

		if (m->otype == MAT_GLOW &&
				o->otype != OBJ_SOURCE &&
				m->oargs.farg[3] <= FTINY)
			continue;			/* don't bother */

		if (nsources >= MAXSOURCE)
			error(INTERNAL, "too many sources in marksources");

		newsource(&srcval[nsources], o);

		if (m->otype == MAT_GLOW) {
			srcval[nsources].sflags |= SPROX;
			srcval[nsources].sl.prox = m->oargs.farg[3];
			if (o->otype == OBJ_SOURCE)
				srcval[nsources].sflags |= SSKIP;
		} else if (m->otype == MAT_SPOT) {
			srcval[nsources].sflags |= SSPOT;
			srcval[nsources].sl.s = makespot(m);
		}
		nsources++;
	}
}


newsource(src, so)			/* add a source to the array */
register SOURCE  *src;
register OBJREC  *so;
{
	double  cos(), tan(), sqrt();
	double  theta;
	FACE  *f;
	CONE  *co;
	int  j;
	register int  i;
	
	src->sflags = 0;
	src->so = so;

	switch (so->otype) {
	case OBJ_SOURCE:
		if (so->oargs.nfargs != 4)
			objerror(so, USER, "bad arguments");
		src->sflags |= SDISTANT;
		VCOPY(src->sloc, so->oargs.farg);
		if (normalize(src->sloc) == 0.0)
			objerror(so, USER, "zero direction");
		theta = PI/180.0/2.0 * so->oargs.farg[3];
		if (theta <= FTINY)
			objerror(so, USER, "zero size");
		src->ss = theta >= PI/4 ? 1.0 : tan(theta);
		src->ss2 = 2.0*PI * (1.0 - cos(theta));
		break;
	case OBJ_SPHERE:
		VCOPY(src->sloc, so->oargs.farg);
		src->ss = so->oargs.farg[3];
		src->ss2 = PI * src->ss * src->ss;
		break;
	case OBJ_FACE:
						/* get the face */
		f = getface(so);
						/* find the center */
		for (j = 0; j < 3; j++) {
			src->sloc[j] = 0.0;
			for (i = 0; i < f->nv; i++)
				src->sloc[j] += VERTEX(f,i)[j];
			src->sloc[j] /= f->nv;
		}
		if (!inface(src->sloc, f))
			objerror(so, USER, "cannot hit center");
		src->ss = sqrt(f->area / PI);
		src->ss2 = f->area;
		break;
	case OBJ_RING:
						/* get the ring */
		co = getcone(so, 0);
		VCOPY(src->sloc, CO_P0(co));
		if (CO_R0(co) > 0.0)
			objerror(so, USER, "cannot hit center");
		src->ss = CO_R1(co);
		src->ss2 = PI * src->ss * src->ss;
		break;
	default:
		objerror(so, USER, "illegal material");
	}
}


SPOT *
makespot(m)			/* make a spotlight */
register OBJREC  *m;
{
	extern double  cos();
	register SPOT  *ns;

	if ((ns = (SPOT *)malloc(sizeof(SPOT))) == NULL)
		error(SYSTEM, "out of memory in makespot");
	ns->siz = 2.0*PI * (1.0 - cos(PI/180.0/2.0 * m->oargs.farg[3]));
	VCOPY(ns->aim, m->oargs.farg+4);
	if ((ns->flen = normalize(ns->aim)) == 0.0)
		objerror(m, USER, "zero focus vector");
	return(ns);
}


double
srcray(sr, r, sn)		/* send a ray to a source, return domega */
register RAY  *sr;		/* returned source ray */
RAY  *r;			/* ray which hit object */
register int  sn;		/* source number */
{
	register double  *norm = NULL;	/* plane normal */
	double  ddot;			/* (distance times) cosine */
	FVECT  vd;
	double  d;
	register int  i;

	if (srcval[sn].sflags & SSKIP)
		return(0.0);			/* skip this source */

	rayorigin(sr, r, SHADOW, 1.0);		/* ignore limits */

	sr->rsrc = sn;				/* remember source */
						/* get source direction */
	if (srcval[sn].sflags & SDISTANT)
						/* constant direction */
		VCOPY(sr->rdir, srcval[sn].sloc);
	else {					/* compute direction */
		for (i = 0; i < 3; i++)
			sr->rdir[i] = srcval[sn].sloc[i] - sr->rorg[i];

		if (srcval[sn].so->otype == OBJ_FACE)
 			norm = getface(srcval[sn].so)->norm;
		else if (srcval[sn].so->otype == OBJ_RING)
			norm = getcone(srcval[sn].so,0)->ad;

		if (norm != NULL && (ddot = -DOT(sr->rdir, norm)) <= FTINY)
			return(0.0);		/* behind surface! */
	}
	if (dstrsrc > FTINY) {
					/* distribute source direction */
		for (i = 0; i < 3; i++)
			vd[i] = dstrsrc * srcval[sn].ss * (1.0 - 2.0*frandom());

		if (norm != NULL) {		/* project offset */
			d = DOT(vd, norm);
			for (i = 0; i < 3; i++)
				vd[i] -= d * norm[i];
		}
		for (i = 0; i < 3; i++)		/* offset source direction */
			sr->rdir[i] += vd[i];

	} else if (srcval[sn].sflags & SDISTANT)
						/* already normalized */
		return(srcval[sn].ss2);

	if ((d = normalize(sr->rdir)) == 0.0)
						/* at source! */
		return(0.0);
	
	if (srcval[sn].sflags & SDISTANT)
						/* domega constant */
		return(srcval[sn].ss2);

	else {
						/* check proximity */
		if (srcval[sn].sflags & SPROX &&
				d > srcval[sn].sl.prox)
			return(0.0);

		if (norm != NULL)
			ddot /= d;
		else
			ddot = 1.0;
						/* check angle */
		if (srcval[sn].sflags & SSPOT) {
			if (srcval[sn].sl.s->siz < 2.0*PI *
				(1.0 + DOT(srcval[sn].sl.s->aim,sr->rdir)))
				return(0.0);
			d += srcval[sn].sl.s->flen;
		}
						/* return domega */
		return(ddot*srcval[sn].ss2/(d*d));
	}
}


sourcehit(r)			/* check to see if ray hit distant source */
register RAY  *r;
{
	int  first, last;
	register int  i;

	if (r->rsrc >= 0) {		/* check only one if aimed */
		first = last = r->rsrc;
	} else {			/* otherwise check all */
		first = 0; last = nsources-1;
	}
	for (i = first; i <= last; i++)
		if (srcval[i].sflags & SDISTANT)
			/*
			 * Check to see if ray is within
			 * solid angle of source.
			 */
			if (2.0*PI * (1.0 - DOT(srcval[i].sloc,r->rdir))
					<= srcval[i].ss2) {
				r->ro = srcval[i].so;
				if (!(srcval[i].sflags & SSKIP))
					break;
			}

	if (r->ro != NULL) {
		for (i = 0; i < 3; i++)
			r->ron[i] = -r->rdir[i];
		r->rod = 1.0;
		r->rofs = 1.0; setident4(r->rofx);
		r->robs = 1.0; setident4(r->robx);
		return(1);
	}
	return(0);
}


#define  wrongsource(m, r)	(m->otype!=MAT_ILLUM && \
				r->rsrc>=0 && \
				srcval[r->rsrc].so!=r->ro)

#define  badambient(m, r)	((r->crtype&(AMBIENT|SHADOW))==AMBIENT && \
				!(r->rtype&REFLECTED) && 	/* hack! */\
				!(m->otype==MAT_GLOW&&r->rot>m->oargs.farg[3]))

#define  passillum(m, r)	(m->otype==MAT_ILLUM && \
				!(r->rsrc>=0&&srcval[r->rsrc].so==r->ro))


m_light(m, r)			/* ray hit a light source */
register OBJREC  *m;
register RAY  *r;
{
						/* check for behind */
	if (r->rod < 0.0)
		return;
						/* check for over-counting */
	if (wrongsource(m, r) || badambient(m, r))
		return;
						/* check for passed illum */
	if (passillum(m, r)) {

		if (m->oargs.nsargs < 1 || !strcmp(m->oargs.sarg[0], VOIDID))
			raytrans(r);
		else
			rayshade(r, modifier(m->oargs.sarg[0]));

						/* otherwise treat as source */
	} else {
						/* get distribution pattern */
		raytexture(r, m->omod);
						/* get source color */
		setcolor(r->rcol, m->oargs.farg[0],
				  m->oargs.farg[1],
				  m->oargs.farg[2]);
						/* modify value */
		multcolor(r->rcol, r->pcol);
	}
}


o_source() {}		/* intersection with a source is done elsewhere */
