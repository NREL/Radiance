/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for simulating virtual light sources
 *	Thus far, we only support planar mirrors.
 */

#include  "ray.h"

#include  "otypes.h"

#include  "source.h"

#include  "cone.h"

#include  "face.h"


double  intercircle();
SRCREC  *makevsrc();

static OBJECT  *vobject;		/* virtual source objects */
static int  nvobjects = 0;		/* number of virtual source objects */


markvirtuals()			/* find and mark virtual sources */
{
	register OBJREC  *o;
	register int  i;
					/* check number of direct relays */
	if (directrelay <= 0)
		return;
					/* find virtual source objects */
	for (i = 0; i < nobjects; i++) {
		o = objptr(i);
		if (!issurface(o->otype) || o->omod == OVOID)
			continue;
		if (!isvlight(objptr(o->omod)->otype))
			continue;
		if (sfun[o->otype].of == NULL ||
				sfun[o->otype].of->getpleq == NULL)
			objerror(o, USER, "illegal material");
		if (nvobjects == 0)
			vobject = (OBJECT *)malloc(sizeof(OBJECT));
		else
			vobject = (OBJECT *)realloc((char *)vobject,
				(unsigned)(nvobjects+1)*sizeof(OBJECT));
		if (vobject == NULL)
			error(SYSTEM, "out of memory in addvirtuals");
		vobject[nvobjects++] = i;
	}
	if (nvobjects == 0)
		return;
					/* append virtual sources */
	for (i = nsources; i-- > 0; )
		if (!(source[i].sflags & SSKIP))
			addvirtuals(&source[i], directrelay);
					/* done with our object list */
	free((char *)vobject);
	nvobjects = 0;
}


addvirtuals(sr, nr)		/* add virtual sources associated with sr */
SRCREC  *sr;
int  nr;
{
	register int  i;
				/* check relay limit first */
	if (nr <= 0)
		return;
				/* check each virtual object for projection */
	for (i = 0; i < nvobjects; i++)
					/* vproject() calls us recursively */
		vproject(objptr(i), sr, nr-1);
}


vproject(o, s, n)		/* create projected source(s) if they exist */
OBJREC  *o;
SRCREC  *s;
int  n;
{
	register int  i;
	register VSMATERIAL  *vsmat;
	MAT4  proj;
	SRCREC  *ns;
				/* get virtual source material */
	vsmat = sfun[objptr(o->omod)->otype].mf;
				/* project virtual sources */
	for (i = 0; i < vsmat->nproj; i++)
		if ((*vsmat->vproj)(proj, o, s, i))
			if ((ns = makevsrc(o, s, proj)) != NULL)
				addvirtuals(ns, n);
}


SRCREC *
makevsrc(op, sp, pm)		/* make virtual source if reasonable */
OBJREC  *op;
register SRCREC  *sp;
MAT4  pm;
{
	register SRCREC  *newsrc;
	FVECT  nsloc, ocent, nsnorm;
	int  nsflags;
	double  maxrad2;
	double  d1;
	SPOT  theirspot, ourspot;
	register int  i;

	nsflags = (sp->sflags|(SVIRTUAL|SFOLLOW)) & ~SSPOT;
					/* get object center and max. radius */
	if (sfun[op->otype].of->getdisk != NULL) {
		maxrad2 = (*sfun[op->otype].of->getdisk)(ocent, op);
		if (maxrad2 <= FTINY)			/* too small? */
			return(NULL);
		nsflags |= SSPOT;
	}
					/* get location and spot */
	if (sp->sflags & SDISTANT) {		/* distant source */
		if (sp->sflags & SPROX)
			return(NULL);		/* should never get here! */
		multv3(nsloc, sp->sloc, pm);
		if (nsflags & SSPOT) {
			VCOPY(ourspot.aim, ocent);
			ourspot.siz = PI*maxrad2;
			ourspot.flen = 0.;
		}
		if (sp->sflags & SSPOT) {
			copystruct(&theirspot, sp->sl.s);
			multp3(theirspot.aim, sp->sl.s->aim, pm);
			if (nsflags & SSPOT &&
				!commonbeam(&ourspot, &theirspot, nsloc))
				return(NULL);		/* no overlap */
		}
	} else {				/* local source */
		multp3(nsloc, sp->sloc, pm);
		if (nsflags & SSPOT) {
			for (i = 0; i < 3; i++)
				ourspot.aim[i] = ocent[i] - nsloc[i];
			if ((d1 = normalize(ourspot.aim)) == 0.)
				return(NULL);		/* at source!! */
			if (sp->sflags & SPROX && d1 > sp->sl.prox)
				return(NULL);		/* too far away */
			ourspot.siz = 2.*PI*(1. - d1/sqrt(d1*d1+maxrad2));
			ourspot.flen = 0.;
		} else if (sp->sflags & SPROX) {
			FVECT  norm;
			double  offs;
						/* use distance from plane */
			offs = (*sfun[op->otype].of->getpleq)(norm, op);
			d1 = DOT(norm, nsloc) - offs;
			if (d1 > sp->sl.prox || d1 < -sp->sl.prox)
				return(NULL);		/* too far away */
		}
		if (sp->sflags & SSPOT) {
			copystruct(&theirspot, sp->sl.s);
			multv3(theirspot.aim, sp->sl.s->aim, pm);
			if (nsflags & SSPOT) {
				if (!commonspot(&ourspot, &theirspot, nsloc))
					return(NULL);	/* no overlap */
				ourspot.flen = theirspot.flen;
			}
		}
		if (sp->sflags & SFLAT) {	/* check for behind source */
			multv3(nsnorm, sp->snorm, pm);
			if (nsflags & SSPOT && checkspot(&ourspot, nsnorm) < 0)
				return(NULL);
		}
	}
					/* everything is OK, make source */
	if ((newsrc = newsource()) == NULL)
		goto memerr;
	newsrc->sflags = nsflags;
	VCOPY(newsrc->sloc, nsloc);
	if (nsflags & SFLAT)
		VCOPY(newsrc->snorm, nsnorm);
	newsrc->ss = sp->ss; newsrc->ss2 = sp->ss2;
	if ((nsflags | sp->sflags) & SSPOT) {
		if ((newsrc->sl.s = (SPOT *)malloc(sizeof(SPOT))) == NULL)
			goto memerr;
		if (nsflags & SSPOT)
			copystruct(newsrc->sl.s, &ourspot);
		else
			copystruct(newsrc->sl.s, &theirspot);
		newsrc->sflags |= SSPOT;
	}
	if (nsflags & SPROX)
		newsrc->sl.prox = sp->sl.prox;
	newsrc->sa.svnext = sp - source;
	return(newsrc);
memerr:
	error(SYSTEM, "out of memory in makevsrc");
}


commonspot(sp1, sp2, org)	/* set sp1 to intersection of sp1 and sp2 */
register SPOT  *sp1, *sp2;
FVECT  org;
{
	FVECT  cent;
	double  rad2, cos1, cos2;

	cos1 = 1. - sp1->siz/(2.*PI);
	cos2 = 1. - sp2->siz/(2.*PI);
	if (sp2->siz >= 2.*PI-FTINY)		/* BIG, just check overlap */
		return(DOT(sp1->aim,sp2->aim) >= cos1*cos2 -
					sqrt((1.-cos1*cos1)*(1.-cos2*cos2)));
				/* compute and check disks */
	rad2 = intercircle(cent, sp1->aim, sp2->aim,
			1./(cos1*cos1) - 1.,  1./(cos2*cos2) - 1.);
	if (rad2 <= FTINY || normalize(cent) == 0.)
		return(0);
	VCOPY(sp1->aim, cent);
	sp1->siz = 2.*PI*(1. - 1./sqrt(1.+rad2));
	return(1);
}


commonbeam(sp1, sp2, dir)	/* set sp1 to intersection of sp1 and sp2 */
register SPOT  *sp1, *sp2;
FVECT  dir;
{
	FVECT  cent, c1, c2;
	double  rad2, d;
	register int  i;
					/* move centers to common plane */
	d = DOT(sp1->aim, dir);
	for (i = 0; i < 3; i++)
		c1[i] = sp1->aim[i] - d*dir[i];
	d = DOT(sp2->aim, dir);
	for (i = 0; i < 3; i++)
		c2[i] = sp2->aim[i] - d*dir[i];
					/* compute overlap */
	rad2 = intercircle(cent, c1, c2, sp1->siz/PI, sp2->siz/PI);
	if (rad2 <= FTINY)
		return(0);
	VCOPY(sp1->aim, cent);
	sp1->siz = PI*rad2;
	return(1);
}


checkspot(sp, nrm)		/* check spotlight for behind source */
register SPOT  *sp;
FVECT  nrm;
{
	double  d, d1;

	d = DOT(sp->aim, nrm);
	if (d > FTINY)			/* center in front? */
		return(0);
					/* else check horizon */
	d1 = 1. - sp->siz/(2.*PI);
	return(1.-FTINY-d*d > d1*d1);
}


double
intercircle(cc, c1, c2, r1s, r2s)	/* intersect two circles */
FVECT  cc;			/* midpoint (return value) */
FVECT  c1, c2;			/* circle centers */
double  r1s, r2s;		/* radii squared */
{
	double  a2, d2, l;
	FVECT  disp;
	register int  i;

	for (i = 0; i < 3; i++)
		disp[i] = c2[i] - c1[i];
	d2 = DOT(disp,disp);
					/* circle within overlap? */
	if (r1s < r2s) {
		if (r2s >= r1s + d2) {
			VCOPY(cc, c1);
			return(r1s);
		}
	} else {
		if (r1s >= r2s + d2) {
			VCOPY(cc, c2);
			return(r2s);
		}
	}
	a2 = .25*(2.*(r1s+r2s) - d2 - (r2s-r1s)*(r2s-r1s)/d2);
					/* no overlap? */
	if (a2 <= 0.)
		return(0.);
					/* overlap, compute center */
	l = sqrt((r1s - a2)/d2);
	for (i = 0; i < 3; i++)
		cc[i] = c1[i] + l*disp[i];
	return(a2);
}
