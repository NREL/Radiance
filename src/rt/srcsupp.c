/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Support routines for source objects and materials
 */

#include  "ray.h"

#include  "otypes.h"

#include  "source.h"

#include  "cone.h"

#include  "face.h"


SRCREC  *source = NULL;			/* our list of sources */
int  nsources = 0;			/* the number of sources */

SRCFUNC  sfun[NUMOTYPE];		/* source dispatch table */


initstypes()			/* initialize source dispatch table */
{
	extern VSMATERIAL  mirror_vs;
	extern int  fsetsrc(), ssetsrc(), sphsetsrc(), rsetsrc();
	extern double  fgetplaneq(), rgetplaneq();
	extern double  fgetmaxdisk(), rgetmaxdisk();
	static SOBJECT  fsobj = {fsetsrc, fgetplaneq, fgetmaxdisk};
	static SOBJECT  ssobj = {ssetsrc};
	static SOBJECT  sphsobj = {sphsetsrc};
	static SOBJECT  rsobj = {rsetsrc, rgetplaneq, rgetmaxdisk};

	sfun[MAT_MIRROR].mf = &mirror_vs;
	sfun[OBJ_FACE].of = &fsobj;
	sfun[OBJ_SOURCE].of = &ssobj;
	sfun[OBJ_SPHERE].of = &sphsobj;
	sfun[OBJ_RING].of = &rsobj;
}


int
newsource()			/* allocate new source in our array */
{
	if (nsources == 0)
		source = (SRCREC *)malloc(sizeof(SRCREC));
	else
		source = (SRCREC *)realloc((char *)source,
				(unsigned)(nsources+1)*sizeof(SRCREC));
	if (source == NULL)
		return(-1);
	source[nsources].sflags = 0;
	source[nsources].nhits = 1;
	source[nsources].ntests = 2;	/* initial hit probability = 1/2 */
	return(nsources++);
}


fsetsrc(src, so)			/* set a face as a source */
register SRCREC  *src;
OBJREC  *so;
{
	register FACE  *f;
	register int  i, j;
	
	src->sa.success = 2*AIMREQT-1;		/* bitch on second failure */
	src->so = so;
						/* get the face */
	f = getface(so);
						/* find the center */
	for (j = 0; j < 3; j++) {
		src->sloc[j] = 0.0;
		for (i = 0; i < f->nv; i++)
			src->sloc[j] += VERTEX(f,i)[j];
		src->sloc[j] /= (double)f->nv;
	}
	if (!inface(src->sloc, f))
		objerror(so, USER, "cannot hit center");
	src->sflags |= SFLAT;
	VCOPY(src->snorm, f->norm);
	src->ss = sqrt(f->area / PI);
	src->ss2 = f->area;
}


ssetsrc(src, so)			/* set a source as a source */
register SRCREC  *src;
register OBJREC  *so;
{
	double  theta;
	
	src->sa.success = 2*AIMREQT-1;		/* bitch on second failure */
	src->so = so;
	if (so->oargs.nfargs != 4)
		objerror(so, USER, "bad arguments");
	src->sflags |= SDISTANT;
	VCOPY(src->sloc, so->oargs.farg);
	if (normalize(src->sloc) == 0.0)
		objerror(so, USER, "zero direction");
	theta = PI/180.0/2.0 * so->oargs.farg[3];
	if (theta <= FTINY)
		objerror(so, USER, "zero size");
	src->ss = theta >= PI/4.0 ? 1.0 : tan(theta);
	src->ss2 = 2.0*PI * (1.0 - cos(theta));
}


sphsetsrc(src, so)			/* set a sphere as a source */
register SRCREC  *src;
register OBJREC  *so;
{
	src->sa.success = 2*AIMREQT-1;		/* bitch on second failure */
	src->so = so;
	if (so->oargs.nfargs != 4)
		objerror(so, USER, "bad # arguments");
	if (so->oargs.farg[3] <= FTINY)
		objerror(so, USER, "illegal radius");
	VCOPY(src->sloc, so->oargs.farg);
	src->ss = so->oargs.farg[3];
	src->ss2 = PI * src->ss * src->ss;
}


rsetsrc(src, so)			/* set a ring (disk) as a source */
register SRCREC  *src;
OBJREC  *so;
{
	register CONE  *co;
	
	src->sa.success = 2*AIMREQT-1;		/* bitch on second failure */
	src->so = so;
						/* get the ring */
	co = getcone(so, 0);
	VCOPY(src->sloc, CO_P0(co));
	if (CO_R0(co) > 0.0)
		objerror(so, USER, "cannot hit center");
	src->sflags |= SFLAT;
	VCOPY(src->snorm, co->ad);
	src->ss = CO_R1(co);
	src->ss2 = PI * src->ss * src->ss;
}


SPOT *
makespot(m)			/* make a spotlight */
register OBJREC  *m;
{
	register SPOT  *ns;

	if ((ns = (SPOT *)malloc(sizeof(SPOT))) == NULL)
		return(NULL);
	ns->siz = 2.0*PI * (1.0 - cos(PI/180.0/2.0 * m->oargs.farg[3]));
	VCOPY(ns->aim, m->oargs.farg+4);
	if ((ns->flen = normalize(ns->aim)) == 0.0)
		objerror(m, USER, "zero focus vector");
	return(ns);
}


double
fgetmaxdisk(ocent, op)		/* get center and squared radius of face */
FVECT  ocent;
OBJREC  *op;
{
	double  maxrad2;
	double  d;
	register int  i, j;
	register FACE  *f;
	
	f = getface(op);
	if (f->area == 0.)
		return(0.);
	for (i = 0; i < 3; i++) {
		ocent[i] = 0.;
		for (j = 0; j < f->nv; j++)
			ocent[i] += VERTEX(f,j)[i];
		ocent[i] /= (double)f->nv;
	}
	d = DOT(ocent,f->norm);
	for (i = 0; i < 3; i++)
		ocent[i] += (f->offset - d)*f->norm[i];
	maxrad2 = 0.;
	for (j = 0; j < f->nv; j++) {
		d = dist2(VERTEX(f,j), ocent);
		if (d > maxrad2)
			maxrad2 = d;
	}
	return(maxrad2);
}


double
rgetmaxdisk(ocent, op)		/* get center and squared radius of ring */
FVECT  ocent;
OBJREC  *op;
{
	register CONE  *co;
	
	co = getcone(op, 0);
	VCOPY(ocent, CO_P0(co));
	return(CO_R1(co)*CO_R1(co));
}


double
fgetplaneq(nvec, op)			/* get plane equation for face */
FVECT  nvec;
OBJREC  *op;
{
	register FACE  *fo;

	fo = getface(op);
	VCOPY(nvec, fo->norm);
	return(fo->offset);
}


double
rgetplaneq(nvec, op)			/* get plane equation for ring */
FVECT  nvec;
OBJREC  *op;
{
	register CONE  *co;

	co = getcone(op, 0);
	VCOPY(nvec, co->ad);
	return(DOT(nvec, CO_P0(co)));
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
register SPOT  *sp;	/* spotlight */
FVECT  nrm;		/* source surface normal */
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
spotdisk(oc, op, sp, pos)	/* intersect spot with object op */
FVECT  oc;
OBJREC  *op;
register SPOT  *sp;
FVECT  pos;
{
	FVECT  onorm;
	double  offs, d, dist;
	register int  i;

	offs = getplaneq(onorm, op);
	d = -DOT(onorm, sp->aim);
	if (d >= -FTINY && d <= FTINY)
		return(0.);
	dist = (DOT(pos, onorm) - offs)/d;
	if (dist < 0.)
		return(0.);
	for (i = 0; i < 3; i++)
		oc[i] = pos[i] + dist*sp->aim[i];
	return(sp->siz*dist*dist/PI/(d*d));
}


double
beamdisk(oc, op, sp, dir)	/* intersect beam with object op */
FVECT  oc;
OBJREC  *op;
register SPOT  *sp;
FVECT  dir;
{
	FVECT  onorm;
	double  offs, d, dist;
	register int  i;

	offs = getplaneq(onorm, op);
	d = -DOT(onorm, dir);
	if (d >= -FTINY && d <= FTINY)
		return(0.);
	dist = (DOT(sp->aim, onorm) - offs)/d;
	for (i = 0; i < 3; i++)
		oc[i] = sp->aim[i] + dist*dir[i];
	return(sp->siz/PI/(d*d));
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
		if (source[i].sflags & SDISTANT)
			/*
			 * Check to see if ray is within
			 * solid angle of source.
			 */
			if (2.0*PI * (1.0 - DOT(source[i].sloc,r->rdir))
					<= source[i].ss2) {
				r->ro = source[i].so;
				if (!(source[i].sflags & SSKIP))
					break;
			}

	if (r->ro != NULL) {
		for (i = 0; i < 3; i++)
			r->ron[i] = -r->rdir[i];
		r->rod = 1.0;
		r->rox = NULL;
		return(1);
	}
	return(0);
}


#define  wrongsource(m, r)	(m->otype!=MAT_ILLUM && \
				r->rsrc>=0 && \
				source[r->rsrc].so!=r->ro)

#define  badambient(m, r)	((r->crtype&(AMBIENT|SHADOW))==AMBIENT && \
				!(m->otype==MAT_GLOW&&r->rot>m->oargs.farg[3]))

#define  passillum(m, r)	(m->otype==MAT_ILLUM && \
				!(r->rsrc>=0&&source[r->rsrc].so==r->ro))


m_light(m, r)			/* ray hit a light source */
register OBJREC  *m;
register RAY  *r;
{
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
						/* check for behind */
		if (r->rod < 0.0)
			return;
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
