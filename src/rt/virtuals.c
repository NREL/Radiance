/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for simulating virtual light sources
 *	Thus far, we only support planar mirrors.
 */

#include  "ray.h"

#include  "source.h"

#include  "otypes.h"

#include  "cone.h"

#include  "face.h"

extern int  directrelay;		/* maximum number of source relays */

double  getplaneq();
double  getmaxdisk();
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
		if (o->omod == OVOID)
			continue;
		if (!isvlight(objptr(o->omod)->otype))
			continue;
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
		vproject(objptr(i), sr, nr-1);	/* calls us recursively */
}


SRCREC *
makevsrc(op, sp, pm)		/* make virtual source if reasonable */
OBJREC  *op;
register SRCREC  *sp;
MAT4  pm;
{
	register SRCREC  *newsrc;
	FVECT  nsloc, ocent, nsnorm;
	double  maxrad2;
	double  d1, d2;
	SPOT  theirspot, ourspot;
	register int  i;
					/* get object center and max. radius */
	maxrad2 = getmaxdisk(ocent, op);
	if (maxrad2 <= FTINY)			/* too small? */
		return(NULL);
					/* get location and spot */
	if (sp->sflags & SDISTANT) {		/* distant source */
		if (sp->sflags & SPROX)
			return(NULL);		/* should never get here! */
		multv3(nsloc, sp->sloc, pm);
		VCOPY(ourspot.aim, ocent);
		ourspot.siz = PI*maxrad2;
		ourspot.flen = 0.;
		if (sp->sflags & SSPOT) {
			copystruct(&theirspot, sp->sl.s);
			multp3(theirspot.aim, sp->sl.s->aim, pm);
			if (!commonbeam(&ourspot, &theirspot, nsloc))
				return(NULL);		/* no overlap */
		}
	} else {				/* local source */
		multp3(nsloc, sp->sloc, pm);
		if (sp->sflags & SPROX) {
			d2 = 0.;
			for (i = 0; i < 3; i++) {
				d1 = ocent[i] - nsloc[i];
				d2 += d1*d1;
			}
			if (d2 > sp->sl.prox*sp->sl.prox)
				return(NULL);	/* too far away */
		}
		for (i = 0; i < 3; i++)
			ourspot.aim[i] = ocent[i] - nsloc[i];
		if ((d1 = normalize(ourspot.aim)) == 0.)
			return(NULL);		/* at source!! */
		ourspot.siz = 2.*PI*(1. - d1/sqrt(d1*d1+maxrad2));
		ourspot.flen = 0.;
		if (sp->sflags & SSPOT) {
			copystruct(&theirspot, sp->sl.s);
			multv3(theirspot.aim, sp->sl.s->aim, pm);
			if (!commonspot(&ourspot, &theirspot, nsloc))
				return(NULL);		/* no overlap */
			ourspot.flen = theirspot.flen;
		}
		if (sp->sflags & SFLAT) {	/* check for behind source */
			multv3(nsnorm, sp->snorm, pm);
			if (checkspot(&ourspot, nsnorm) < 0)
				return(NULL);
		}
	}
					/* everything is OK, make source */
	if ((newsrc = newsource()) == NULL)
		goto memerr;
	newsrc->sflags = sp->sflags | (SVIRTUAL|SSPOT|SFOLLOW);
	VCOPY(newsrc->sloc, nsloc);
	if (newsrc->sflags & SFLAT)
		VCOPY(newsrc->snorm, nsnorm);
	newsrc->ss = sp->ss; newsrc->ss2 = sp->ss2;
	if ((newsrc->sl.s = (SPOT *)malloc(sizeof(SPOT))) == NULL)
		goto memerr;
	copystruct(newsrc->sl.s, &ourspot);
	if (newsrc->sflags & SPROX)
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
	double  rad2, d1r2, d2r2;

	d1r2 = 1. - sp1->siz/(2.*PI);
	d2r2 = 1. - sp2->siz/(2.*PI);
	if (sp2->siz >= 2.*PI-FTINY)		/* BIG, just check overlap */
		return(DOT(sp1->aim,sp2->aim) >= d1r2*d2r2 -
					sqrt((1.-d1r2*d1r2)*(1.-d2r2*d2r2)));
				/* compute and check disks */
	d1r2 = 1./(d1r2*d1r2) - 1.;
	d2r2 = 1./(d2r2*d2r2) - 1.;
	rad2 = intercircle(cent, sp1->aim, sp2->aim, d1r2, d2r2);
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
		c1[i] = sp2->aim[i] - d*dir[i];
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


mirrorproj(m, nv, offs)		/* get mirror projection for surface */
register MAT4  m;
FVECT  nv;
double  offs;
{
	register int  i, j;
					/* assign matrix */
	setident4(m);
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			m[i][j] -= 2.*nv[i]*nv[j];
	for (j = 0; j < 3; j++)
		m[3][j] = 2.*offs*nv[j];
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
	l = sqrt((r1s - a2)/d2);
	for (i = 0; i < 3; i++)
		cc[i] = c1[i] + l*disp[i];
	return(a2);
}


/*
 * The following routines depend on the supported OBJECTS:
 */


double
getmaxdisk(ocent, op)		/* get object center and squared radius */
FVECT  ocent;
register OBJREC  *op;
{
	double  maxrad2;

	switch (op->otype) {
	case OBJ_FACE:
		{
			double  d1, d2;
			register int  i, j;
			register FACE  *f = getface(op);

			for (i = 0; i < 3; i++) {
				ocent[i] = 0.;
				for (j = 0; j < f->nv; j++)
					ocent[i] += VERTEX(f,j)[i];
				ocent[i] /= (double)f->nv;
			}
			maxrad2 = 0.;
			for (j = 0; j < f->nv; j++) {
				d2 = 0.;
				for (i = 0; i < 3; i++) {
					d1 = VERTEX(f,j)[i] - ocent[i];
					d2 += d1*d1;
				}
				if (d2 > maxrad2)
					maxrad2 = d2;
			}
		}
		return(maxrad2);
	case OBJ_RING:
		{
			register CONE  *co = getcone(op, 0);

			VCOPY(ocent, CO_P0(co));
			maxrad2 = CO_R1(co);
			maxrad2 *= maxrad2;
		}
		return(maxrad2);
	}
	objerror(op, USER, "illegal material");
}


double
getplaneq(nvec, op)			/* get plane equation for object */
FVECT  nvec;
OBJREC  *op;
{
	register FACE  *fo;
	register CONE  *co;

	switch (op->otype) {
	case OBJ_FACE:
		fo = getface(op);
		VCOPY(nvec, fo->norm);
		return(fo->offset);
	case OBJ_RING:
		co = getcone(op, 0);
		VCOPY(nvec, co->ad);
		return(DOT(nvec, CO_P0(co)));
	}
	objerror(op, USER, "illegal material");
}


/*
 * The following routines depend on the supported MATERIALS:
 */


vproject(o, s, n)		/* create projected source(s) if they exist */
OBJREC  *o;
SRCREC  *s;
int  n;
{
	SRCREC  *ns;
	FVECT  norm;
	double  offset;
	MAT4  proj;
				/* get surface normal and offset */
	offset = getplaneq(norm, o);
	switch (objptr(o->omod)->otype) {
	case MAT_MIRROR:			/* mirror source */
		if (DOT(s->sloc, norm) <= (s->sflags & SDISTANT ?
					FTINY : offset+FTINY))
			return;			/* behind mirror */
		mirrorproj(proj, norm, offset);
		if ((ns = makevsrc(o, s, proj)) != NULL)
			addvirtuals(ns, n);
		break;
	}
}


vsrcrelay(rn, rv)		/* relay virtual source ray */
register RAY  *rn, *rv;
{
	int  snext;
	register int  i;
					/* source we're aiming for here */
	snext = source[rv->rsrc].sa.svnext;
					/* compute relayed ray direction */
	switch (objptr(rv->ro->omod)->otype) {
	case MAT_MIRROR:		/* mirror: singular reflection */
		rayorigin(rn, rv, REFLECTED, 1.);
					/* ignore textures */
		for (i = 0; i < 3; i++)
			rn->rdir[i] = rv->rdir[i] + 2.*rv->rod*rv->ron[i];
		break;
#ifdef DEBUG
	default:
		error(CONSISTENCY, "inappropriate material in vsrcrelay");
#endif
	}
	rn->rsrc = snext;
}


m_mirror(m, r)			/* shade mirrored ray */
register OBJREC  *m;
register RAY  *r;
{
	COLOR  mcolor;
	RAY  nr;
	register int  i;

	if (m->oargs.nfargs != 3 || m->oargs.nsargs > 1)
		objerror(m, USER, "bad number of arguments");
	if (r->rsrc >= 0) {			/* aiming for somebody */
		if (source[r->rsrc].so != r->ro)
			return;				/* but not us */
	} else if (m->oargs.nsargs > 0) {	/* else call substitute? */
		rayshade(r, modifier(m->oargs.sarg[0]));
		return;
	}
	if (r->rod < 0.)			/* back is black */
		return;
					/* get modifiers */
	raytexture(r, m->omod);
					/* assign material color */
	setcolor(mcolor, m->oargs.farg[0],
			m->oargs.farg[1],
			m->oargs.farg[2]);
	multcolor(mcolor, r->pcol);
					/* compute reflected ray */
	if (r->rsrc >= 0)			/* relayed light source */
		vsrcrelay(&nr, r);
	else {					/* ordinary reflection */
		FVECT  pnorm;
		double  pdot;

		if (rayorigin(&nr, r, REFLECTED, bright(mcolor)) < 0)
			return;
		pdot = raynormal(pnorm, r);	/* use textures */
		for (i = 0; i < 3; i++)
			nr.rdir[i] = r->rdir[i] + 2.*pdot*pnorm[i];
	}
	rayvalue(&nr);
	multcolor(nr.rcol, mcolor);
	addcolor(r->rcol, nr.rcol);
}
