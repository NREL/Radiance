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


double  intercircle();

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
#ifdef DEBUG
	fprintf(stderr, "found %d virtual source objects\n", nvobjects);
#endif
					/* append virtual sources */
	for (i = nsources; i-- > 0; )
		if (!(source[i].sflags & SSKIP))
			addvirtuals(i, directrelay);
					/* done with our object list */
	free((char *)vobject);
	nvobjects = 0;
}


addvirtuals(sn, nr)		/* add virtuals associated with source */
int  sn;
int  nr;
{
	register int  i;
				/* check relay limit first */
	if (nr <= 0)
		return;
				/* check each virtual object for projection */
	for (i = 0; i < nvobjects; i++)
					/* vproject() calls us recursively */
		vproject(objptr(vobject[i]), sn, nr-1);
}


vproject(o, sn, n)		/* create projected source(s) if they exist */
OBJREC  *o;
int  sn;
int  n;
{
	register int  i;
	register VSMATERIAL  *vsmat;
	MAT4  proj;
	int  ns;

	if (o == source[sn].so)	/* objects cannot project themselves */
		return;
				/* get virtual source material */
	vsmat = sfun[objptr(o->omod)->otype].mf;
				/* project virtual sources */
	for (i = 0; i < vsmat->nproj; i++)
		if ((*vsmat->vproj)(proj, o, &source[sn], i))
			if ((ns = makevsrc(o, sn, proj)) >= 0) {
#ifdef DEBUG
				virtverb(&source[sn], stderr);
#endif
				addvirtuals(ns, n);
			}
}


int
makevsrc(op, sn, pm)		/* make virtual source if reasonable */
OBJREC  *op;
register int  sn;
MAT4  pm;
{
	register int  nsn;
	FVECT  nsloc, ocent, nsnorm;
	int  nsflags;
	double  maxrad2;
	double  d1;
	SPOT  theirspot, ourspot;
	register int  i;

	nsflags = (source[sn].sflags|(SVIRTUAL|SFOLLOW)) & ~SSPOT;
					/* get object center and max. radius */
	if (sfun[op->otype].of->getdisk != NULL) {
		maxrad2 = (*sfun[op->otype].of->getdisk)(ocent, op);
		if (maxrad2 <= FTINY)			/* too small? */
			return(NULL);
		nsflags |= SSPOT;
	}
					/* get location and spot */
	if (source[sn].sflags & SDISTANT) {		/* distant source */
		if (source[sn].sflags & SPROX)
			return(NULL);		/* should never get here! */
		multv3(nsloc, source[sn].sloc, pm);
		if (nsflags & SSPOT) {
			VCOPY(ourspot.aim, ocent);
			ourspot.siz = PI*maxrad2;
			ourspot.flen = 0.;
		}
		if (source[sn].sflags & SSPOT) {
			copystruct(&theirspot, source[sn].sl.s);
			multp3(theirspot.aim, source[sn].sl.s->aim, pm);
			if (nsflags & SSPOT &&
				!commonbeam(&ourspot, &theirspot, nsloc))
				return(NULL);		/* no overlap */
		}
	} else {				/* local source */
		multp3(nsloc, source[sn].sloc, pm);
		if (nsflags & SSPOT) {
			for (i = 0; i < 3; i++)
				ourspot.aim[i] = ocent[i] - nsloc[i];
			if ((d1 = normalize(ourspot.aim)) == 0.)
				return(NULL);		/* at source!! */
			if (source[sn].sflags & SPROX &&
					d1 > source[sn].sl.prox)
				return(NULL);		/* too far away */
			ourspot.siz = 2.*PI*(1. - d1/sqrt(d1*d1+maxrad2));
			ourspot.flen = 0.;
		} else if (source[sn].sflags & SPROX) {
			FVECT  norm;
			double  offs;
						/* use distance from plane */
			offs = (*sfun[op->otype].of->getpleq)(norm, op);
			d1 = DOT(norm, nsloc) - offs;
			if (d1 < 0.) d1 = -d1;
			if (d1 > source[sn].sl.prox)
				return(NULL);		/* too far away */
		}
		if (source[sn].sflags & SSPOT) {
			copystruct(&theirspot, source[sn].sl.s);
			multv3(theirspot.aim, source[sn].sl.s->aim, pm);
			if (nsflags & SSPOT) {
				if (!commonspot(&ourspot, &theirspot, nsloc))
					return(NULL);	/* no overlap */
				ourspot.flen = theirspot.flen;
			}
		}
		if (source[sn].sflags & SFLAT) {	/* behind source? */
			multv3(nsnorm, source[sn].snorm, pm);
			if (nsflags & SSPOT && checkspot(&ourspot, nsnorm) < 0)
				return(NULL);
		}
	}
					/* everything is OK, make source */
	if ((nsn = newsource()) < 0)
		goto memerr;
	source[nsn].sflags = nsflags;
	VCOPY(source[nsn].sloc, nsloc);
	if (nsflags & SFLAT)
		VCOPY(source[nsn].snorm, nsnorm);
	source[nsn].ss = source[sn].ss; source[nsn].ss2 = source[sn].ss2;
	if ((nsflags | source[sn].sflags) & SSPOT) {
		if ((source[nsn].sl.s = (SPOT *)malloc(sizeof(SPOT))) == NULL)
			goto memerr;
		if (nsflags & SSPOT)
			copystruct(source[nsn].sl.s, &ourspot);
		else
			copystruct(source[nsn].sl.s, &theirspot);
		source[nsn].sflags |= SSPOT;
	}
	if (nsflags & SPROX)
		source[nsn].sl.prox = source[sn].sl.prox;
	source[nsn].sa.svnext = sn;
	source[nsn].so = op;
	return(nsn);
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


#ifdef DEBUG
virtverb(vs, fp)	/* print verbose description of virtual source */
register SRCREC  *vs;
FILE  *fp;
{
	register int  i;

	fprintf(fp, "%s virtual source %d in %s %s\n",
			vs->sflags & SDISTANT ? "distant" : "local",
			vs-source, ofun[vs->so->otype].funame, vs->so->oname);
	fprintf(fp, "\tat (%f,%f,%f)\n",
			vs->sloc[0], vs->sloc[1], vs->sloc[2]);
	fprintf(fp, "\tlinked to source %d (%s)\n",
			vs->sa.svnext, source[vs->sa.svnext].so->oname);
	if (vs->sflags & SFOLLOW)
		fprintf(fp, "\talways followed\n");
	else
		fprintf(fp, "\tnever followed\n");
	if (!(vs->sflags & SSPOT))
		return;
	fprintf(fp, "\twith spot aim (%f,%f,%f) and size %f\n",
			vs->sl.s->aim[0], vs->sl.s->aim[1], vs->sl.s->aim[2],
			vs->sl.s->siz);
}
#endif
