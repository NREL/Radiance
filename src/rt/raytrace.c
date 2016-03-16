#ifndef lint
static const char RCSid[] = "$Id$";
#endif
/*
 *  raytrace.c - routines for tracing and shading rays.
 *
 *  External symbols declared in ray.h
 */

#include "copyright.h"

#include  "ray.h"
#include  "source.h"
#include  "otypes.h"
#include  "otspecial.h"
#include  "random.h"
#include  "pmap.h"

#define  MAXCSET	((MAXSET+1)*2-1)	/* maximum check set size */

RNUMBER  raynum = 0;		/* next unique ray number */
RNUMBER  nrays = 0;		/* number of calls to localhit */

static RREAL  Lambfa[5] = {PI, PI, PI, 0.0, 0.0};
OBJREC  Lamb = {
	OVOID, MAT_PLASTIC, "Lambertian",
	{NULL, Lambfa, 0, 5}, NULL
};					/* a Lambertian surface */

OBJREC  Aftplane;			/* aft clipping plane object */

#define  RAYHIT		(-1)		/* return value for intercepted ray */

static int raymove(FVECT  pos, OBJECT  *cxs, int  dirf, RAY  *r, CUBE  *cu);
static int checkhit(RAY  *r, CUBE  *cu, OBJECT  *cxs);
static void checkset(OBJECT  *os, OBJECT  *cs);


int
rayorigin(		/* start new ray from old one */
	RAY  *r,
	int  rt,
	const RAY  *ro,
	const COLOR rc
)
{
	double	rw, re;
						/* assign coefficient/weight */
	if (rc == NULL) {
		rw = 1.0;
		setcolor(r->rcoef, 1., 1., 1.);
	} else {
		rw = intens(rc);
		if (rc != r->rcoef)
			copycolor(r->rcoef, rc);
	}
	if ((r->parent = ro) == NULL) {		/* primary ray */
		r->rlvl = 0;
		r->rweight = rw;
		r->crtype = r->rtype = rt;
		r->rsrc = -1;
		r->clipset = NULL;
		r->revf = raytrace;
		copycolor(r->cext, cextinction);
		copycolor(r->albedo, salbedo);
		r->gecc = seccg;
		r->slights = NULL;
	} else {				/* spawned ray */
		if (ro->rot >= FHUGE) {
			memset(r, 0, sizeof(RAY));
			return(-1);		/* illegal continuation */
		}
		r->rlvl = ro->rlvl;
		if (rt & RAYREFL) {
			r->rlvl++;
			r->rsrc = -1;
			r->clipset = ro->clipset;
			r->rmax = 0.0;
		} else {
			r->rsrc = ro->rsrc;
			r->clipset = ro->newcset;
			r->rmax = ro->rmax <= FTINY ? 0.0 : ro->rmax - ro->rot;
		}
		r->revf = ro->revf;
		copycolor(r->cext, ro->cext);
		copycolor(r->albedo, ro->albedo);
		r->gecc = ro->gecc;
		r->slights = ro->slights;
		r->crtype = ro->crtype | (r->rtype = rt);
		VCOPY(r->rorg, ro->rop);
		r->rweight = ro->rweight * rw;
						/* estimate extinction */
		re = colval(ro->cext,RED) < colval(ro->cext,GRN) ?
				colval(ro->cext,RED) : colval(ro->cext,GRN);
		if (colval(ro->cext,BLU) < re) re = colval(ro->cext,BLU);
		re *= ro->rot;
		if (re > 0.1) {
			if (re > 92.) {
				r->rweight = 0.0;
			} else {
				r->rweight *= exp(-re);
			}
		}
	}
	rayclear(r);
	if (r->rweight <= 0.0)			/* check for expiration */
		return(-1);
	if (r->crtype & SHADOW)			/* shadow commitment */
		return(0);
						/* ambient in photon map? */
	if (ro != NULL && ro->crtype & AMBIENT) {
		if (causticPhotonMapping)
			return(-1);
		if (photonMapping && rt != TRANS)
			return(-1);
	}
	if (maxdepth <= 0 && rc != NULL) {	/* Russian roulette */
		if (minweight <= 0.0)
			error(USER, "zero ray weight in Russian roulette");
		if (maxdepth < 0 && r->rlvl > -maxdepth)
			return(-1);		/* upper reflection limit */
		if (r->rweight >= minweight)
			return(0);
		if (frandom() > r->rweight/minweight)
			return(-1);
		rw = minweight/r->rweight;	/* promote survivor */
		scalecolor(r->rcoef, rw);
		r->rweight = minweight;
		return(0);
	}
	return(r->rweight >= minweight && r->rlvl <= abs(maxdepth) ? 0 : -1);
}


void
rayclear(			/* clear a ray for (re)evaluation */
	RAY  *r
)
{
	r->rno = raynum++;
	r->newcset = r->clipset;
	r->hitf = rayhit;
	r->robj = OVOID;
	r->ro = NULL;
	r->rox = NULL;
	r->rt = r->rot = FHUGE;
	r->pert[0] = r->pert[1] = r->pert[2] = 0.0;
	r->uv[0] = r->uv[1] = 0.0;
	setcolor(r->pcol, 1.0, 1.0, 1.0);
	setcolor(r->rcol, 0.0, 0.0, 0.0);
#ifdef DAYSIM
	daysimSet(r->daylightCoef, 0.0);
#endif
}


void
raytrace(			/* trace a ray and compute its value */
	RAY  *r
)
{
	if (localhit(r, &thescene))
		raycont(r);		/* hit local surface, evaluate */
	else if (r->ro == &Aftplane) {
		r->ro = NULL;		/* hit aft clipping plane */
		r->rot = FHUGE;
	} else if (sourcehit(r))
		rayshade(r, r->ro->omod);	/* distant source */

	if (trace != NULL)
		(*trace)(r);		/* trace execution */

	rayparticipate(r);		/* for participating medium */
}


void
raycont(			/* check for clipped object and continue */
	RAY  *r
)
{
	if ((r->clipset != NULL && inset(r->clipset, r->ro->omod)) ||
			!rayshade(r, r->ro->omod))
		raytrans(r);
}


void
raytrans(			/* transmit ray as is */
	RAY  *r
)
{
	RAY  tr;

	rayorigin(&tr, TRANS, r, NULL);		/* always continue */
	VCOPY(tr.rdir, r->rdir);
	rayvalue(&tr);
	copycolor(r->rcol, tr.rcol);
	r->rt = r->rot + tr.rt;
#ifdef DAYSIM
	daysimCopy(r->daylightCoef, tr.daylightCoef);
#endif
}


int
rayshade(		/* shade ray r with material mod */
	RAY  *r,
	int  mod
)
{
	OBJREC  *m;

	r->rt = r->rot;			/* set effective ray length */
	for ( ; mod != OVOID; mod = m->omod) {
		m = objptr(mod);
		/****** unnecessary test since modifier() is always called
		if (!ismodifier(m->otype)) {
			sprintf(errmsg, "illegal modifier \"%s\"", m->oname);
			error(USER, errmsg);
		}
		******/
					/* hack for irradiance calculation */
		if (do_irrad && !(r->crtype & ~(PRIMARY|TRANS)) &&
				m->otype != MAT_CLIP &&
				(ofun[m->otype].flags & (T_M|T_X))) {
			if (irr_ignore(m->otype)) {
				raytrans(r);
				return(1);
			}
			if (!islight(m->otype))
				m = &Lamb;
		}
		if ((*ofun[m->otype].funp)(m, r))
			return(1);	/* materials call raytexture() */
	}
	return(0);			/* no material! */
}


void
rayparticipate(			/* compute ray medium participation */
	RAY  *r
)
{
	COLOR	ce, ca;
	double	re, ge, be;

	if (intens(r->cext) <= 1./FHUGE)
		return;				/* no medium */
	re = r->rot*colval(r->cext,RED);
	ge = r->rot*colval(r->cext,GRN);
	be = r->rot*colval(r->cext,BLU);
	if (r->crtype & SHADOW) {		/* no scattering for sources */
		re *= 1. - colval(r->albedo,RED);
		ge *= 1. - colval(r->albedo,GRN);
		be *= 1. - colval(r->albedo,BLU);
	}
	setcolor(ce,	re<=FTINY ? 1. : re>92. ? 0. : exp(-re),
			ge<=FTINY ? 1. : ge>92. ? 0. : exp(-ge),
			be<=FTINY ? 1. : be>92. ? 0. : exp(-be));
	multcolor(r->rcol, ce);			/* path extinction */
#ifdef DAYSIM
	daysimScale(r->daylightCoef, colval(ce, RED));
#endif
	if (r->crtype & SHADOW || intens(r->albedo) <= FTINY)
		return;				/* no scattering */
	
	/* PMAP: indirect inscattering accounted for by volume photons? */
	if (!volumePhotonMapping) {
		setcolor(ca,
			colval(r->albedo,RED)*colval(ambval,RED)*(1.-colval(ce,RED)),
			colval(r->albedo,GRN)*colval(ambval,GRN)*(1.-colval(ce,GRN)),
			colval(r->albedo,BLU)*colval(ambval,BLU)*(1.-colval(ce,BLU)));
		addcolor(r->rcol, ca);			/* ambient in scattering */
	}
	
	srcscatter(r);				/* source in scattering */
}


void
raytexture(			/* get material modifiers */
	RAY  *r,
	OBJECT  mod
)
{
	OBJREC  *m;
					/* execute textures and patterns */
	for ( ; mod != OVOID; mod = m->omod) {
		m = objptr(mod);
		/****** unnecessary test since modifier() is always called
		if (!ismodifier(m->otype)) {
			sprintf(errmsg, "illegal modifier \"%s\"", m->oname);
			error(USER, errmsg);
		}
		******/
		if ((*ofun[m->otype].funp)(m, r)) {
			sprintf(errmsg, "conflicting material \"%s\"",
					m->oname);
			objerror(r->ro, USER, errmsg);
		}
	}
}


int
raymixture(		/* mix modifiers */
	RAY  *r,
	OBJECT  fore,
	OBJECT  back,
	double  coef
)
{
	RAY  fr, br;
	int  foremat, backmat;
	int  i;
					/* bound coefficient */
	if (coef > 1.0)
		coef = 1.0;
	else if (coef < 0.0)
		coef = 0.0;
					/* compute foreground and background */
	foremat = backmat = 0;
					/* foreground */
	fr = *r;
	if (coef > FTINY) {
		fr.rweight *= coef;
		scalecolor(fr.rcoef, coef);
		foremat = rayshade(&fr, fore);
	}
					/* background */
	br = *r;
	if (coef < 1.0-FTINY) {
		br.rweight *= 1.0-coef;
		scalecolor(br.rcoef, 1.0-coef);
		backmat = rayshade(&br, back);
	}
					/* check for transparency */
	if (backmat ^ foremat) {
		if (backmat && coef > FTINY)
			raytrans(&fr);
		else if (foremat && coef < 1.0-FTINY)
			raytrans(&br);
	}
					/* mix perturbations */
	for (i = 0; i < 3; i++)
		r->pert[i] = coef*fr.pert[i] + (1.0-coef)*br.pert[i];
					/* mix pattern colors */
	scalecolor(fr.pcol, coef);
	scalecolor(br.pcol, 1.0-coef);
	copycolor(r->pcol, fr.pcol);
	addcolor(r->pcol, br.pcol);
					/* return value tells if material */
	if (!foremat & !backmat)
		return(0);
					/* mix returned ray values */
	scalecolor(fr.rcol, coef);
	scalecolor(br.rcol, 1.0-coef);
	copycolor(r->rcol, fr.rcol);
	addcolor(r->rcol, br.rcol);
#ifdef DAYSIM
	daysimAssignScaled(r->daylightCoef, fr.daylightCoef, coef);
	daysimAddScaled(r->daylightCoef, br.daylightCoef, 1.0 - coef);
#endif
	r->rt = bright(fr.rcol) > bright(br.rcol) ? fr.rt : br.rt;
	return(1);
}


double
raydist(		/* compute (cumulative) ray distance */
	const RAY  *r,
	int  flags
)
{
	double  sum = 0.0;

	while (r != NULL && r->crtype&flags) {
		sum += r->rot;
		r = r->parent;
	}
	return(sum);
}


void
raycontrib(		/* compute (cumulative) ray contribution */
	RREAL  rc[3],
	const RAY  *r,
	int  flags
)
{
	double	eext[3];
	int	i;

	eext[0] = eext[1] = eext[2] = 0.;
	rc[0] = rc[1] = rc[2] = 1.;

	while (r != NULL && r->crtype&flags) {
		for (i = 3; i--; ) {
			rc[i] *= colval(r->rcoef,i);
			eext[i] += r->rot * colval(r->cext,i);
		}
		r = r->parent;
	}
	for (i = 3; i--; )
		rc[i] *= (eext[i] <= FTINY) ? 1. :
				(eext[i] > 92.) ? 0. : exp(-eext[i]);
}


double
raynormal(		/* compute perturbed normal for ray */
	FVECT  norm,
	RAY  *r
)
{
	double  newdot;
	int  i;

	/*	The perturbation is added to the surface normal to obtain
	 *  the new normal.  If the new normal would affect the surface
	 *  orientation wrt. the ray, a correction is made.  The method is
	 *  still fraught with problems since reflected rays and similar
	 *  directions calculated from the surface normal may spawn rays behind
	 *  the surface.  The only solution is to curb textures at high
	 *  incidence (namely, keep DOT(rdir,pert) < Rdot).
	 */

	for (i = 0; i < 3; i++)
		norm[i] = r->ron[i] + r->pert[i];

	if (normalize(norm) == 0.0) {
		objerror(r->ro, WARNING, "illegal normal perturbation");
		VCOPY(norm, r->ron);
		return(r->rod);
	}
	newdot = -DOT(norm, r->rdir);
	if ((newdot > 0.0) != (r->rod > 0.0)) {		/* fix orientation */
		for (i = 0; i < 3; i++)
			norm[i] += 2.0*newdot*r->rdir[i];
		newdot = -newdot;
	}
	return(newdot);
}


void
newrayxf(			/* get new tranformation matrix for ray */
	RAY  *r
)
{
	static struct xfn {
		struct xfn  *next;
		FULLXF  xf;
	}  xfseed = { &xfseed }, *xflast = &xfseed;
	struct xfn  *xp;
	const RAY  *rp;

	/*
	 * Search for transform in circular list that
	 * has no associated ray in the tree.
	 */
	xp = xflast;
	for (rp = r->parent; rp != NULL; rp = rp->parent)
		if (rp->rox == &xp->xf) {		/* xp in use */
			xp = xp->next;			/* move to next */
			if (xp == xflast) {		/* need new one */
				xp = (struct xfn *)bmalloc(sizeof(struct xfn));
				if (xp == NULL)
					error(SYSTEM,
						"out of memory in newrayxf");
							/* insert in list */
				xp->next = xflast->next;
				xflast->next = xp;
				break;			/* we're done */
			}
			rp = r;			/* start check over */
		}
					/* got it */
	r->rox = &xp->xf;
	xflast = xp;
}


void
flipsurface(			/* reverse surface orientation */
	RAY  *r
)
{
	r->rod = -r->rod;
	r->ron[0] = -r->ron[0];
	r->ron[1] = -r->ron[1];
	r->ron[2] = -r->ron[2];
	r->pert[0] = -r->pert[0];
	r->pert[1] = -r->pert[1];
	r->pert[2] = -r->pert[2];
}


void
rayhit(			/* standard ray hit test */
	OBJECT  *oset,
	RAY  *r
)
{
	OBJREC  *o;
	int	i;

	for (i = oset[0]; i > 0; i--) {
		o = objptr(oset[i]);
		if ((*ofun[o->otype].funp)(o, r))
			r->robj = oset[i];
	}
}


int
localhit(		/* check for hit in the octree */
	RAY  *r,
	CUBE  *scene
)
{
	OBJECT  cxset[MAXCSET+1];	/* set of checked objects */
	FVECT  curpos;			/* current cube position */
	int  sflags;			/* sign flags */
	double  t, dt;
	int  i;

	nrays++;			/* increment trace counter */
	sflags = 0;
	for (i = 0; i < 3; i++) {
		curpos[i] = r->rorg[i];
		if (r->rdir[i] > 1e-7)
			sflags |= 1 << i;
		else if (r->rdir[i] < -1e-7)
			sflags |= 0x10 << i;
	}
	if (!sflags) {
		error(WARNING, "zero ray direction in localhit");
		return(0);
	}
					/* start off assuming nothing hit */
	if (r->rmax > FTINY) {		/* except aft plane if one */
		r->ro = &Aftplane;
		r->rot = r->rmax;
		VSUM(r->rop, r->rorg, r->rdir, r->rot);
	}
					/* find global cube entrance point */
	t = 0.0;
	if (!incube(scene, curpos)) {
					/* find distance to entry */
		for (i = 0; i < 3; i++) {
					/* plane in our direction */
			if (sflags & 1<<i)
				dt = scene->cuorg[i];
			else if (sflags & 0x10<<i)
				dt = scene->cuorg[i] + scene->cusize;
			else
				continue;
					/* distance to the plane */
			dt = (dt - r->rorg[i])/r->rdir[i];
			if (dt > t)
				t = dt;	/* farthest face is the one */
		}
		t += FTINY;		/* fudge to get inside cube */
		if (t >= r->rot)	/* clipped already */
			return(0);
					/* advance position */
		VSUM(curpos, curpos, r->rdir, t);

		if (!incube(scene, curpos))	/* non-intersecting ray */
			return(0);
	}
	cxset[0] = 0;
	raymove(curpos, cxset, sflags, r, scene);
	return((r->ro != NULL) & (r->ro != &Aftplane));
}


static int
raymove(		/* check for hit as we move */
	FVECT  pos,			/* current position, modified herein */
	OBJECT  *cxs,			/* checked objects, modified by checkhit */
	int  dirf,			/* direction indicators to speed tests */
	RAY  *r,
	CUBE  *cu
)
{
	int  ax;
	double  dt, t;

	if (istree(cu->cutree)) {		/* recurse on subcubes */
		CUBE  cukid;
		int  br, sgn;

		cukid.cusize = cu->cusize * 0.5;	/* find subcube */
		VCOPY(cukid.cuorg, cu->cuorg);
		br = 0;
		if (pos[0] >= cukid.cuorg[0]+cukid.cusize) {
			cukid.cuorg[0] += cukid.cusize;
			br |= 1;
		}
		if (pos[1] >= cukid.cuorg[1]+cukid.cusize) {
			cukid.cuorg[1] += cukid.cusize;
			br |= 2;
		}
		if (pos[2] >= cukid.cuorg[2]+cukid.cusize) {
			cukid.cuorg[2] += cukid.cusize;
			br |= 4;
		}
		for ( ; ; ) {
			cukid.cutree = octkid(cu->cutree, br);
			if ((ax = raymove(pos,cxs,dirf,r,&cukid)) == RAYHIT)
				return(RAYHIT);
			sgn = 1 << ax;
			if (sgn & dirf)			/* positive axis? */
				if (sgn & br)
					return(ax);	/* overflow */
				else {
					cukid.cuorg[ax] += cukid.cusize;
					br |= sgn;
				}
			else
				if (sgn & br) {
					cukid.cuorg[ax] -= cukid.cusize;
					br &= ~sgn;
				} else
					return(ax);	/* underflow */
		}
		/*NOTREACHED*/
	}
	if (isfull(cu->cutree)) {
		if (checkhit(r, cu, cxs))
			return(RAYHIT);
	} else if (r->ro == &Aftplane && incube(cu, r->rop))
		return(RAYHIT);
					/* advance to next cube */
	if (dirf&0x11) {
		dt = dirf&1 ? cu->cuorg[0] + cu->cusize : cu->cuorg[0];
		t = (dt - pos[0])/r->rdir[0];
		ax = 0;
	} else
		t = FHUGE;
	if (dirf&0x22) {
		dt = dirf&2 ? cu->cuorg[1] + cu->cusize : cu->cuorg[1];
		dt = (dt - pos[1])/r->rdir[1];
		if (dt < t) {
			t = dt;
			ax = 1;
		}
	}
	if (dirf&0x44) {
		dt = dirf&4 ? cu->cuorg[2] + cu->cusize : cu->cuorg[2];
		dt = (dt - pos[2])/r->rdir[2];
		if (dt < t) {
			t = dt;
			ax = 2;
		}
	}
	VSUM(pos, pos, r->rdir, t);
	return(ax);
}


static int
checkhit(		/* check for hit in full cube */
	RAY  *r,
	CUBE  *cu,
	OBJECT  *cxs
)
{
	OBJECT  oset[MAXSET+1];

	objset(oset, cu->cutree);
	checkset(oset, cxs);			/* avoid double-checking */

	(*r->hitf)(oset, r);			/* test for hit in set */

	if (r->robj == OVOID)
		return(0);			/* no scores yet */

	return(incube(cu, r->rop));		/* hit OK if in current cube */
}


static void
checkset(		/* modify checked set and set to check */
	OBJECT  *os,			/* os' = os - cs */
	OBJECT  *cs			/* cs' = cs + os */
)
{
	OBJECT  cset[MAXCSET+MAXSET+1];
	int  i, j;
	int  k;
					/* copy os in place, cset <- cs */
	cset[0] = 0;
	k = 0;
	for (i = j = 1; i <= os[0]; i++) {
		while (j <= cs[0] && cs[j] < os[i])
			cset[++cset[0]] = cs[j++];
		if (j > cs[0] || os[i] != cs[j]) {	/* object to check */
			os[++k] = os[i];
			cset[++cset[0]] = os[i];
		}
	}
	if (!(os[0] = k))		/* new "to check" set size */
		return;			/* special case */
	while (j <= cs[0])		/* get the rest of cs */
		cset[++cset[0]] = cs[j++];
	if (cset[0] > MAXCSET)		/* truncate "checked" set if nec. */
		cset[0] = MAXCSET;
	/* setcopy(cs, cset); */	/* copy cset back to cs */
	os = cset;
	for (i = os[0]; i-- >= 0; )
		*cs++ = *os++;
}
