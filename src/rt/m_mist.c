/* Copyright (c) 1995 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Mist volumetric material.
 */

#include  "ray.h"

#include  "source.h"

/*
 *  A mist volume is used to specify a region in the scene where a certain
 *  light source (or sources) is going to contribute to scattering.  The
 *  material can add to the existing global medium, and override any ray
 *  settings for scattering albedo and eccentricity.  Overlapping mist
 *  regions should agree w.r.t. albedo and eccentricity, and
 *  should have disjoint source lists.
 *
 *  A pattern, if used, should compute the line integral of extinction,
 *  and will modify the first three arguments directly.  This will tend
 *  to invalidate results when there are other objects intersected within
 *  the mist region.
 *
 *  The string arguments for MAT_MIST are the identifiers for the important
 *  light sources, which will be looked up in the source array.  The last
 *  source found matching a name is the one used.  A relayed light source
 *  may be indicated by the relay surface name, followed by a '>' character,
 *  followed by the relayed source name (which may be another relay).
 *
 *  Up to five real arguments may be given for MAT_MIST:
 *
 *	[ext_r  ext_g  ext_b  [albedo [gecc]]]
 *
 *  The primaries indicate medium extinction per unit length (absorption
 *  plus scattering).  The albedo is the ratio of scattering to extinction,
 *  and is set globally by the -ma option (salbedo) and overridden here.
 *  The Heyney-Greenstein eccentricity parameter (-mg seccg) indicates how much
 *  scattering favors the forward direction.  A value of 0 means isotropic
 *  scattering.  A value approaching 1 indicates strong forward scattering.
 */

#define MAXSLIST	32		/* maximum sources to check */
#define RELAYDELIM	'>'		/* relay delimiter character */

extern COLOR  cextinction;		/* global coefficient of extinction */
extern double  salbedo;			/* global scattering albedo */
extern double  seccg;			/* global scattering eccentricity */


static int
inslist(sl, n)		/* return index of source n if it's in list sl */
register int  *sl;
register int  n;
{
	register int  i;

	for (i = sl[0]; i > 0; i--)
		if (sl[i] == n)
			return(i);
	return(0);
}


static int
srcmatch(sn, id)	/* check for an id match on a light source */
register int  sn;
char  *id;
{
	extern char  *index();
	register char  *cp;
again:
	if (source[sn].so == NULL)		/* just in case */
		return(0);
	if ((cp = index(id, RELAYDELIM)) != NULL) {	/* relay source */
		if (!(source[sn].sflags & SVIRTUAL))
			return(0);
		*cp = '\0';
		if (strcmp(id, source[sn].so->oname)) {
			*cp = RELAYDELIM;
			return(0);
		}
		*cp = RELAYDELIM;
		id = cp + 1;				/* recurse */
		sn = source[sn].sa.sv.sn;
		goto again;
	}
	if (source[sn].sflags & SVIRTUAL)
		return(0);
	return(!strcmp(id, source[sn].so->oname));
}


m_mist(m, r)		/* process a ray entering or leaving some mist */
OBJREC  *m;
register RAY  *r;
{
	RAY  p;
	int  *myslist = NULL;
	int  newslist[MAXSLIST+1];
	COLOR  mext;
	double  re, ge, be;
	register int  i, j;
					/* check arguments */
	if (m->oargs.nfargs > 5)
		objerror(m, USER, "bad arguments");
	if (m->oargs.nsargs > MAXSLIST)
		objerror(m, USER, "too many sources in list");
					/* get source indices */
	if (m->oargs.nsargs > 0 && (myslist = (int *)m->os) == NULL) {
		myslist = (int *)malloc((m->oargs.nsargs+1)*sizeof(int));
		if (myslist == NULL)
			goto memerr;
		myslist[0] = m->oargs.nsargs;	/* size is first in set */
		for (j = myslist[0]; j > 0; j--) {
			i = nsources;		/* look up each source id */
			while (i--)
				if (srcmatch(i, m->oargs.sarg[j-1]))
					break;
			if (i < 0) {
				sprintf(errmsg, "unknown source \"%s\"",
						m->oargs.sarg[j-1]);
				objerror(m, WARNING, errmsg);
			} else
				myslist[j] = i;
		}
		m->os = (char *)myslist;
	}
	if (m->oargs.nfargs > 2) {		/* compute extinction */
		setcolor(mext, m->oargs.farg[0], m->oargs.farg[1],
				m->oargs.farg[2]);
		raytexture(r, m->omod);			/* get modifiers */
		multcolor(mext, r->pcol);
	} else
		setcolor(mext, 0., 0., 0.);
						/* start transmitted ray */
	if (rayorigin(&p, r, TRANS, 1.) < 0)
		return(1);
	VCOPY(p.rdir, r->rdir);
	p.slights = newslist;
	if (r->slights != NULL)
		for (j = r->slights[0]; j >= 0; j--)
			p.slights[j] = r->slights[j];
	else
		p.slights[0] = 0;
	if (r->rod > 0.) {			/* entering ray */
		addcolor(p.cext, mext);
		if (m->oargs.nfargs > 3)
			p.albedo = m->oargs.farg[3];
		if (m->oargs.nfargs > 4)
			p.gecc = m->oargs.farg[4];
		if (myslist != NULL)			/* add to list */
			for (j = myslist[0]; j > 0; j--)
				if (!inslist(p.slights, myslist[j])) {
					if (p.slights[0] >= MAXSLIST)
						error(USER,
					"scattering source list overflow");
					p.slights[++p.slights[0]] = myslist[j];
				}
	} else {				/* leaving ray */
		re = colval(r->cext,RED) - colval(mext,RED);
		ge = colval(r->cext,GRN) - colval(mext,GRN);
		be = colval(r->cext,BLU) - colval(mext,BLU);
		setcolor(p.cext, re<0. ? 0. : re,
				ge<0. ? 0. : ge,
				be<0. ? 0. : be);
		if (m->oargs.nfargs > 3)
			p.albedo = salbedo;
		if (m->oargs.nfargs > 4)
			p.gecc = seccg;
		if (myslist != NULL) {			/* delete from list */
			for (j = myslist[0]; j > 0; j--)
				if (i = inslist(p.slights, myslist[j]))
					p.slights[i] = -1;
			for (i = 0, j = 1; j <= p.slights[0]; j++)
				if (p.slights[j] != -1)
					p.slights[++i] = p.slights[j];
			p.slights[0] = i;
		}
	}
	rayvalue(&p);				/* calls rayparticipate() */
	copycolor(r->rcol, p.rcol);		/* return value */
	r->rt = r->rot + p.rt;
	return(1);
memerr:
	error(SYSTEM, "out of memory in m_mist");
}
