/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  source.c - routines dealing with illumination sources.
 *
 *     8/20/85
 */

#include  "ray.h"

#include  "octree.h"

#include  "otypes.h"

#include  "source.h"

#include  "random.h"

/*
 * Structures used by direct()
 */

typedef struct {
	FVECT  dir;		/* source direction */
	COLOR  coef;		/* material coefficient */
	COLOR  val;		/* contribution */
}  CONTRIB;		/* direct contribution */

typedef struct {
	int  sno;		/* source number */
	float  brt;		/* brightness (for comparison) */
}  CNTPTR;		/* contribution pointer */

static CONTRIB  *srccnt;		/* source contributions in direct() */
static CNTPTR  *cntord;			/* source ordering in direct() */


marksources()			/* find and mark source objects */
{
	int  i;
	register OBJREC  *o, *m;
	register int  ns;
					/* initialize dispatch table */
	initstypes();
					/* find direct sources */
	for (i = 0; i < nobjects; i++) {
	
		o = objptr(i);

		if (!issurface(o->otype) || o->omod == OVOID)
			continue;

		m = objptr(o->omod);

		if (!islight(m->otype))
			continue;
	
		if (m->oargs.nfargs != (m->otype == MAT_GLOW ? 4 :
				m->otype == MAT_SPOT ? 7 : 3))
			objerror(m, USER, "bad # arguments");

		if (m->otype == MAT_GLOW &&
				o->otype != OBJ_SOURCE &&
				m->oargs.farg[3] <= FTINY)
			continue;			/* don't bother */

		if (sfun[o->otype].of == NULL ||
				sfun[o->otype].of->setsrc == NULL)
			objerror(o, USER, "illegal material");

		if ((ns = newsource()) < 0)
			goto memerr;

		setsource(&source[ns], o);

		if (m->otype == MAT_GLOW) {
			source[ns].sflags |= SPROX;
			source[ns].sl.prox = m->oargs.farg[3];
			if (o->otype == OBJ_SOURCE)
				source[ns].sflags |= SSKIP;
		} else if (m->otype == MAT_SPOT) {
			source[ns].sflags |= SSPOT;
			if ((source[ns].sl.s = makespot(m)) == NULL)
				goto memerr;
			if (source[ns].sflags & SFLAT &&
				!checkspot(source[ns].sl.s,source[ns].snorm)) {
				objerror(o, WARNING,
					"invalid spotlight direction");
				source[ns].sflags |= SSKIP;
			}
		}
	}
	if (nsources <= 0) {
		error(WARNING, "no light sources found");
		return;
	}
	markvirtuals();			/* find and add virtual sources */
	srccnt = (CONTRIB *)malloc(nsources*sizeof(CONTRIB));
	cntord = (CNTPTR *)malloc(nsources*sizeof(CNTPTR));
	if (srccnt == NULL || cntord == NULL)
		goto memerr;
	return;
memerr:
	error(SYSTEM, "out of memory in marksources");
}


double
srcray(sr, r, sn)		/* send a ray to a source, return domega */
register RAY  *sr;		/* returned source ray */
RAY  *r;			/* ray which hit object */
register int  sn;		/* source number */
{
	double  ddot;			/* (distance times) cosine */
	FVECT  vd;
	double  d;
	register int  i;

	if (source[sn].sflags & SSKIP)
		return(0.0);			/* skip this source */

	rayorigin(sr, r, SHADOW, 1.0);		/* ignore limits */

	sr->rsrc = sn;				/* remember source */
						/* get source direction */
	if (source[sn].sflags & SDISTANT) {
						/* constant direction */
		VCOPY(sr->rdir, source[sn].sloc);
	} else {				/* compute direction */
		for (i = 0; i < 3; i++)
			sr->rdir[i] = source[sn].sloc[i] - sr->rorg[i];

		if (source[sn].sflags & SFLAT &&
			(ddot = -DOT(sr->rdir, source[sn].snorm)) <= FTINY)
			return(0.0);		/* behind surface! */
	}
	if (dstrsrc > FTINY) {
					/* distribute source direction */
		dimlist[ndims++] = sn;
		for (i = 0; i < 3; i++) {
			dimlist[ndims] = i + 8831;
			vd[i] = dstrsrc * source[sn].ss *
		(1.0 - 2.0*urand(urind(ilhash(dimlist,ndims+1),samplendx)));
		}
		ndims--;
		if (source[sn].sflags & SFLAT) {	/* project offset */
			d = DOT(vd, source[sn].snorm);
			for (i = 0; i < 3; i++)
				vd[i] -= d * source[sn].snorm[i];
		}
		for (i = 0; i < 3; i++)		/* offset source direction */
			sr->rdir[i] += vd[i];
						/* normalize */
		d = normalize(sr->rdir);

	} else if (!(source[sn].sflags & SDISTANT))
						/* normalize direction */
		d = normalize(sr->rdir);

	if (source[sn].sflags & SDISTANT) {
		if (source[sn].sflags & SSPOT) {	/* check location */
			for (i = 0; i < 3; i++)
				vd[i] = sr->rorg[i] - source[sn].sl.s->aim[i];
			d = DOT(sr->rdir,vd);
			d = DOT(vd,vd) - d*d;
			if (PI*d > source[sn].sl.s->siz)
				return(0.0);
		}
		return(source[sn].ss2);		/* domega constant */
	}
						/* check direction */
	if (d == 0.0)
		return(0.0);
						/* check proximity */
	if (source[sn].sflags & SPROX &&
			d > source[sn].sl.prox)
		return(0.0);
						/* compute dot product */
	if (source[sn].sflags & SFLAT)
		ddot /= d;
	else
		ddot = 1.0;
						/* check angle */
	if (source[sn].sflags & SSPOT) {
		if (source[sn].sl.s->siz < 2.0*PI *
				(1.0 + DOT(source[sn].sl.s->aim,sr->rdir)))
			return(0.0);
		d += source[sn].sl.s->flen;	/* adjust length */
	}
						/* compute domega */
	return(ddot*source[sn].ss2/(d*d));
}


srcvalue(r)			/* punch ray to source and compute value */
RAY  *r;
{
	register SRCREC  *sp;

	sp = &source[r->rsrc];
	if (sp->sflags & SVIRTUAL) {	/* virtual source */
					/* check intersection */
		if (!(*ofun[sp->so->otype].funp)(sp->so, r))
			return;
		raycont(r);		/* compute contribution */
		return;
	}
					/* compute intersection */
	if (sp->sflags & SDISTANT ? sourcehit(r) :
			(*ofun[sp->so->otype].funp)(sp->so, r)) {
		if (sp->sa.success >= 0)
			sp->sa.success++;
		raycont(r);		/* compute contribution */
		return;
	}
	if (sp->sa.success < 0)
		return;			/* bitched already */
	sp->sa.success -= AIMREQT;
	if (sp->sa.success >= 0)
		return;			/* leniency */
	sprintf(errmsg, "aiming failure for light source \"%s\"",
			sp->so->oname);
	error(WARNING, errmsg);		/* issue warning */
}


static int
cntcmp(sc1, sc2)			/* contribution compare (descending) */
register CNTPTR  *sc1, *sc2;
{
	if (sc1->brt > sc2->brt)
		return(-1);
	if (sc1->brt < sc2->brt)
		return(1);
	return(0);
}


direct(r, f, p)				/* add direct component */
RAY  *r;			/* ray that hit surface */
int  (*f)();			/* direct component coefficient function */
char  *p;			/* data for f */
{
	extern double  pow();
	register int  sn;
	int  nshadcheck, ncnts;
	int  nhits;
	double  dom, prob, ourthresh, hwt;
	RAY  sr;
			/* NOTE: srccnt and cntord global so no recursion */
	if (nsources <= 0)
		return;		/* no sources?! */
						/* compute number to check */
	nshadcheck = pow((double)nsources, shadcert) + .5;
						/* modify threshold */
	ourthresh = shadthresh / r->rweight;
						/* potential contributions */
	for (sn = 0; sn < nsources; sn++) {
		cntord[sn].sno = sn;
		cntord[sn].brt = 0.0;
						/* get source ray */
		if ((dom = srcray(&sr, r, sn)) == 0.0)
			continue;
		VCOPY(srccnt[sn].dir, sr.rdir);
						/* compute coefficient */
		(*f)(srccnt[sn].coef, p, srccnt[sn].dir, dom);
		cntord[sn].brt = bright(srccnt[sn].coef);
		if (cntord[sn].brt <= 0.0)
			continue;
						/* compute potential */
		sr.revf = srcvalue;
		rayvalue(&sr);
		copycolor(srccnt[sn].val, sr.rcol);
		multcolor(srccnt[sn].val, srccnt[sn].coef);
		cntord[sn].brt = bright(srccnt[sn].val);
	}
						/* sort contributions */
	qsort(cntord, nsources, sizeof(CNTPTR), cntcmp);
	{					/* find last */
		register int  l, m;

		sn = 0; ncnts = l = nsources;
		while ((m = (sn + ncnts) >> 1) != l) {
			if (cntord[m].brt > 0.0)
				sn = m;
			else
				ncnts = m;
			l = m;
		}
	}
                                                /* accumulate tail */
        for (sn = ncnts-1; sn > 0; sn--)
                cntord[sn-1].brt += cntord[sn].brt;
						/* test for shadows */
	nhits = 0;
	for (sn = 0; sn < ncnts; sn++) {
						/* check threshold */
		if ((sn+nshadcheck>=ncnts ? cntord[sn].brt :
				cntord[sn].brt-cntord[sn+nshadcheck].brt)
				< ourthresh*bright(r->rcol))
			break;
						/* get statistics */
		source[cntord[sn].sno].ntests++;
						/* test for hit */
		rayorigin(&sr, r, SHADOW, 1.0);
		VCOPY(sr.rdir, srccnt[cntord[sn].sno].dir);
		sr.rsrc = cntord[sn].sno;
		if (localhit(&sr, &thescene) &&
				( sr.ro != source[cntord[sn].sno].so ||
				source[cntord[sn].sno].sflags & SFOLLOW )) {
						/* follow entire path */
			raycont(&sr);
			if (bright(sr.rcol) <= FTINY)
				continue;	/* missed! */
			copycolor(srccnt[cntord[sn].sno].val, sr.rcol);
			multcolor(srccnt[cntord[sn].sno].val,
					srccnt[cntord[sn].sno].coef);
		}
						/* add contribution if hit */
		addcolor(r->rcol, srccnt[cntord[sn].sno].val);
		nhits++;
		source[cntord[sn].sno].nhits++;
	}
					/* surface hit rate */
	if (sn > 0)
		hwt = (double)nhits / (double)sn;
	else
		hwt = 0.5;
#ifdef DEBUG
	sprintf(errmsg, "%d tested, %d untested, %f hit rate\n",
			sn, ncnts-sn, hwt);
	eputs(errmsg);
#endif
					/* add in untested sources */
	for ( ; sn < ncnts; sn++) {
		prob = hwt * (double)source[cntord[sn].sno].nhits /
				(double)source[cntord[sn].sno].ntests;
		scalecolor(srccnt[cntord[sn].sno].val, prob);
		addcolor(r->rcol, srccnt[cntord[sn].sno].val);
	}
}
