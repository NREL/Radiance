#ifndef lint
static const char	RCSid[] = "$Id: virtuals.c,v 2.25 2021/02/12 00:41:19 greg Exp $";
#endif
/*
 * Routines for simulating virtual light sources
 *	Thus far, we only support planar mirrors.
 *
 *  External symbols declared in source.h
 */

#include "copyright.h"

#include  "ray.h"
#include  "otypes.h"
#include  "otspecial.h"
#include  "source.h"
#include  "random.h"

#define  MINSAMPLES	16		/* minimum number of pretest samples */
#define  STESTMAX	32		/* maximum seeks per sample */


static OBJECT  *vobject;		/* virtual source objects */
static int  nvobjects = 0;		/* number of virtual source objects */


static int
isident4(MAT4 m)
{
	int	i, j;

	for (i = 4; i--; )
		for (j = 4; j--; )
			if (!FABSEQ(m[i][j], i==j))
				return(0);
	return(1);
}


void
markvirtuals(void)			/* find and mark virtual sources */
{
	OBJREC  *o;
	int  i;
					/* check number of direct relays */
	if (directrelay <= 0)
		return;
					/* find virtual source objects */
	for (i = 0; i < nsceneobjs; i++) {
		o = objptr(i);
		if (!issurface(o->otype) || o->omod == OVOID)
			continue;
		if (!isvlight(vsmaterial(o)->otype))
			continue;
		if (sfun[o->otype].of == NULL ||
				sfun[o->otype].of->getpleq == NULL) {
			objerror(o,WARNING,"secondary sources not supported");
			continue;
		}
		if (nvobjects == 0)
			vobject = (OBJECT *)malloc(sizeof(OBJECT));
		else
			vobject = (OBJECT *)realloc((void *)vobject,
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
		addvirtuals(i, directrelay);
					/* done with our object list */
	free((void *)vobject);
	nvobjects = 0;
}


void
addvirtuals(		/* add virtuals associated with source */
	int  sn,
	int  nr
)
{
	int  i;
				/* check relay limit first */
	if (nr <= 0)
		return;
	if (source[sn].sflags & SSKIP)
		return;
				/* check each virtual object for projection */
	for (i = 0; i < nvobjects; i++)
					/* vproject() calls us recursively */
		vproject(objptr(vobject[i]), sn, nr-1);
}


void
vproject(		/* create projected source(s) if they exist */
	OBJREC  *o,
	int  sn,
	int  n
)
{
	int  i;
	VSMATERIAL  *vsmat;
	MAT4  proj;
	int  ns;

	if (o == source[sn].so)	/* objects cannot project themselves */
		return;
				/* get virtual source material */
	vsmat = sfun[vsmaterial(o)->otype].mf;
				/* project virtual sources */
	for (i = 0; i < vsmat->nproj; i++)
		if ((*vsmat->vproj)(proj, o, &source[sn], i))
			if ((ns = makevsrc(o, sn, proj)) >= 0) {
				source[ns].sa.sv.pn = i;
#ifdef DEBUG
				virtverb(ns, stderr);
#endif
				addvirtuals(ns, n);
			}
}


OBJREC *
vsmaterial(			/* get virtual source material pointer */
	OBJREC  *o
)
{
	int  i;
	OBJREC  *m;

	i = o->omod;
	m = findmaterial(o);
	if (m == NULL)
		return(objptr(i));
	if (m->otype != MAT_ILLUM || m->oargs.nsargs < 1 ||
			!strcmp(m->oargs.sarg[0], VOIDID) ||
			(i = lastmod(objndx(m), m->oargs.sarg[0])) == OVOID)
		return(m);		/* direct modifier */
	return(objptr(i));		/* illum alternate */
}


int
makevsrc(		/* make virtual source if reasonable */
	OBJREC  *op,
	int  sn,
	MAT4  pm
)
{
	FVECT  nsloc, nsnorm, ocent, v;
	double  maxrad2, d;
	int  nsflags;
	SPOT  theirspot, ourspot;
	int  i;
					/* check for no-op */
	if (isident4(pm))
		return(0);
	nsflags = source[sn].sflags | (SVIRTUAL|SSPOT|SFOLLOW);
					/* get object center and max. radius */
	maxrad2 = getdisk(ocent, op, sn);
	if (maxrad2 <= FTINY)			/* too small? */
		return(-1);
					/* get location and spot */
	if (source[sn].sflags & SDISTANT) {		/* distant source */
		if (source[sn].sflags & SPROX)
			return(-1);		/* should never get here! */
		multv3(nsloc, source[sn].sloc, pm);
		normalize(nsloc);
		VCOPY(ourspot.aim, ocent);
		ourspot.siz = PI*maxrad2;
		ourspot.flen = -1.;
		if (source[sn].sflags & SSPOT) {
			multp3(theirspot.aim, source[sn].sl.s->aim, pm);
						/* adjust for source size */
			d = sqrt(dist2(ourspot.aim, theirspot.aim));
			d = sqrt(source[sn].sl.s->siz/PI) + d*source[sn].srad;
			theirspot.siz = PI*d*d;
			ourspot.flen = theirspot.flen = source[sn].sl.s->flen;
			d = ourspot.siz;
			if (!commonbeam(&ourspot, &theirspot, nsloc))
				return(-1);	/* no overlap */
			if (ourspot.siz < d-FTINY) {	/* it shrunk */
				d = beamdisk(v, op, &ourspot, nsloc);
				if (d <= FTINY)
					return(-1);
				if (d < maxrad2) {
					maxrad2 = d;
					VCOPY(ocent, v);
				}
			}
		}
	} else {				/* local source */
		multp3(nsloc, source[sn].sloc, pm);
		for (i = 0; i < 3; i++)
			ourspot.aim[i] = ocent[i] - nsloc[i];
		if ((d = normalize(ourspot.aim)) == 0.)
			return(-1);		/* at source!! */
		if (source[sn].sflags & SPROX && d > source[sn].sl.prox)
			return(-1);		/* too far away */
		ourspot.flen = 0.;
						/* adjust for source size */
		d = (sqrt(maxrad2) + source[sn].srad) / d;
		if (d < 1.-FTINY)
			ourspot.siz = 2.*PI*(1. - sqrt(1.-d*d));
		else
			nsflags &= ~SSPOT;
		if (source[sn].sflags & SSPOT) {
			theirspot = *(source[sn].sl.s);
			multv3(theirspot.aim, source[sn].sl.s->aim, pm);
			normalize(theirspot.aim);
			if (nsflags & SSPOT) {
				ourspot.flen = theirspot.flen;
				d = ourspot.siz;
				if (!commonspot(&ourspot, &theirspot, nsloc))
					return(-1);	/* no overlap */
			} else {
				nsflags |= SSPOT;
				ourspot = theirspot;
				d = 2.*ourspot.siz;
			}
			if (ourspot.siz < d-FTINY) {	/* it shrunk */
				d = spotdisk(v, op, &ourspot, nsloc);
				if (d <= FTINY)
					return(-1);
				if (d < maxrad2) {
					maxrad2 = d;
					VCOPY(ocent, v);
				}
			}
		}
		if (source[sn].sflags & SFLAT) {	/* behind source? */
			multv3(nsnorm, source[sn].snorm, pm);
			normalize(nsnorm);
			if (nsflags & SSPOT && !checkspot(&ourspot, nsnorm))
				return(-1);
		}
	}
					/* pretest visibility */
	nsflags = vstestvis(nsflags, op, ocent, maxrad2, sn);
	if (nsflags & SSKIP)
		return(-1);	/* obstructed */
					/* it all checks out, so make it */
	if ((i = newsource()) < 0)
		goto memerr;
	source[i].sflags = nsflags;
	VCOPY(source[i].sloc, nsloc);
	multv3(source[i].ss[SU], source[sn].ss[SU], pm);
	multv3(source[i].ss[SV], source[sn].ss[SV], pm);
	if (nsflags & SFLAT)
		VCOPY(source[i].snorm, nsnorm);
	else
		multv3(source[i].ss[SW], source[sn].ss[SW], pm);
	source[i].srad = source[sn].srad;
	source[i].ss2 = source[sn].ss2;
	if (nsflags & SSPOT) {
		if ((source[i].sl.s = (SPOT *)malloc(sizeof(SPOT))) == NULL)
			goto memerr;
		*(source[i].sl.s) = ourspot;
	}
	if (nsflags & SPROX)
		source[i].sl.prox = source[sn].sl.prox;
	source[i].sa.sv.sn = sn;
	source[i].so = op;
	return(i);
memerr:
	error(SYSTEM, "out of memory in makevsrc");
	return -1; /* pro forma return */
}


double
getdisk(		/* get visible object disk */
	FVECT  oc,
	OBJREC  *op,
	int  sn
)
{
	double  rad2, roffs, offs, d, rd, rdoto;
	FVECT  rnrm, nrm;
				/* first, use object getdisk function */
	rad2 = getmaxdisk(oc, op);
	if (!(source[sn].sflags & SVIRTUAL))
		return(rad2);		/* all done for normal source */
				/* check for correct side of relay surface */
	roffs = getplaneq(rnrm, source[sn].so);
	rd = DOT(rnrm, source[sn].sloc);	/* source projection */
	if (!(source[sn].sflags & SDISTANT))
		rd -= roffs;
	d = DOT(rnrm, oc) - roffs;	/* disk distance to relay plane */
	if ((d > 0.) ^ (rd > 0.))
		return(rad2);		/* OK if opposite sides */
	if (d*d >= rad2)
		return(0.);		/* no relay is possible */
				/* we need a closer look */
	offs = getplaneq(nrm, op);
	rdoto = DOT(rnrm, nrm);
	if (d*d >= rad2*(1.-rdoto*rdoto))
		return(0.);		/* disk entirely on projection side */
				/* should shrink disk but I'm lazy */
	return(rad2);
}


int
vstestvis(		/* pretest source visibility */
	int  f,			/* virtual source flags */
	OBJREC  *o,		/* relay object */
	FVECT  oc,		/* relay object center */
	double  or2,		/* relay object radius squared */
	int  sn	/* target source number */
)
{
	RAY  sr;
	FVECT  onorm;
	double  offsdir[3];
	SRCINDEX  si;
	double  or, d, d1;
	int  stestlim, ssn;
	int  nhit, nok;
	int  i, n;
				/* return if pretesting disabled */
	if (vspretest <= 0)
		return(f);
				/* get surface normal */
	getplaneq(onorm, o);
				/* set number of rays to sample */
	if (source[sn].sflags & SDISTANT) {
					/* 32. == heuristic constant */
		n = 32.*or2/(thescene.cusize*thescene.cusize)*vspretest + .5;
	} else {
		VSUB(offsdir, source[sn].sloc, oc);
		d = DOT(offsdir,offsdir);
		if (d <= FTINY)
			n = 2.*PI * vspretest + .5;
		else
			n = 2.*PI * (1.-sqrt(1./(1.+or2/d)))*vspretest + .5;
	}
	if (n < MINSAMPLES) n = MINSAMPLES;
#ifdef DEBUG
	fprintf(stderr, "pretesting source %d in object %s with %d rays\n",
			sn, o->oname, n);
#endif
				/* sample */
	or = sqrt(or2);
	stestlim = n*STESTMAX;
	ssn = 0;
	nhit = nok = 0;
	initsrcindex(&si);
	while (n-- > 0) {
					/* get sample point */
		do {
			if (ssn >= stestlim) {
#ifdef DEBUG
				fprintf(stderr, "\ttoo hard to hit\n");
#endif
				return(f);	/* too small a target! */
			}
			multisamp(offsdir, 3, urand(sn*931+5827+ssn));
			for (i = 0; i < 3; i++)
				offsdir[i] = or*(1. - 2.*offsdir[i]);
			ssn++;
			d = 1. - DOT(offsdir, onorm);
			for (i = 0; i < 3; i++) {
				sr.rorg[i] = oc[i] + offsdir[i] + d*onorm[i];
				sr.rdir[i] = -onorm[i];
			}
			sr.rmax = 0.0;
			rayorigin(&sr, PRIMARY, NULL, NULL);
		} while (!(*ofun[o->otype].funp)(o, &sr));
					/* check against source */
		VCOPY(sr.rorg, sr.rop);	/* starting from intersection */
		samplendx++;
		if (si.sp >= si.np-1 ||
				!srcray(&sr, NULL, &si) || sr.rsrc != sn) {
			si.sn = sn-1;		/* reset index to our source */
			si.np = 0;
			if (!srcray(&sr, NULL, &si) || sr.rsrc != sn)
				continue;	/* can't get there from here */
		}
		sr.revf = srcvalue;
		rayvalue(&sr);			/* check sample validity */
		if ((d = bright(sr.rcol)) <= FTINY)
			continue;
		nok++;			/* got sample; check obstructions */
		rayclear(&sr);
		sr.revf = raytrace;
		rayvalue(&sr);
		if ((d1 = bright(sr.rcol)) > FTINY) {
			if (d - d1 > FTINY) {
#ifdef DEBUG
				fprintf(stderr, "\tpartially shadowed\n");
#endif
				return(f);	/* intervening transmitter */
			}
			nhit++;
		}
		if (nhit > 0 && nhit < nok) {
#ifdef DEBUG
			fprintf(stderr, "\tpartially occluded\n");
#endif
			return(f);		/* need to shadow test */
		}
	}
	if (nhit == 0) {
#ifdef DEBUG
		fprintf(stderr, "\t0%% hit rate\n");
#endif
		return(f | SSKIP);	/* 0% hit rate:  totally occluded */
	}
#ifdef DEBUG
	fprintf(stderr, "\t100%% hit rate\n");
#endif
	return(f & ~SFOLLOW);		/* 100% hit rate:  no occlusion */
}
	

#ifdef DEBUG
void
virtverb(	/* print verbose description of virtual source */
	int  sn,
	FILE  *fp
)
{
	fprintf(fp, "%s virtual source %d in %s %s\n",
			source[sn].sflags & SDISTANT ? "distant" : "local",
			sn, ofun[source[sn].so->otype].funame,
			source[sn].so->oname);
	fprintf(fp, "\tat (%f,%f,%f)\n",
		source[sn].sloc[0], source[sn].sloc[1], source[sn].sloc[2]);
	fprintf(fp, "\tlinked to source %d (%s)\n",
		source[sn].sa.sv.sn, source[source[sn].sa.sv.sn].so->oname);
	if (source[sn].sflags & SFOLLOW)
		fprintf(fp, "\talways followed\n");
	else
		fprintf(fp, "\tnever followed\n");
	if (!(source[sn].sflags & SSPOT))
		return;
	fprintf(fp, "\twith spot aim (%f,%f,%f) and size %f\n",
			source[sn].sl.s->aim[0], source[sn].sl.s->aim[1],
			source[sn].sl.s->aim[2], source[sn].sl.s->siz);
}
#endif
